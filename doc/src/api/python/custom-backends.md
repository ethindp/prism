## Custom Backends and Registries

This chapter documents the Python interface to the facilities described in the C API's chapters on [custom backends](../c/custom-backends.md) and [shared library backends](../c/shared-library-backends.md). The requirements stated there apply to backends implemented in Python.

### `CustomBackend`

The base class of a backend implemented in Python.

#### Syntax

```python
class CustomBackend:
    EXTRA_FEATURES: BackendFeatures = BackendFeatures()

    def initialize(self) -> None: ...
    def is_supported(self) -> bool: ...
    def speak(self, text: str, interrupt: bool) -> None: ...
    def speak_to_memory(self, text: str, emit: AudioCallback) -> None: ...
    def braille(self, text: str) -> None: ...
    def output(self, text: str, interrupt: bool) -> None: ...
    def stop(self) -> None: ...
    def pause(self) -> None: ...
    def resume(self) -> None: ...
    def is_speaking(self) -> bool: ...
    def set_volume(self, volume: float) -> None: ...
    def get_volume(self) -> float: ...
    def set_rate(self, rate: float) -> None: ...
    def get_rate(self) -> float: ...
    def set_pitch(self, pitch: float) -> None: ...
    def get_pitch(self) -> float: ...
    def refresh_voices(self) -> None: ...
    def count_voices(self) -> int: ...
    def get_voice_name(self, voice_id: int) -> str: ...
    def get_voice_language(self, voice_id: int) -> str: ...
    def set_voice(self, voice_id: int) -> None: ...
    def get_voice(self) -> int: ...
    def get_channels(self) -> int: ...
    def get_sample_rate(self) -> int: ...
    def get_bit_depth(self) -> int: ...
```

#### Members

Each method corresponds to the member of [`PrismBackendVTable`](../c/custom-backends.md#prismbackendvtable) with the same name.

`EXTRA_FEATURES`

Feature bits that do not arise from overriding a method, such as `performs_silence_trimming_on_speak`. Declaring a bit that corresponds to a method causes `ValueError` to be raised at registration.

#### Remarks

A subclass overrides only the operations it supports. The feature bit for an operation is set if, and only if, the subclass overrides the corresponding method, and the registration supplies a vtable function for only those methods. `is_supported` returns `True` unless overridden, and `initialize` does nothing unless overridden.

The class MUST be constructible with no arguments, since an instance is constructed whenever Prism creates the backend.

An exception raised by a method is printed and is reported to Prism as follows:

| Exception | `PrismError` |
| --- | --- |
| `NotImplementedError` | `PRISM_ERROR_NOT_IMPLEMENTED` |
| `UnicodeError` | `PRISM_ERROR_INVALID_UTF8` |
| `ValueError` | `PRISM_ERROR_INVALID_PARAM` |
| Any other exception | `PRISM_ERROR_INTERNAL` |

An exception raised by `is_supported` is reported as a result of `False`.

Methods MAY be invoked on threads other than the thread which registered the backend, including Prism's poll thread in the case of `is_supported`.

### `RegistryBuilder`

Constructs a registry. This class wraps [`prism_registry_builder_new`](../c/custom-backends.md#prism_registry_builder_new) and [`prism_registry_builder_free`](../c/custom-backends.md#prism_registry_builder_free), and MAY be used as a context manager.

#### Syntax

```python
class RegistryBuilder:
    def __init__(self) -> None: ...
```

#### Exceptions

Raises `MemoryError` if the builder could not be created.

### `RegistryBuilder.add_backend`

Adds a backend implemented in Python. This method wraps [`prism_registry_builder_add_backend`](../c/custom-backends.md#prism_registry_builder_add_backend).

#### Syntax

```python
def add_backend(
    self,
    name: str,
    backend: type[CustomBackend],
    *,
    priority: int = 100,
) -> int: ...
```

#### Parameters

`name`

The name of the backend.

`backend`

A subclass of `CustomBackend`. The class is passed, not an instance.

`priority`

The priority of the backend.

#### Return Value

Returns the identifier assigned to the backend.

#### Exceptions

Raises `TypeError` if `backend` is not a subclass of `CustomBackend`, `RuntimeError` if the builder is spent, and the appropriate subclass of `PrismError` if the C function fails.

### `RegistryBuilder.add_library`

Adds the backends of a plugin. This method wraps [`prism_registry_builder_add_library`](../c/shared-library-backends.md#prism_registry_builder_add_library).

#### Syntax

```python
def add_library(
    self,
    path: str | os.PathLike[str],
    *,
    priority_override: int | None = None,
) -> int: ...
```

#### Parameters

`path`

The path to the shared library.

`priority_override`

A non-negative priority to assign to every backend the plugin supplies, or `None` to honor the priorities the plugin declares.

#### Return Value

Returns the number of backends added.

#### Exceptions

Raises `RuntimeError` if the builder is spent, `TypeError` if `path` does not yield a `str`, `PrismInvalidParamError` if `path` is empty or contains an embedded NUL character, `ValueError` if `priority_override` is negative, and the appropriate subclass of `PrismError` if the C function fails.

### `RegistryBuilder.freeze`

Produces a registry from the builder. This method wraps [`prism_registry_freeze`](../c/custom-backends.md#prism_registry_freeze).

#### Syntax

```python
def freeze(self) -> Registry: ...
```

#### Return Value

Returns the frozen registry.

#### Exceptions

Raises `RuntimeError` if the builder is spent or the registry could not be created.

### `RegistryBuilder.close`

Releases the builder. This method is called when the builder is destroyed or a `with` statement ends, and MAY be called more than once.

#### Syntax

```python
def close(self) -> None: ...
```

### `Registry`

A frozen registry, which is passed to [`Context`](./context.md#context). Instances are obtained from `RegistryBuilder.freeze`. The class wraps a reference to the registry, which is released through [`prism_registry_release`](../c/custom-backends.md#prism_registry_release) when the object is destroyed or `close` is called, and MAY be used as a context manager.

#### Syntax

```python
class Registry:
    def close(self) -> None: ...
```

#### Members

`close`

Releases the reference held by this object. It MAY be called more than once.

#### Remarks

A context holds its own reference to the registry, so `close` MAY be called once the registry has been bound to a context.

#### Example

```python
import prism

class Logger(prism.CustomBackend):
    def speak(self, text, interrupt):
        print(text)

with prism.RegistryBuilder() as builder:
    logger_id = builder.add_backend("Example Logger", Logger, priority=10)
    registry = builder.freeze()

context = prism.Context(registry)
backend = context.create(prism.BackendId(logger_id))
backend.speak("Hello from a custom backend")
```
