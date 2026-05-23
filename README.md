# PRISM

Prism is the Platform-agnostic Reader Interface for Speech and Messages. Since that's a hell of a mouthful, we just call it Prism for short. The name comes from prisms in optics, which are transparent optical components with flat surfaces that refract light into many beams. Thus, the metaphor: refract your TTS strings to send them to many different backends, potentially simultaneously.

Prism aims to unify the various screen reader abstraction libraries like SpeechCore, UniversalSpeech, SRAL, Tolk, etc., into a single unified system with a single unified API. Of course, we also support traditional TTS engines. I have tried to develop Prism in such a way that compilation is trivial and requires no external dependencies. To that end, the CMake builder will download all needed dependencies. However, since it uses [cpm.cmake](https://github.com/cpm-cmake/CPM.cmake), vendoring of dependencies is very possible.

## Minimum system requirements

Prism requires the following to be met to function properly. If your system does not meet these minimums, Prism MAY malfunction or fail to load at all. If you attempt to load Prism and the load fails, or if Prism malfunctions, please verify that you meet the requirements in this section before opening an issue.

* For windows, Windows 10 or later is required. Older versions of windows are NOT supported.
* Apple platforms require either MacOS 11, iOS 11, tvOS 11, WatchOS 6, or VisionOS 1.
* Linux requires Glib 2.80.0 or later and Orca 49 or later for the Orca backend to function. Speech-dispatcher will work with any version of speech dispatcher that supports ABI version 2 (which means that versions 0.11.1 and on will definitely work).
* Android requires version 8/API level 26 or later.
* Web requires that the browser implement the [WebSpeech API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Speech_API) to be implemented.

## Building

To build Prism, all you need do is create a build directory and run cmake as you ordinarily would. The following build options are available:

| Option | Description |
| --- | --- |
| `PRISM_ENABLE_TESTS` | Build the test suite (currently reserved). |
| `PRISM_ENABLE_DEMOS` | Enable building of demo apps to demonstrate Prism either generally or being used in a specific language. |
| `PRISM_ENABLE_LINTING` | Enable linting of source code with clang-tidy and other static analysis tools. |
| `PRISM_ENABLE_VCPKG_SPECIFIC_OPTIONS` | DO NOT USE. Enables options primarily used by the vcpkg package manager. |
| `PRISM_ENABLE_GDEXTENSION` | Build the GDExtension and Prism together (on by default). |
| `PRISM_ENABLE_LEGACY_BACKENDS` | Enable all legacy backends supported by Prism. For the purposes of this option and the `LEGACY_*` options which follow, a "legacy" backend is a backend which is supported but which has a very limited number of users, or is only available for compatibility reasons. |
| `PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND` | Enable the system access backend. |
| `PRISM_ENABLE_LEGACY_WINDOW_EYES_BACKEND` | Enable the window eyes backend. |
| `PRISM_ENABLE_SHIMS` | Enable compatibility shims. |
| `PRISM_ENABLE_TOLK_SHIM` | Enable Tolk compatibility shim. |
| `PRISM_BUILD_WINELIBS` | Build winelibs which allow Windows apps which use Prism to talk to Linux and BSD backends when these apps are running under Wine. Requires a Linux system and `winegcc` to be available. |

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
