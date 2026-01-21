# PRISM

Prism is the Platform-agnostic Reader Interface for Speech and Messages. Since that's a hell of a mouthful, we just call it Prism for short. The name comes from prisms in optics, which are transparent optical components with flat surfaces that refract light into many beams. Thus, the metaphor: refract your TTS strings to send them to many different backends, potentially simultaneously.

Prism aims to unify the various screen reader abstraction libraries like SpeechCore, UniversalSpeech, SRAL, Tolk, etc., into a single unified system with a single unified API. Of course, we also support traditional TTS engines. I have tried to develop Prism in such a way that compilation is trivial and requires no external dependencies. To that end, the CMake builder will download all needed dependencies. However, since it uses [cpm.cmake](https://github.com/cpm-cmake/CPM.cmake), vendoring of dependencies is very possible.

## Building

To build Prism, all you need do is create a build directory and run cmake as you ordinarily would. The only build option at this time is `PRISM_ENABLE_TESTS`, which will determine whether the test suite is built or not. In the 99 percent of cases, you should disable this option, since running the tests is only for validating that the library functions correctly. However, the tests have currently known problems and are still in a very early stage of development, so validation is platform-dependent and may fail in very strange ways on other platforms than Windows. (Specifically, failure is known to happen on at least MacOS, due to the header's various compiler attributes in use.)

## Documentation

Documentation uses [mdbook](https://github.com/rust-lang/mdBook). To view it offline, install mdbook and then run `mdbook serve` from the doc directory.

## API

The API is fully documented in the documentation above. If the documentation and header do not align in guarantees or expectations, this is a bug and should be reported.

## Bindings

Currently bindings are an in-progress effort. We are working on bindings to at least Python and .NET, although both will take a bit of time to actually do. If you wish to contribute bindings, please don't hesitate to open a pull request. Your bindings must obey the restrictions in the documentation; specifically, where the documentation requires something to not occur, your bindings MUST prohibit that occurence before calling any Prism C API function. For example, calling `prism_backend_speak` with a `NULL` backend or text string MUST be prohibited by your bindings.

## License

This project is licensed under the Mozilla Public License version 2.0. Full details are available in the LICENSE file.

This project uses code from other projects. Specifically:

* The SAPI bridge is credited to the [NVGT](https://github.com/samtupy/nvgt) project, as well as the functions `range_convert` and `range_convert_midpoint` in utils.h and utils.cpp. Similar attribution goes to NVGT for the Android screen reader backend.
* The `simdutf` library is licensed under the Apache-2.0 license.
* On Windows, Prism includes NVDA controller client RPC definitions under LGPL-2.1 (and generated RPC stubs from those inputs). Those portions remain under LGPL-2.1. See licenses/ and idl/ for the actual texts. On all non-Windows platforms, this does not apply, however the LGPL licenses are included for completeness in all SDKs.

## Contributing

Contributions are welcome. This includes, but is not limited to, documentation enhancements, new backends, bindings, build system improvements, etc. The project uses C++23 so please ensure that your compiler supports that standard.

