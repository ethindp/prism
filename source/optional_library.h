// SPDX-License-Identifier: MPL-2.0

#pragma once
#ifdef _WIN32
#include "shared_library.h"

struct InstallLocation {
  const wchar_t *subkey;
  const wchar_t *value;
};

SharedLibrary open_optional_library(const wchar_t *dll,
                                    const InstallLocation *install);
#endif
