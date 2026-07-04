// SPDX-License-Identifier: MPL-2.0

#include "frozen_registry.h"
#include <algorithm>
#include <ranges>
#include <simdutf/simdutf.h>
#include <unordered_set>

FrozenRegistry::FrozenRegistry(std::vector<Registration> registrations)
    : refcount(1) {
    logger.info("Initializing");
    logger.debug("Constructed with {} registrations", registrations.size());
  std::unordered_set<std::uint64_t> seen;
  entries.reserve(registrations.size());
  for (auto &reg : registrations) {
    if (!seen.insert(static_cast<std::uint64_t>(reg.id)).second) {
    logger.warn("Registration {} is already present; skipping", reg.id);
      continue;
      }
    entries.push_back(Entry{.reg = std::move(reg), .cached = {}});
  }
  logger.debug("Stable-sorting entries on priority order");
  std::ranges::stable_sort(entries, std::ranges::greater{},
                           [](const Entry &e) { return e.reg.priority; });
                           logger.info("Initialization complete");
}

FrozenRegistry *
FrozenRegistry::create(std::vector<Registration> registrations) {
  return new (std::nothrow) FrozenRegistry(std::move(registrations));
}

FrozenRegistry *FrozenRegistry::global() {
  static FrozenRegistry *g =
      FrozenRegistry::create(BackendCatalog::instance().snapshot());
      if (g == nullptr) {
      logger.error("Frozen registry memory allocation failure!");
      }
  return g;
}

void FrozenRegistry::retain() noexcept {
  refcount.fetch_add(1, std::memory_order_acq_rel);
}

void FrozenRegistry::release() noexcept {
  if (refcount.fetch_sub(1, std::memory_order_acq_rel) == 1)
    delete this;
}

FrozenRegistry::Entry *FrozenRegistry::find(BackendId id) noexcept {
  for (auto &e : entries)
    if (e.reg.id == id)
      return &e;
  return nullptr;
}

FrozenRegistry::Entry *FrozenRegistry::find(std::string_view name) noexcept {
  for (auto &e : entries)
    if (e.reg.name == name)
      return &e;
  return nullptr;
}

std::size_t FrozenRegistry::count() const noexcept { return entries.size(); }

bool FrozenRegistry::has(BackendId id) const noexcept {
  return std::ranges::any_of(entries,
                             [id](const Entry &e) { return e.reg.id == id; });
}

bool FrozenRegistry::has(std::string_view name) const noexcept {
  return std::ranges::any_of(
      entries, [name](const Entry &e) { return e.reg.name == name; });
}

std::string_view FrozenRegistry::name(BackendId id) const noexcept {
  for (const auto &e : entries)
    if (e.reg.id == id)
      return e.reg.name;
  return {};
}

BackendId FrozenRegistry::id(std::string_view name) const noexcept {
  for (const auto &e : entries)
    if (e.reg.name == name)
      return e.reg.id;
  return BackendId{0};
}

BackendId FrozenRegistry::id_at(std::size_t index) const noexcept {
  return index < entries.size() ? entries[index].reg.id : BackendId{0};
}

std::shared_ptr<TextToSpeechBackend>
FrozenRegistry::create_at(std::size_t index) {
  if (index >= entries.size())
    return nullptr;
  auto &e = entries[index];
  return e.reg.factory ? e.reg.factory() : nullptr;
}

const char *FrozenRegistry::name_at(std::size_t index) const noexcept {
  return index < entries.size() ? entries[index].reg.name.c_str() : nullptr;
}

int FrozenRegistry::priority(BackendId id) const noexcept {
  for (const auto &e : entries)
    if (e.reg.id == id)
      return e.reg.priority;
  return -1;
}

std::vector<BackendId> FrozenRegistry::list() const {
  std::vector<BackendId> result;
  result.reserve(entries.size());
  for (const auto &e : entries)
    result.push_back(e.reg.id);
  return result;
}

std::shared_ptr<TextToSpeechBackend> FrozenRegistry::get(BackendId id) {
  std::shared_lock lock(cache_mutex);
  if (auto *e = find(id))
    return e->cached.lock();
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend>
FrozenRegistry::get(std::string_view name) {
  std::shared_lock lock(cache_mutex);
  if (auto *e = find(name))
    return e->cached.lock();
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend> FrozenRegistry::create(BackendId id) {
  auto *e = find(id);
  return e != nullptr && e->reg.factory != nullptr ? e->reg.factory() : nullptr;
}

std::shared_ptr<TextToSpeechBackend>
FrozenRegistry::create(std::string_view name) {
  auto *e = find(name);
  return e != nullptr && e->reg.factory != nullptr ? e->reg.factory() : nullptr;
}

std::shared_ptr<TextToSpeechBackend> FrozenRegistry::create_best() {
  for (const auto &e : entries) {
    if (!e.reg.factory)
      continue;
    if (auto b = e.reg.factory(); b && b->initialize())
      return b;
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend> FrozenRegistry::acquire_entry(Entry *e) {
  if (e == nullptr)
    return nullptr;
  {
    std::shared_lock lock(cache_mutex);
    if (auto cached = e->cached.lock(); cached != nullptr)
      return cached;
  }
  if (!e->reg.factory)
    return nullptr;
  auto backend = e->reg.factory();
  if (backend == nullptr)
    return nullptr;
  std::unique_lock lock(cache_mutex);
  if (auto cached = e->cached.lock(); cached != nullptr)
    return cached;
  e->cached = backend;
  return backend;
}

std::shared_ptr<TextToSpeechBackend> FrozenRegistry::acquire(BackendId id) {
  return acquire_entry(find(id));
}

std::shared_ptr<TextToSpeechBackend>
FrozenRegistry::acquire(std::string_view name) {
  return acquire_entry(find(name));
}

std::shared_ptr<TextToSpeechBackend> FrozenRegistry::acquire_best() {
  {
    std::shared_lock lock(cache_mutex);
    for (const auto &e : entries)
      if (auto cached = e.cached.lock(); cached != nullptr)
        return cached;
  }
  for (auto &e : entries) {
    if (!e.reg.factory)
      continue;
    auto backend = e.reg.factory();
    if (backend == nullptr || !backend->initialize())
      continue;
    std::unique_lock lock(cache_mutex);
    if (auto cached = e.cached.lock(); cached != nullptr)
      return cached;
    e.cached = backend;
    return backend;
  }
  return nullptr;
}

void FrozenRegistry::clear_cache() {
  std::unique_lock lock(cache_mutex);
  for (auto &e : entries)
    e.cached.reset();
}

RegistryBuilder::RegistryBuilder()
    : registrations(BackendCatalog::instance().snapshot()) {}

BuilderResult RegistryBuilder::add(std::string name, int priority,
                                   BackendFactory factory, BackendId *out_id) {
  if (is_spent)
    return BuilderResult::Spent;
  if (name.empty())
    return BuilderResult::EmptyName;
  if (!simdutf::validate_utf8(name.data(), name.size()))
    return BuilderResult::InvalidUtf8;
  if (priority < 0)
    return BuilderResult::NegativePriority;
  const auto new_id = make_backend_id(name);
  if (new_id == BackendId{0})                         // <-- add (#4)
    return BuilderResult::ReservedId;
  for (const auto &r : registrations) {
    if (r.name == name)
      return BuilderResult::DuplicateName;
    if (r.id == new_id)
      return BuilderResult::DuplicateId;
  }
  registrations.push_back(Registration{.id = new_id,
                                       .name = std::move(name),
                                       .priority = priority,
                                       .factory = std::move(factory)});
  if (out_id != nullptr)
    *out_id = new_id;
  return BuilderResult::Ok;
}

FrozenRegistry *RegistryBuilder::freeze() {
  if (is_spent)
    return nullptr;
  is_spent = true;
  return FrozenRegistry::create(std::move(registrations));
}
