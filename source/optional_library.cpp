// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "optional_library.h"
#include "logging.h"
#include <filesystem>
#include <optional>
#include <string>
#include <tchar.h>
#include <utility>
#include <windows.h>

namespace {
std::optional<std::filesystem::path> prism_folder() {
  static constexpr int anchor = 0;
  HMODULE module = nullptr;
  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        reinterpret_cast<LPCTSTR>(&anchor), &module) == 0)
    return std::nullopt;
  std::basic_string<TCHAR> path(MAX_PATH, _T('\0'));
  while (true) {
    const DWORD length =
        GetModuleFileName(module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0)
      return std::nullopt;
    if (length < path.size()) {
      path.resize(length);
      return std::filesystem::path(path).parent_path();
    }
    path.resize(path.size() * 2);
  }
}

std::optional<std::filesystem::path>
install_folder(const InstallLocation &install) {
  DWORD size = 0;
  if (RegGetValue(HKEY_LOCAL_MACHINE, install.subkey, install.value,
                  RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS)
    return std::nullopt;
  std::basic_string<TCHAR> folder(size / sizeof(TCHAR), _T('\0'));
  if (RegGetValue(HKEY_LOCAL_MACHINE, install.subkey, install.value,
                  RRF_RT_REG_SZ, nullptr, folder.data(),
                  &size) != ERROR_SUCCESS)
    return std::nullopt;
  folder.resize(folder.find(_T('\0')));
  if (folder.empty())
    return std::nullopt;
  return std::filesystem::path(folder);
}

std::optional<SharedLibrary> search_explicitly(const TCHAR *dll,
                                               const InstallLocation *install) {
  static const LogSource log{"prism/optional_library"};
  if (const auto folder = prism_folder()) {
    const auto path = *folder / dll;
    SharedLibrary library{path.string<TCHAR>().c_str()};
    if (library)
      return library;
    log.trace(_T("{} is not in {} (LastError={})"), dll,
              folder->string<TCHAR>(), SharedLibrary::last_error());
  }
  if (install != nullptr) {
    if (const auto folder = install_folder(*install)) {
      const auto path = *folder / dll;
      SharedLibrary library{path.string<TCHAR>().c_str()};
      if (library)
        return library;
      log.trace(_T("{} is not in {} (LastError={})"), dll,
                folder->string<TCHAR>(), SharedLibrary::last_error());
    } else {
      log.trace(_T("no install folder for {} in the registry"), dll);
    }
  }
  return std::nullopt;
}
} // namespace

SharedLibrary open_optional_library(const TCHAR *dll,
                                    const InstallLocation *install) noexcept {
  // Prefer an app-local copy, then the explicitly registered product copy.
  // Only fall back to Windows' ambient DLL search path after those locations,
  // so a stale same-named DLL in the working directory or PATH cannot shadow
  // the installed library.
  // NOLINTBEGIN(bugprone-empty-catch)
  try {
    if (auto found = search_explicitly(dll, install))
      return std::move(*found);
  } catch (...) {
  }
  // NOLINTEND(bugprone-empty-catch)

  SharedLibrary library{dll};
  if (!library) {
    try {
      static const LogSource log{"prism/optional_library"};
      log.trace(_T("{} is not on the DLL search path (LastError={})"), dll,
                SharedLibrary::last_error());
    } catch (...) {
      return library;
    }
  }
  return library;
}
#endif
