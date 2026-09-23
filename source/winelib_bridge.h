// SPDX-License-Identifier: MPL-2.0

#pragma once
#ifdef _WIN32
#include "backend.h"
#include <cstdint>
#include <raw/prism_winelib_status.h>
#include <string>
#include <tchar.h>
#include <utility>
#include <windows.h>

inline bool running_under_wine() noexcept {
  auto *const ntdll = GetModuleHandle(_T("ntdll.dll"));
  return ntdll != nullptr &&
         GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

inline BackendResult<> winelib_result(PrismWinelibStatus status) noexcept {
  switch (status) {
  case PRISM_WINELIB_OK:
    return {};
  case PRISM_WINELIB_INVALID_ARGUMENT:
    return std::unexpected(BackendError::InvalidParam);
  case PRISM_WINELIB_NOT_AVAILABLE:
    return std::unexpected(BackendError::BackendNotAvailable);
  case PRISM_WINELIB_SPEAK_FAILURE:
    return std::unexpected(BackendError::SpeakFailure);
  case PRISM_WINELIB_RANGE:
    return std::unexpected(BackendError::RangeOutOfBounds);
  case PRISM_WINELIB_NO_VOICES:
    return std::unexpected(BackendError::NoVoices);
  case PRISM_WINELIB_NOT_SPEAKING:
    return std::unexpected(BackendError::NotSpeaking);
  case PRISM_WINELIB_NOT_PAUSED:
    return std::unexpected(BackendError::NotPaused);
  case PRISM_WINELIB_ALREADY_PAUSED:
    return std::unexpected(BackendError::AlreadyPaused);
  case PRISM_WINELIB_UNDEFINED_STATE:
    return std::unexpected(BackendError::BackendEnteredUndefinedState);
  case PRISM_WINELIB_MEMORY:
    return std::unexpected(BackendError::MemoryFailure);
  case PRISM_WINELIB_INTERNAL:
  case PRISM_WINELIB_BUFFER_TOO_SMALL:
    return std::unexpected(BackendError::InternalBackendError);
  default:
    return std::unexpected(BackendError::Unknown);
  }
}

template <typename Handle>
BackendResult<std::string>
winelib_fetch_string(PrismWinelibStatus (*fn)(Handle *, std::uint32_t, char *,
                                              std::uint32_t, std::uint32_t *),
                     Handle *h, std::size_t id) {
  if (!std::in_range<std::uint32_t>(id))
    return std::unexpected(BackendError::RangeOutOfBounds);
  std::string out(64, '\0');
  for (int attempt = 0; attempt < 4; ++attempt) {
    std::uint32_t needed = 0;
    const auto status = fn(h, static_cast<std::uint32_t>(id), out.data(),
                           static_cast<std::uint32_t>(out.size() + 1), &needed);
    if (status != PRISM_WINELIB_BUFFER_TOO_SMALL) {
      if (const auto r = winelib_result(status); !r)
        return std::unexpected(r.error());
      out.resize(needed);
      return out;
    }
    out.resize(needed);
  }
  return std::unexpected(BackendError::InternalBackendError);
}
#endif
