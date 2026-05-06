// SPDX-License-Identifier: MPL-2.0

#include "backend_registry.h"
#include <algorithm>
#include <ranges>

BackendRegistry &BackendRegistry::instance() {
  static BackendRegistry registry;
  return registry;
}

void BackendRegistry::register_backend(BackendId id, std::string_view name,
                                       int priority, Factory factory) {
  std::unique_lock lock(mutex);
  if (std::ranges::any_of(entries,
                          [id](const auto &e) { return e.id == id; })) {
    return;
  }
  Entry entry{.id = id,
              .name = name,
              .priority = priority,
              .factory = std::move(factory),
              .cached = {}};
  auto pos = std::ranges::lower_bound(entries, priority, std::ranges::greater{},
                                      &Entry::priority);
  entries.insert(pos, std::move(entry));
}

bool BackendRegistry::has(BackendId id) const {
  std::shared_lock lock(mutex);
  return std::ranges::any_of(entries,
                             [id](const auto &e) { return e.id == id; });
}

bool BackendRegistry::has(std::string_view name) const {
  std::shared_lock lock(mutex);
  return std::ranges::any_of(entries,
                             [name](const auto &e) { return e.name == name; });
}

std::string_view BackendRegistry::name(BackendId id) const {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (e.id == id)
      return e.name;
  }
  return {};
}

BackendId BackendRegistry::id(std::string_view name) const {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (e.name == name)
      return e.id;
  }
  return BackendId{0};
}

int BackendRegistry::priority(BackendId id) const {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (e.id == id)
      return e.priority;
  }
  return -1;
}

std::vector<BackendId> BackendRegistry::list() const {
  std::shared_lock lock(mutex);
  std::vector<BackendId> result;
  result.reserve(entries.size());
  for (const auto &e : entries) {
    result.push_back(e.id);
  }
  return result;
}

std::shared_ptr<TextToSpeechBackend> BackendRegistry::get(BackendId id) {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (e.id == id) {
      return e.cached.lock();
    }
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend>
BackendRegistry::get(std::string_view name) {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (e.name == name) {
      return e.cached.lock();
    }
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend> BackendRegistry::create(BackendId id) {
  return create([id](const Entry &e) { return e.id == id; });
}

std::shared_ptr<TextToSpeechBackend>
BackendRegistry::create(std::string_view name) {
  return create([name](const Entry &e) { return e.name == name; });
}

std::shared_ptr<TextToSpeechBackend> BackendRegistry::create_best() {
  std::vector<Factory> factories;
  {
    std::shared_lock lock(mutex);
    factories.reserve(entries.size());
    for (const auto &e : entries)
      factories.push_back(e.factory);
  }
  for (auto &f : factories) {
    if (auto b = f(); b && b->initialize())
      return b;
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend> BackendRegistry::acquire(BackendId id) {
  return acquire([id](const Entry &e) { return e.id == id; });
}

std::shared_ptr<TextToSpeechBackend>
BackendRegistry::acquire(std::string_view name) {
  return acquire([name](const Entry &e) { return e.name == name; });
}

std::shared_ptr<TextToSpeechBackend> BackendRegistry::acquire_best() {
  {
    std::shared_lock lock(mutex);
    for (const auto &e : entries) {
      if (auto cached = e.cached.lock(); cached != nullptr)
        return cached;
    }
  }
  std::vector<std::tuple<BackendId, Factory>> snapshot;
  {
    std::shared_lock lock(mutex);
    snapshot.reserve(entries.size());
    for (const auto &e : entries) {
      snapshot.emplace_back(e.id, e.factory);
    }
  }
  for (auto &[id, factory] : snapshot) {
    if (factory == nullptr)
      continue;
    auto backend = factory();
    if (backend == nullptr || !backend->initialize())
      continue;
    std::unique_lock lock(mutex);
    auto it = std::ranges::find_if(
        entries, [id = id](const Entry &e) { return e.id == id; });
    if (it == entries.end())
      return backend;
    if (auto cached = it->cached.lock(); cached != nullptr)
      return cached;
    it->cached = backend;
    return backend;
  }
  return nullptr;
}

void BackendRegistry::clear_cache() {
  std::unique_lock lock(mutex);
  for (auto &e : entries) {
    e.cached.reset();
  }
}
