# SPDX-License-Identifier: MPL-2.0

# Reads the Semantic Versioning 2.0.0 version in VERSION. project() takes only
# the numeric core, so the prerelease and build parts are kept separately.
include_guard(GLOBAL)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../VERSION" PRISM_VERSION_FULL
     LIMIT_COUNT 1)
string(STRIP "${PRISM_VERSION_FULL}" PRISM_VERSION_FULL)
set(_numeric "(0|[1-9][0-9]*)")
if(NOT PRISM_VERSION_FULL MATCHES
   "^${_numeric}\\.${_numeric}\\.${_numeric}(-([^+]+))?(\\+(.+))?$")
  message(
    FATAL_ERROR
      "VERSION holds '${PRISM_VERSION_FULL}', which is not a Semantic Versioning 2.0.0 version."
  )
endif()
set(PRISM_VERSION_CORE "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
set(PRISM_VERSION_PRERELEASE "${CMAKE_MATCH_5}")
set(PRISM_VERSION_BUILD "${CMAKE_MATCH_7}")
set(_identifiers "^[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*$")
if(NOT PRISM_VERSION_PRERELEASE STREQUAL ""
   AND (NOT PRISM_VERSION_PRERELEASE MATCHES "${_identifiers}"
        OR PRISM_VERSION_PRERELEASE MATCHES "(^|\\.)0[0-9]+(\\.|$)"))
  message(
    FATAL_ERROR
      "The prerelease '${PRISM_VERSION_PRERELEASE}' in VERSION is not valid. Its identifiers must be non-empty, use only ASCII letters, digits and hyphens, and have no leading zeros if numeric."
  )
endif()
if(NOT PRISM_VERSION_BUILD STREQUAL "" AND NOT PRISM_VERSION_BUILD MATCHES
                                           "${_identifiers}")
  message(
    FATAL_ERROR
      "The build metadata '${PRISM_VERSION_BUILD}' in VERSION is not valid. Its identifiers must be non-empty and use only ASCII letters, digits and hyphens."
  )
endif()
unset(_numeric)
unset(_identifiers)
