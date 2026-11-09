# Application Deployment

This chapter describes what is required for an application using Prism to function on systems other than the one on which it was built. It distinguishes between three kinds of dependency:

* Build dependencies are required only when Prism itself is compiled from source. Applications using a release package or a binding's package do not require them.
* Load dependencies MUST be present on the end user's system, or the Prism library cannot be loaded at all.
* Backend dependencies are required only for a particular backend to function. If one is absent, that backend is unavailable and every other backend continues to function.

The screen readers and speech engines themselves are backend dependencies. Applications do not distribute them, and SHOULD NOT instruct users to replace their screen reader in order to use an application. An application that handles the absence of a backend as described in the Getting Started chapter functions with whichever backends the user's system provides.

## Licensing

Prism is licensed under the Mozilla Public License, version 2.0, and incorporates code from other projects under their own licenses. The files `NOTICE` and `LICENSES` are included in every release package. A distributor is responsible for complying with these licenses, and SHOULD include those files with the application.

## Windows

Prism requires Windows 10 or later. The shared form of the library, `prism.dll`, MUST be deployed with the application, normally in the directory containing its executable. The static form requires no additional files. Neither form of the release package requires the Visual C++ Redistributable; the Godot add-on, whose C runtime is required to match that of `godot-cpp`, does.

The PC-Talker, ZDSR, and Boy PC Reader backends load a library at run time (`PCTKUSR.dll`, `ZDSRAPI.dll` or `ZDSRAPI_x64.dll`, and `byctrl.dll` or `byctrl-x64.dll` respectively), which is installed with the screen reader and which the application does not distribute. The GDK Speech backend loads `xgameruntime.thunks.dll` from the Microsoft GDK. Prism does not distribute this library; an application that uses the backend MUST deploy it, and its redistribution is governed by the GDK license.

An application that runs under Wine or Proton and wishes to use the speech services of the host system MUST also deploy the Wine bridges, as described in the Wine Bridges chapter.

## Linux

### Load dependencies

The release packages and the Godot add-on for Linux require glibc 2.34 or later and `libstdc++.so.6` and `libgcc_s.so.1` from GCC 12 or later. They also link `libdbus-1.so.3`, for the Orca backend and power management, and `libglib-2.0.so.0`, `libgobject-2.0.so.0`, and `libgio-2.0.so.0`, for the Spiel backend. The names of the packages which provide these libraries vary between distributions.

These libraries are present on nearly all desktop Linux systems, but they MAY be absent from a minimal container image. The Python wheels bundle their dependencies other than glibc and `libstdc++`, and consequently have no dependency on D-Bus or GLib being installed. The `manylinux` wheels require glibc 2.34 or later, and the `musllinux` wheels require musl 1.2 or later.

The release packages and the Godot add-on require glibc, and do not function on distributions based on musl, such as Alpine Linux. Applications that require this configuration SHOULD use the `musllinux` wheels or build Prism from source.

### Backend dependencies

The Orca backend requires a session D-Bus connection and a running Orca 49 or later, the first release to implement Orca's remote-control interface. The Speech Dispatcher backend requires `libspeechd.so.2` from Speech Dispatcher 0.11.1 or later and a reachable Speech Dispatcher service. The Spiel backend requires `libspiel-1.0.so.1`, a session D-Bus connection, and at least one speech provider. The Clipboard backend requires `libwayland-client.so.0` and a compositor implementing a data control protocol, or `libxcb.so.1` and an X11 server or Xwayland, as described in the chapter on backend-specific notes.

None of these are required for Prism to load, and Prism does not require that any particular one be present. A system on which only Speech Dispatcher is available, for instance because its distribution packages an older release of Orca, is a fully supported configuration. Applications therefore SHOULD NOT state that any of these backends is required, unless the application has selected a specific backend deliberately.

Spiel is not widely packaged at present. Application authors SHOULD NOT require their users to build it from source.

### Diagnosing a failure to load

| Message | Meaning |
| --- | --- |
| `libdbus-1.so.3: cannot open shared object file` | A load dependency is absent. The corresponding package MUST be installed. The same applies to the other libraries listed under load dependencies. |
| `version 'GLIBC_2.xx' not found` | The system's glibc is older than the build requires. The system is not supported by the release packages, and Prism MUST be built from source on a system at least as old as the oldest intended target. |
| `version 'GLIBCXX_3.4.xx' not found` or `version 'CXXABI_1.3.xx' not found` | The system's C++ runtime is older than the build requires. The remedy is the same as for glibc. |

A build of Prism inherits its requirements from the system on which it is compiled. A build produced on a recent distribution MAY therefore fail to load on an older one, even when the release packages function on it.

## Apple platforms

Prism has no load dependencies beyond system frameworks on Apple platforms. Both backends are subject to the requirements on the host process described in the chapter on backend-specific notes; in particular, a VoiceOver backend on macOS requires that the application own an `NSWindow` and run a main run loop, which a console program does not.

An application on macOS that wishes to use the legacy VoiceOver automation path MUST declare `NSAppleEventsUsageDescription` in its `Info.plist`. A notarized application MUST additionally hold the `com.apple.security.automation.apple-events` entitlement, and the path is not available to a sandboxed application. An application that does not meet these conditions uses the accessibility-announcement path, and no action is necessary.

## Android

Prism for Android requires Android 8 (API level 26) or later. It SHOULD be consumed in a manner that preserves the library's merged manifest, as the application context is otherwise not initialized automatically; the chapter on backend-specific notes describes this further.

## Web

The WebAssembly build requires a browser that implements the Web Speech API. The first request for speech MUST originate from a user-initiated event in browsing contexts that enforce an autoplay policy.

Prism is built with thread support, so a program that links it MUST be compiled and linked with `-pthread`, and, when the Web Speech backend is present, linked with `-lembind`. The CMake package supplies these flags to targets that link `prism::prism`. Because threads in WebAssembly require `SharedArrayBuffer`, the page MUST be served cross-origin isolated, with the headers `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`.

## Godot

The `addons/prismatoid` directory MUST be included in the exported project. The add-on contains binaries for every supported platform and architecture, and an application MAY remove those for platforms it does not target. The load and backend dependencies of the platform apply as described in this chapter.

## Python

An application SHOULD declare `prismatoid` as a dependency, which also installs `cffi`. No further action is necessary on platforms for which wheels are published. Where an application is packaged with a tool that collects its dependencies, the contents of the `prism/_native` directory and, on Linux, the `prismatoid.libs` directory MUST be preserved.

## Building from source

A source build requires a C++23 compiler, of which GCC 12 is the oldest version of GCC known to function, and CMake 3.24 or later. On Linux, the development files are located through `pkg-config`: `dbus-1` for the Orca backend and power management, `speech-dispatcher` for the Speech Dispatcher backend, `gio-2.0` and `spiel-1.0` for the Spiel backend, and `xcb` and `wayland-client` 1.20 or later for the Clipboard backend. Only the headers of `speech-dispatcher`, `spiel-1.0`, `xcb`, and `wayland-client` are used, since those libraries are loaded at run time.

Under the default setting of `PRISM_BACKEND_DEFAULT`, a backend whose development files are absent is omitted from the build. The build does not fail, and the resulting library lacks the backend. The outcome is reported in the `Prism backends:` line of the CMake output, which distributors SHOULD inspect. A distributor that expects a particular backend to be present SHOULD set its option, such as `PRISM_ENABLE_ORCA_BACKEND`, to `ON`, in which case the absence of its development files causes the configuration to fail.

## Verifying a deployment

Distributors SHOULD test an application on a clean installation of each operating system they intend to support, since a development system will generally have libraries installed that an end user's does not.

Setting the `PRISM_LOG` environment variable to `trace`, `debug`, `info`, `warn`, or `error` causes Prism to write log messages of that level and above to standard error. The messages include the reason a backend could not be loaded or initialized, and are the first thing to consult when a backend is unexpectedly unavailable. Applications MAY instead install a handler, as described in the chapter on logging.
