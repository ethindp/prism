# Getting Started

This chapter describes how to obtain Prism and how to produce speech from an application. It is an introduction and does not state the complete contract of any function; the C API Reference remains authoritative. Requirements that apply when an application is distributed to other systems are described in the Application Deployment chapter.

## Obtaining Prism

C and C++ applications obtain Prism from the GitHub releases page, where each platform has its own archive and the SDK archive contains every platform along with this manual. Prism is also available as the `ethindp-prism` port in vcpkg, whose `speech-dispatcher` feature enables the Speech Dispatcher backend on Linux, and can be built from source using the options described in the README.

Python applications use the `prismatoid` package on PyPI. Godot applications use the `prismatoid-gdextension` archive on the releases page, which contains the add-on for every supported platform. Bindings for other languages are maintained separately and are listed in the README.

The name of the Python package differs from the name of its module because the name `prism` on PyPI belongs to an unrelated project. Applications MUST therefore declare a dependency on `prismatoid`, and MUST use `import prism` in their source code. Installing the unrelated `prism` package in the same environment causes the two modules to conflict.

## A first program

The following C program speaks a single sentence through the best backend available:

```c
#include <prism.h>
#include <stdio.h>

int main(void) {
    PrismContext *ctx = prism_init(NULL);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to initialize Prism\n");
        return 1;
    }
    PrismBackend *backend = prism_registry_create_best(ctx);
    if (backend == NULL) {
        fprintf(stderr, "No speech backend is available\n");
        prism_shutdown(ctx);
        return 1;
    }
    PrismError err = prism_backend_speak(backend, "Hello from Prism.", true);
    if (err != PRISM_OK) {
        fprintf(stderr, "Speech failed: %s\n", prism_error_string(err));
    }
    printf("Using %s. Press Enter to exit.\n", prism_backend_name(backend));
    getchar();
    prism_backend_free(backend);
    prism_shutdown(ctx);
    return err == PRISM_OK ? 0 : 1;
}
```

`prism_init` creates a context, through which the backend registry is accessed. `prism_registry_create_best` creates and initializes the highest-priority backend that is able to initialize, so the program does not need to know which screen readers or speech engines are present on the system. `prism_backend_speak` submits the text, with the final argument requesting that any speech in progress be interrupted. Finally, `prism_backend_free` and `prism_shutdown` release the resources the program obtained.

`prism_registry_create_best` returns `NULL` if no backend can be initialized. This happens in ordinary use, for example in a headless environment with no session services and no speech engine, and applications MUST handle it.

The program waits for input before exiting because `prism_backend_speak` MAY return before speech has finished, and because some backends discard queued and playing speech when the backend instance is freed. An application with a longer lifetime does not need to take any such measure, provided that it retains the backend for as long as it expects speech to continue. Waiting by polling `prism_backend_is_speaking` is not portable, as not all backends implement that function.

## Compiling the program

Using a release package on Linux, the program can be compiled as follows, where `$PRISM` is the directory into which the archive was extracted:

```text
cc hello.c -o hello -I"$PRISM/include" -L"$PRISM/dynamic/release/lib" -lprism -Wl,-rpath,"$PRISM/dynamic/release/lib"
```

Using a release package on Windows, the program can be compiled from a Developer Command Prompt as follows, where `%PRISM%` is the directory into which the archive was extracted:

```text
cl hello.c /I"%PRISM%\include" /link "%PRISM%\dynamic\release\lib\prism.lib"
```

The file `prism.dll` from the package's `dynamic\release\bin` directory MUST then be placed beside `hello.exe` or otherwise be resolvable through the process's library search order.

A CMake project can locate Prism as a package, whether it was obtained through vcpkg, installed from source, or extracted from a release package:

```cmake
find_package(prism CONFIG REQUIRED)
add_executable(hello hello.c)
target_link_libraries(hello PRIVATE prism::prism)
```

In the case of a release package, the directory of the desired configuration is supplied when the project is configured, as in `-DCMAKE_PREFIX_PATH="$PRISM/dynamic/release"`.

This program is included in the Prism source tree as `demos/hello_demo.c`, and is built along with the other demonstration programs when `PRISM_ENABLE_DEMOS` is enabled.

Prism's implementation is written in C++. A program which links the static form of the library MUST therefore also link the C++ standard library, most easily by using the C++ compiler driver for the link step. A CMake project does so automatically, provided that it enables the `CXX` language in its `project` command, even if the project's own sources are written in C.

## Python

The equivalent program in Python is as follows:

```python
import prism

context = prism.Context()
backend = context.create_best()
backend.speak("Hello from Prism.", interrupt=True)
input(f"Using {backend.name}. Press Enter to exit.")
```

This program is included in the Prism source tree as `demos/hello_demo.py`.

Functions report failure by raising exceptions. `create_best` raises `ValueError` if no backend can be initialized, and other failures raise subclasses of `prism.PrismError`. Prism's resources are released when the corresponding Python objects are destroyed, so an application MUST retain a reference to the backend for as long as it expects speech to continue.

## Godot

The add-on is installed by extracting the archive into the root of a Godot project, which places it in `addons/prismatoid`. The add-on requires Godot 4.5 or later. No further configuration is required, and the extension registers a global singleton named `Prism`:

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

Operations that can fail return a `PrismError` value, which applications SHOULD inspect. The backend is held in a member variable because a backend held only by a local variable is released when the function returns, which, on some backends, discards the speech it has queued.

## Next steps

The Screen Readers and Speech Engines chapter explains which backend a program will receive and how an application should use it. The API Reference documents the C, Python, and Godot interfaces, and the Application Deployment chapter describes what is required for the application to function on other systems.
