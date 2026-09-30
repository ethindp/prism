// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <memory>
#ifdef _WIN32
#include <type_traits>
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// A library a backend opens itself, rather than linking, so that its absence
// costs that one backend instead of every backend in prism.
class SharedLibrary {
public:
#ifdef _WIN32
  explicit SharedLibrary(const TCHAR *path) noexcept
      : handle(LoadLibrary(path)) {}
#else
  explicit SharedLibrary(const char *name) noexcept
      : handle(dlopen(name, RTLD_NOW | RTLD_LOCAL)) {}
#endif

  explicit operator bool() const noexcept { return handle != nullptr; }

#ifdef _WIN32
  [[nodiscard]] static DWORD last_error() noexcept { return GetLastError(); }
#else
  // The message belongs to the calling thread, so the thread safety the check
  // asks about is not in question here.
  [[nodiscard]] static const char *last_error() noexcept {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char *error = dlerror();
    return error != nullptr ? error : "unknown error";
  }
#endif

  // Binds one function, typed by the declaration the library's own header
  // gives it, so a changed signature fails to compile.
  template <class Function>
  [[nodiscard]] bool bind(Function &function, const char *name) const noexcept {
#ifdef _WIN32
    // NOLINTNEXTLINE(bugprone-casting-through-void)
    function = reinterpret_cast<Function>(
        reinterpret_cast<void *>(GetProcAddress(handle.get(), name)));
#else
    function = reinterpret_cast<Function>(dlsym(handle.get(), name));
#endif
    return function != nullptr;
  }

private:
#ifdef _WIN32
  struct Close {
    void operator()(HMODULE library) const noexcept { FreeLibrary(library); }
  };

  std::unique_ptr<std::remove_pointer_t<HMODULE>, Close> handle;
#else
  struct Close {
    void operator()(void *library) const noexcept { dlclose(library); }
  };

  std::unique_ptr<void, Close> handle;
#endif
};
