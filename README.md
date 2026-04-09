# PRISM

Prism is the Platform-agnostic Reader Interface for Speech and Messages. Since that's a hell of a mouthful, we just call it Prism for short. The name comes from prisms in optics, which are transparent optical components with flat surfaces that refract light into many beams. Thus, the metaphor: refract your TTS strings to send them to many different backends, potentially simultaneously.

Prism aims to unify the various screen reader abstraction libraries like SpeechCore, UniversalSpeech, SRAL, Tolk, etc., into a single unified system with a single unified API. Of course, we also support traditional TTS engines. I have tried to develop Prism in such a way that compilation is trivial and requires no external dependencies. To that end, the CMake builder will download all needed dependencies. However, since it uses [cpm.cmake](https://github.com/cpm-cmake/CPM.cmake), vendoring of dependencies is very possible.

## Building

To build Prism, all you need do is create a build directory and run cmake as you ordinarily would. The following build options are available:

| Option | Description |
| --- | --- |
| `PRISM_ENABLE_TESTS` | Build the test suite (currently reserved). |
| `PRISM_ENABLE_DEMOS` | Enable building of demo apps to demonstrate Prism either generally or being used in a specific language. |
| `PRISM_ENABLE_LINTING` | Enable linting of source code with clang-tidy and other static analysis tools. |
| `PRISM_ENABLE_VCPKG_SPECIFIC_OPTIONS` | DO NOT USE. Enables options primarily used by the vcpkg package manager. |
| `PRISM_ENABLE_GDEXTENSION` | Build the GDExtension and Prism together (on by default). |

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

This project uses code from other projects. Specifically:

* The `simdutf` library is licensed under the Apache-2.0 license, and is used for Unicode validation and conversion.
* On Windows, Prism includes NVDA controller client RPC definitions originally under LGPL-2.1 (and generated RPC stubs from those inputs). The Prism project has received permission to license the IDL files (and there generated outputs) under the MPL-2.0 regardless of the original license. Thus, you may assume that they are licensed under the MPL-2.0. The LGPL headers and license file in the LICENSES directory remain for providing attribution.

## Contributing

Contributions are welcome. This includes, but is not limited to, documentation enhancements, new backends, bindings, build system improvements, etc. The project uses C++23 so please ensure that your compiler supports that standard.

When requesting  features or enhancements, or submitting issues, please adhere to the following rules:

