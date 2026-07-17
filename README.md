# PRISM

Prism is the Platform-agnostic Reader Interface for Speech and Messages. Since that's a hell of a mouthful, we just call it Prism for short. The name comes from prisms in optics, which are transparent optical components with flat surfaces that refract light into many beams. Thus, the metaphor: refract your TTS strings to send them to many different backends, potentially simultaneously.

Prism aims to unify the various screen reader abstraction libraries like SpeechCore, UniversalSpeech, SRAL, Tolk, etc., into a single unified system with a single unified API. Of course, we also support traditional TTS engines. I have tried to develop Prism in such a way that compilation is trivial and requires no external dependencies. To that end, the CMake builder will download all needed dependencies. However, since it uses [cpm.cmake](https://github.com/cpm-cmake/CPM.cmake), vendoring of dependencies is very possible.

## Minimum system requirements

Prism requires the following to be met to function properly. If your system does not meet these minimums, Prism MAY malfunction or fail to load at all. If you attempt to load Prism and the load fails, or if Prism malfunctions, please verify that you meet the requirements in this section before opening an issue.

* For windows, Windows 10 or later is required. Older versions of windows are NOT supported.
* Apple platforms require either MacOS 11, iOS 13, tvOS 13, WatchOS 7, or VisionOS 1.
* Linux requires a version of Glib which conforms to the Glib-2.68 ABI. This typically means having glib and glibmm 2.80 or later and Orca 49 or later for the Orca backend to function. Speech-dispatcher will work with any version of speech dispatcher that supports ABI version 2 (which means that versions 0.11.1 and on will definitely work).
* Android requires version 8/API level 26 or later.
* Web requires that the browser implement the [WebSpeech API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Speech_API) to be implemented.

## Building

To build Prism, all you need do is create a build directory and run cmake as you ordinarily would. The following build options are available:

| Option | Description |
| --- | --- |
| `PRISM_ENABLE_TESTS` | Build the test suite (currently reserved). |
| `PRISM_ENABLE_DEMOS` | Enable building of demo apps to demonstrate Prism either generally or being used in a specific language. |
| `PRISM_ENABLE_GDEXTENSION` | Enable building of the Godot GDExtension. |
| `PRISM_ENABLE_SHIMS` | Enable screen reader library compatibility shims. |
| `PRISM_ENABLE_TOLK_SHIM` | Enable the Tolk compatibility shim. |
| `PRISM_ENABLE_LEGACY_BACKENDS` | Build backends for legacy screen readers. |
| `PRISM_ENABLE_POWER_MANAGEMENT` | Allow Prism to manage the backend availability enumeration thread based on OS-level power management transitions if supported by the target. |
| `PRISM_ENABLE_LINTING` | Enable linting of Prism's source code using tools such as clang-tidy. This is a developer option; do not use under normal circumstances. |
| `PRISM_DIAGNOSE_VECTORIZATION` | Request that the compiler generate vectorization diagnostics to assist in understanding vectorization decisions and opportunities. This is a developer option; do not use under normal circumstances. |
| `PRISM_REGENERATE_DJINNI` | Allow regeneration of android JNI binding code. This is a developer option and requires a JRE and Djinni executable; do not use under normal circumstances. |
| `PRISM_BUILD_WINELIBS` | For x86 targets, generate winelibs for communicating to Linux backends when running under Wine/Proton. This option is a fatal error on all other architectures. |
| `PRISM_DEPENDENCY_PROVIDER` | Default provider for vendorable third-party dependencies. If `SYSTEM`, Prism will attempt to find dependencies assuming they are present on the system. If `BUNDLED`, Prism will build what dependencies it is able to. |
| `PRISM_BACKEND_DEFAULT` | The default setting for all backends compiled into Prism. If `OFF`, no backends will be compiled. If `AUTO`, Prism will determine availability of backends based on the target and what dependencies are available. If `ON`, all backends for the target will be compiled and Prism will refuse to build if their dependencies are not available. |

In addition to the aforementioned, Prism defines an option for each backend that is defined. For example, the JAWS backend can be disabled by setting `PRISM_ENABLE_JAWS_BACKEND` to `OFF`. Similarly, for each dependency that Prism requires, an option is defined to control where it comes from; for example, if your system provides `{fmt}`, you can set `PRISM_FMT_PROVIDER` to `SYSTEM`. This granular build setup is primarily useful for package managers.

Prism is also in vcpkg. To install it:

```
vcpkg install ethindp-prism
```

The following features are available:

| Feature | Description |
| --- | --- |
| `speech-dispatcher` | Enables linking to speech dispatcher and, by extension, enables the respective back-end module. If not defined, speech dispatcher will NOT be a supported backend. |
| `orca` | Enables use of glib and gdbus to communicate directly with the Orca screen reader. If not defined, Orca will NOT be available as a supported backend. |


## Documentation

Documentation uses [mdbook](https://github.com/rust-lang/mdBook). To view it offline, install mdbook and then run `mdbook serve` from the doc directory.

## API

The API is fully documented in the documentation above. If the documentation and header do not align in guarantees or expectations, this is a bug and should be reported.

## Bindings

Currently bindings are an in-progress effort. The following Bindings exist:

| Language | Package/add-on/etc. |
| --- | --- |
| .NET | [prismatoid](https://www.nuget.org/packages/prismatoid) |
| Python | [Prismatoid](https://pypi.org/project/prismatoid) |
| Godot | Prismatoid (in-tree) |

We welcome future bindings. If you write bindings and want them added here, please submit a PR!

We also encourage bindings to either follow the Prism API or the Python bindings, with appropriate modifications to the aforementioned for language conventions. Bindings are of course free to add on extra functions, such as the Godot one adding `speak_to_stream`. However, the objective should be to make transitioning between languages as painless as possible, plus take advantage of binding-specific enhancements. This paragraph however is not a requirement and bindings will be accepted either way.

## License

This project is licensed under the Mozilla Public License version 2.0. Full details are available in the LICENSE file.

This project uses code from other projects. The full listing can be found in the `NOTICE` file packaged with each release or at the repository root.

## Contributing

Contributions are welcome. This includes, but is not limited to, documentation enhancements, new backends, bindings, build system improvements, etc. The project uses C++23 so please ensure that your compiler supports that standard.
