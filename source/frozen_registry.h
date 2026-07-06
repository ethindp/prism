// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "backend_catalog.h"
#include "logging.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <vector>

class FrozenRegistry {
public:
  [[nodiscard]] static FrozenRegistry *
  create(std::vector<Registration> registrations);
  [[nodiscard]] static FrozenRegistry *global();
  void retain() noexcept;
  void release() noexcept;
  [[nodiscard]] std::size_t count() const noexcept;
  [[nodiscard]] bool has(BackendId id) const noexcept;
  [[nodiscard]] bool has(std::string_view name) const noexcept;
  [[nodiscard]] std::string_view name(BackendId id) const noexcept;
  [[nodiscard]] BackendId id(std::string_view name) const noexcept;
  [[nodiscard]] BackendId id_at(std::size_t index) const noexcept;
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend>
  create_at(std::size_t index);
  [[nodiscard]] const char *name_at(std::size_t index) const noexcept;
  [[nodiscard]] int priority(BackendId id) const noexcept;
  [[nodiscard]] std::vector<BackendId> list() const;
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> get(BackendId id);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> get(std::string_view name);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> create(BackendId id);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend>
  create(std::string_view name);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> create_best();
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> acquire(BackendId id);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend>
  acquire(std::string_view name);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> acquire_best();
  void clear_cache();

private:
  struct Entry {
    Registration reg;
    std::weak_ptr<TextToSpeechBackend> cached;
  };
  explicit FrozenRegistry(std::vector<Registration> registrations);
  ~FrozenRegistry() = default;
  FrozenRegistry(const FrozenRegistry &) = delete;
  FrozenRegistry &operator=(const FrozenRegistry &) = delete;
  [[nodiscard]] Entry *find(BackendId id) noexcept;
  [[nodiscard]] Entry *find(std::string_view name) noexcept;
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> acquire_entry(Entry *e);
  std::atomic_unsigned_lock_free refcount;
  std::vector<Entry> entries;
  mutable std::shared_mutex cache_mutex;
};

enum class BuilderResult {
  Ok,
  Spent,
  EmptyName,
  InvalidUtf8,
  NegativePriority,
  ReservedId,
  DuplicateName,
  DuplicateId,
};

class RegistryBuilder {
public:
  RegistryBuilder(); // seeded with the compiled-in backends from the catalog
  BuilderResult add(std::string name, int priority, BackendFactory factory,
                    BackendId *out_id);
  [[nodiscard]] FrozenRegistry *freeze();
  [[nodiscard]] bool spent() const noexcept { return is_spent; }

private:
  bool is_spent = false;
  std::vector<Registration> registrations;
};
