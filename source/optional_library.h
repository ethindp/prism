// SPDX-License-Identifier: MPL-2.0

#pragma once
#ifdef _WIN32
#include "shared_library.h"

struct InstallLocation {
  const TCHAR *subkey;
  const TCHAR *value;
};

SharedLibrary open_optional_library(const TCHAR *dll,
                                    const InstallLocation *install);
#endif
