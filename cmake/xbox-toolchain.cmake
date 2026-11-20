# SPDX-License-Identifier: MPL-2.0 Builds for an Xbox console in the games API
# partition, as the console and vcpkg's Xbox triplets do, using only the public
# GDK.
set(XBOX_CONSOLE_TARGET
    "scarlett"
    CACHE STRING "Xbox console to build for (xboxone or scarlett)")
set_property(CACHE XBOX_CONSOLE_TARGET PROPERTY STRINGS xboxone scarlett)
if(NOT XBOX_CONSOLE_TARGET MATCHES "^(xboxone|scarlett)$")
  message(
    FATAL_ERROR
      "XBOX_CONSOLE_TARGET must be xboxone or scarlett, not '${XBOX_CONSOLE_TARGET}'"
  )
endif()
option(PRISM_XBOX_GXDK_MACROS
       "Define the _GAMING_XBOX macros that the GDK's Xbox extensions define"
       OFF)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_CROSSCOMPILING
    ON
    CACHE BOOL "")
add_compile_definitions(
  WIN32
  _WINDOWS
  UNICODE
  _UNICODE
  WINAPI_FAMILY=WINAPI_FAMILY_GAMES
  _WIN32_WINNT=0x0A00
  _ATL_NO_DEFAULT_LIBS
  __WRL_NO_DEFAULT_LIB__
  __WRL_CLASSIC_COM_STRICT__
  _UITHREADCTXT_SUPPORT=0
  _CRT_USE_WINAPI_PARTITION_APP)
if(PRISM_XBOX_GXDK_MACROS)
  string(TOUPPER "${XBOX_CONSOLE_TARGET}" _prism_xbox_console)
  add_compile_definitions(_GAMING_XBOX _GAMING_XBOX_${_prism_xbox_console})
  unset(_prism_xbox_console)
endif()
if(XBOX_CONSOLE_TARGET STREQUAL "scarlett")
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/favor:AMD64;/arch:AVX2>"
                      "$<$<C_COMPILER_ID:Clang>:-mavx2;-mfma>")
else()
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/favor:AMD64;/arch:AVX>"
                      "$<$<C_COMPILER_ID:Clang>:-mavx>")
endif()
set(CMAKE_C_STANDARD_LIBRARIES
    ""
    CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES
    ""
    CACHE STRING "" FORCE)
add_link_options("LINKER:/DEFAULTLIB:onecore_apiset.lib")
foreach(
  _prism_xbox_lib IN
  ITEMS kernel32
        advapi32
        comctl32
        comsupp
        dbghelp
        gdi32
        gdiplus
        guardcfw
        mmc
        msimg32
        msvcole
        msvcoled
        mswsock
        ntstrsafe
        ole2
        ole2autd
        ole2auto
        ole2d
        ole2ui
        ole2uid
        ole32
        oleacc
        oleaut32
        oledlg
        oledlgd
        runtimeobject
        shell32
        shlwapi
        strsafe
        urlmon
        user32
        userenv
        wlmole
        wlmoled
        onecore)
  add_link_options("LINKER:/NODEFAULTLIB:${_prism_xbox_lib}.lib")
endforeach()
unset(_prism_xbox_lib)
