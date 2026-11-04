# PRISM

Prism is the Platform-agnostic Reader Interface for Speech and Messages. Since that's a hell of a mouthful, we just call it Prism for short. The name comes from prisms in optics, which are transparent optical components with flat surfaces that refract light into many beams. Thus, the metaphor: refract your TTS strings to send them to many different backends, potentially simultaneously.

Prism aims to unify the various screen reader abstraction libraries like SpeechCore, UniversalSpeech, SRAL, Tolk, etc., into a single unified system with a single unified API. Of course, we also support traditional TTS engines. I have tried to develop Prism in such a way that compilation is trivial and requires no external dependencies. To that end, the CMake builder will download all needed dependencies. However, since it uses [cpm.cmake](https://github.com/cpm-cmake/CPM.cmake), vendoring of dependencies is very possible.

## Quick start

Prism picks the best backend available on the system: the user's screen reader if one is running, or a speech engine otherwise.

### Python

```
pip install prismatoid
```

The package is called `prismatoid` because `prism` on PyPI is an unrelated project, but the module you import is called `prism`:

```python
import prism

context = prism.Context()
backend = context.create_best()
backend.speak("Hello from Prism.", interrupt=True)
input(f"Using {backend.name}. Press Enter to exit.")
```

### C and C++

Download the package for your platform from the [releases page](https://github.com/ethindp/prism/releases), or see [Building](#building) to build from source.

```c
#include <prism.h>
#include <stdio.h>

int main(void) {
    PrismContext *ctx = prism_init(NULL);
    if (ctx == NULL)
        return 1;
    PrismBackend *backend = prism_registry_create_best(ctx);
    if (backend == NULL) {
        fprintf(stderr, "No speech backend is available\n");
        prism_shutdown(ctx);
        return 1;
    }
    PrismError err = prism_backend_speak(backend, "Hello from Prism.", true);
    if (err != PRISM_OK)
        fprintf(stderr, "Speech failed: %s\n", prism_error_string(err));
    printf("Using %s. Press Enter to exit.\n", prism_backend_name(backend));
    getchar();
    prism_backend_free(backend);
    prism_shutdown(ctx);
    return err == PRISM_OK ? 0 : 1;
}
```

The programs wait for you to press Enter because speech stops on some backends once the backend is destroyed. They're also available as `demos/hello_demo.py` and `demos/hello_demo.c`.

### Godot

Download `prismatoid-gdextension` from the [releases page](https://github.com/ethindp/prism/releases) and extract it into the root of your project. This requires Godot 4.5 or later. The add-on registers a singleton called `Prism`:

```gdscript
extends Node

var backend: GodotPrismBackend

func _ready() -> void:
	backend = Prism.create_best()
	if backend == null or not backend.is_valid():
		push_error("No speech backend is available")
		return
	var err := backend.speak("Hello from Prism.", true)
	if err != GodotPrismBackend.PRISM_OK:
		push_error("Speech failed: %s" % Prism.get_error_string(err))
```

## Minimum system requirements

Prism requires the following to be met to function properly. If your system does not meet these minimums, Prism MAY malfunction or fail to load at all. If you attempt to load Prism and the load fails, or if Prism malfunctions, please verify that you meet the requirements in this section before opening an issue.

* For windows, Windows 10 or later is required. Older versions of windows are NOT supported.
* Apple platforms require either MacOS 11, iOS 14, tvOS 14, WatchOS 10, or VisionOS 1 or higher.
* On Linux, the prebuilt libraries and the Godot add-on require glibc 2.34 and libstdc++ from GCC 12 or later, along with libdbus and GLib (glib, gobject, and gio) to load. The Python wheels bundle the libraries they need other than glibc and libstdc++. No backend's software needs to be installed for Prism to load; a backend is simply unavailable if what it needs is missing:
  * The Orca backend requires Orca 49 or later.
  * The Speech Dispatcher backend loads `libspeechd.so.2` at runtime. It will work with any version of speech dispatcher that supports ABI version 2 (which means that versions 0.11.1 and on will definitely work).
  * The Spiel backend loads `libspiel-1.0.so.1` at runtime.
* Android requires version 8/API level 26 or later.
* Web requires that the browser implement the [WebSpeech API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Speech_API) to be implemented.
* The GDK speech backend requires Microsoft Gaming Services to be installed. On desktop Windows, it also requires `xgameruntime.thunks.dll` from the Microsoft GDK to be on the application's library search path or beside the Prism library. Prism does not distribute this library, as the GDK's license permits only a title to redistribute it; applications that use the backend must ship it themselves. Building the backend requires the GDK's headers; on Xbox, it also requires the GDK's `xgameruntime` library.

## Building

To build Prism, all you need do is create a build directory and run cmake as you ordinarily would. The following build options are available:

| Option | Description |
| --- | --- |
| `PRISM_ENABLE_TESTS` | Build the test suite (currently reserved). |
| `PRISM_ENABLE_DEMOS` | Enable building of demo apps to demonstrate Prism either generally or being used in a specific language. |
| `PRISM_ENABLE_GDEXTENSION` | Enable building of the Godot GDExtension. |
| `PRISM_ENABLE_SHIMS` | Enable screen reader library compatibility shims. |
| `PRISM_ENABLE_TOLK_SHIM` | Enable the Tolk compatibility shim. Defaults to the value of `PRISM_ENABLE_SHIMS`. |
| `PRISM_ENABLE_UNIVERSAL_SPEECH_SHIM` | Enable the UniversalSpeech compatibility shim. Defaults to the value of `PRISM_ENABLE_SHIMS`. |
| `PRISM_ENABLE_SPEECH_CORE_SHIM` | Enable the SpeechCore compatibility shim. Defaults to the value of `PRISM_ENABLE_SHIMS`. |
| `PRISM_ENABLE_SRAL_SHIM` | Enable the SRAL compatibility shim. Defaults to the value of `PRISM_ENABLE_SHIMS`. |
| `PRISM_ENABLE_LEGACY_BACKENDS` | Build backends for legacy screen readers. |
| `PRISM_GDK_ROOT` | Root of the Microsoft GDK edition to build the GDK speech backend against. If unset, Prism uses the latest edition the GDK installer has registered. |
| `PRISM_ENABLE_POWER_MANAGEMENT` | Allow Prism to manage the backend availability enumeration thread based on OS-level power management transitions if supported by the target. |
| `PRISM_ENABLE_LINTING` | Enable linting of Prism's source code using tools such as clang-tidy. This is a developer option; do not use under normal circumstances. |
| `PRISM_MSVC_ANALYSIS` | Run MSVC code analysis (`/W4` and `/analyze` with the C++ Core Guidelines checker) over Prism's own code. Linting enables the same analysis with warnings treated as errors. This is a developer option and slows the build considerably; do not use under normal circumstances. |
| `PRISM_DIAGNOSE_VECTORIZATION` | Request that the compiler generate vectorization diagnostics to assist in understanding vectorization decisions and opportunities. This is a developer option; do not use under normal circumstances. |
| `PRISM_REGENERATE_DJINNI` | Allow regeneration of android JNI binding code. This is a developer option and requires a JRE and Djinni executable; do not use under normal circumstances. |
| `PRISM_BUILD_WINELIBS` | For x86 targets, generate winelibs for communicating to Linux backends when running under Wine/Proton. This option requires a Linux or BSD host, and is a fatal error on other hosts and on all other architectures. |
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
| `speech-dispatcher` | Enables building against speech dispatcher's headers and, by extension, enables the respective back-end module, which loads the library at runtime. If not defined, speech dispatcher will NOT be a supported backend. |


## Documentation

The manual is included in the `doc` directory of the SDK archive on the [releases page](https://github.com/ethindp/prism/releases), so you don't need to install anything to read it. Its Getting Started and Application Deployment chapters are the best places to begin, the latter of which covers what your users need installed. The API Reference covers C, Python, and Godot.

The manual uses [mdbook](https://github.com/rust-lang/mdBook). To build it yourself, install mdbook and [uv](https://docs.astral.sh/uv/) and then run `mdbook serve` from the doc directory. The Python and Godot references are generated from the binding's docstrings and the extension's class reference, so a running Python and the Godot editor's help show the same documentation.

## API

The API is fully documented in the documentation above. If the documentation and header do not align in guarantees or expectations, this is a bug and should be reported.

## Bindings

Currently bindings are an in-progress effort. The following Bindings exist:

| Language | Package/add-on/etc. |
| --- | --- |
| .NET | [prismatoid](https://www.nuget.org/packages/prismatoid) |
| Python | [Prismatoid](https://pypi.org/project/prismatoid) |
| Godot | Prismatoid (in-tree) |
| Rust | [prismer](https://crates.io/crates/prismer) |
| Tauri (TypeScript) | [tauri-plugin-prism](https://github.com/cartertemm/tauri-plugin-prism) ([crates.io](https://crates.io/crates/tauri-plugin-prism), [npm](https://www.npmjs.com/package/tauri-plugin-prism-api)) |

We welcome future bindings. If you write bindings and want them added here, please submit a PR!

We also encourage bindings to either follow the Prism API or the Python bindings, with appropriate modifications to the aforementioned for language conventions. Bindings are of course free to add on extra functions, such as the Godot one adding `speak_to_stream`. However, the objective should be to make transitioning between languages as painless as possible, plus take advantage of binding-specific enhancements. This paragraph however is not a requirement and bindings will be accepted either way.

## License

This project is licensed under the Mozilla Public License version 2.0. Full details are available in the LICENSE file.

This project uses code from other projects. The full listing can be found in the `NOTICE` file packaged with each release or at the repository root.

## Contributing

Contributions are welcome. This includes, but is not limited to, documentation enhancements, new backends, bindings, build system improvements, etc. The project uses C++23 so please ensure that your compiler supports that standard. Please read the [Contributing Guidelines](./CONTRIBUTING.md) for more information on the contribution process.

## Support

If you use Prism and like what it does, please consider sponsoring the project.
