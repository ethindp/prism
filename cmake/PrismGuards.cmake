# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)

function(prism_require_targets)
  foreach(_t IN LISTS ARGN)
    if(NOT TARGET ${_t})
      message(
        FATAL_ERROR
          "${CMAKE_CURRENT_LIST_FILE} requires target '${_t}', which does not exist.\n"
      )
    endif()
  endforeach()
endfunction()

function(prism_require_vars)
  foreach(_v IN LISTS ARGN)
    if(NOT DEFINED ${_v})
      message(
        FATAL_ERROR
          "${CMAKE_CURRENT_LIST_FILE} requires variable '${_v}', which is not defined.\n"
      )
    endif()
  endforeach()
endfunction()

function(prism_require_enum var)
  if(NOT "${${var}}" IN_LIST ARGN)
    string(REPLACE ";" ", " _allowed "${ARGN}")
    message(
      FATAL_ERROR "${var}='${${var}}' is invalid. Must be one of: ${_allowed}")
  endif()
endfunction()
