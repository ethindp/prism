// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <dlfcn.h>
#include <memory>

// A library a backend opens itself, rather than linking, so that its absence
// costs that one backend instead of every backend in prism.
class SharedLibrary {
public:
  explicit SharedLibrary(const char *name) noexcept
      : handle(dlopen(name, RTLD_NOW | RTLD_LOCAL)) {}

  explicit operator bool() const noexcept { return handle != nullptr; }

  // The message belongs to the calling thread, so the thread safety the check
  // asks about is not in question here.
  [[nodiscard]] static const char *last_error() noexcept {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char *error = dlerror();
    return error != nullptr ? error : "unknown error";
  }

  // Binds one function, typed by the declaration the library's own header
  // gives it, so a changed signature fails to compile.
  template <class Function>
  [[nodiscard]] bool bind(Function &function, const char *name) const noexcept {
    function = reinterpret_cast<Function>(dlsym(handle.get(), name));
    return function != nullptr;
  }

private:
  struct Close {
    void operator()(void *library) const noexcept { dlclose(library); }
  };

  std::unique_ptr<void, Close> handle;
};
