// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "optional_library.h"
#include "logging.h"
#include <filesystem>
#include <optional>
#include <string>
#include <windows.h>

namespace {
std::optional<std::filesystem::path> prism_folder() {
  static const int anchor = 0;
  HMODULE module = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&anchor), &module) == 0)
    return std::nullopt;
  std::wstring path(MAX_PATH, L'\0');
  while (true) {
    const DWORD length = GetModuleFileNameW(module, path.data(),
                                            static_cast<DWORD>(path.size()));
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
  if (RegGetValueW(HKEY_LOCAL_MACHINE, install.subkey, install.value,
                   RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS)
    return std::nullopt;
  std::wstring folder(size / sizeof(wchar_t), L'\0');
  if (RegGetValueW(HKEY_LOCAL_MACHINE, install.subkey, install.value,
                   RRF_RT_REG_SZ, nullptr, folder.data(),
                   &size) != ERROR_SUCCESS)
    return std::nullopt;
  folder.resize(folder.find(L'\0'));
  if (folder.empty())
    return std::nullopt;
  return std::filesystem::path(folder);
}
} // namespace

SharedLibrary open_optional_library(const wchar_t *dll,
                                    const InstallLocation *install) {
  static const LogSource log{"prism/optional_library"};
  SharedLibrary library{dll};
  if (library)
    return library;
  log.trace(L"{} is not on the DLL search path (LastError={})", dll,
            SharedLibrary::last_error());
  if (const auto folder = prism_folder()) {
    const auto path = *folder / dll;
    library = SharedLibrary{path.c_str()};
    if (library)
      return library;
    log.trace(L"{} is not in {} (LastError={})", dll, folder->wstring(),
              SharedLibrary::last_error());
  }
  if (install != nullptr) {
    if (const auto folder = install_folder(*install)) {
      const auto path = *folder / dll;
      library = SharedLibrary{path.c_str()};
      if (library)
        return library;
      log.trace(L"{} is not in {} (LastError={})", dll, folder->wstring(),
                SharedLibrary::last_error());
    } else {
      log.trace(L"no install folder for {} in the registry", dll);
    }
  }
  return library;
}
#endif
