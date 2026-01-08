// SPDX-License-Identifier: MPL-2.0

#include "backend_registry.h"
#include <algorithm>

BackendRegistry &BackendRegistry::instance() {
  static BackendRegistry registry;
  return registry;
}

void BackendRegistry::register_backend(BackendId id, std::string_view name,
                                       int priority, Factory factory) {
  std::unique_lock lock(mutex);
  Entry entry{id, name, priority, std::move(factory), {}};
  auto pos =
      std::lower_bound(entries.begin(), entries.end(), priority,
                       [](const Entry &e, int p) { return e.priority > p; });
  entries.insert(pos, std::move(entry));
}

bool BackendRegistry::has(BackendId id) const {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (e.id == id)
      return true;
  }
  return false;
}

bool BackendRegistry::has(std::string_view name) const {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (e.name == name)
      return true;
  }
  return false;
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
      if (e.cached.expired())
        return nullptr;
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
      if (e.cached.expired())
        return nullptr;
      return e.cached.lock();
    }
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend> BackendRegistry::create(BackendId id) {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (e.id == id) {
      auto b = e.factory();
      if (!b) return nullptr;
      #ifdef __ANDROID__
      b->set_jni_env(env);
      #endif
      return b;
      }
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend>
BackendRegistry::create(std::string_view name) {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (e.name == name) {
          auto b = e.factory();
          if (!b) return nullptr;
      #ifdef __ANDROID__
      b->set_jni_env(env);
      #endif
      return b;
}
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend> BackendRegistry::create_best() {
  std::shared_lock lock(mutex);
  for (const auto &e : entries) {
    if (auto backend = e.factory(); backend) {
    #ifdef __ANDROID__
    backend->set_jni_env(env);
    #endif
      if (backend->initialize())
        return backend;
    }
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend> BackendRegistry::acquire(BackendId id) {
  std::unique_lock lock(mutex);
  for (auto &e : entries) {
    if (e.id == id) {
      if (auto existing = e.cached; !existing.expired())
        return existing.lock();
      auto backend = e.factory();
    #ifdef __ANDROID__
    backend->set_jni_env(env);
    #endif
      e.cached = backend;
      return backend;
    }
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend>
BackendRegistry::acquire(std::string_view name) {
  std::unique_lock lock(mutex);
  for (auto &e : entries) {
    if (e.name == name) {
      if (auto existing = e.cached; !existing.expired())
        return existing.lock();
      auto backend = e.factory();
    #ifdef __ANDROID__
    backend->set_jni_env(env);
    #endif
      e.cached = backend;
      return backend;
    }
  }
  return nullptr;
}

std::shared_ptr<TextToSpeechBackend> BackendRegistry::acquire_best() {
  std::unique_lock lock(mutex);
  for (auto &e : entries) {
    if (auto existing = e.cached; !existing.expired())
      return existing.lock();
    if (auto backend = e.factory(); backend) {
    #ifdef __ANDROID__
    backend->set_jni_env(env);
    #endif
      if (backend->initialize()) {
        e.cached = backend;
        return backend;
      }
    }
  }
  return nullptr;
}

void BackendRegistry::clear_cache() {
  std::unique_lock lock(mutex);
  for (auto &e : entries) {
    e.cached.reset();
  }
}

  #ifdef __ANDROID__
  void BackendRegistry::set_jni_env(JNIEnv* env) {
  this->env = env;
  }
  #endif
