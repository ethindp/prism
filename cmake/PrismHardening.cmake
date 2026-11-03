# SPDX-License-Identifier: MPL-2.0

# Standard library precondition checks. Release builds get the cheap ones, which
# libc++ and the MSVC STL design for production, debug builds the slowest each
# library has, and lint builds something in between. Android release builds take
# libc++'s extensive checks as well. A failed check terminates the process.
#
# _GLIBCXX_DEBUG swaps the standard containers for checked ones with a different
# ABI. That is safe here because Prism's interface is C and no dependency passes
# a standard container across its own.
include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism_common)
if(PRISM_ENABLE_LINTING OR ANDROID)
  set(_prism_libcxx_release _LIBCPP_HARDENING_MODE_EXTENSIVE)
else()
  set(_prism_libcxx_release _LIBCPP_HARDENING_MODE_FAST)
endif()
target_compile_definitions(
  prism_common
  INTERFACE
    _MSVC_STL_HARDENING=1
    "$<IF:$<CONFIG:Debug>,_GLIBCXX_DEBUG,_GLIBCXX_ASSERTIONS>"
    "_LIBCPP_HARDENING_MODE=$<IF:$<CONFIG:Debug>,_LIBCPP_HARDENING_MODE_DEBUG,${_prism_libcxx_release}>"
)
unset(_prism_libcxx_release)
