## Context

### `Context`

Creates a Prism context. This class wraps [`prism_init`](../c/context-management.md#prism_init) and [`prism_shutdown`](../c/context-management.md#prism_shutdown).

#### Syntax

```python
class Context:
    def __init__(
        self,
        registry: Registry | None = None,
        *,
        on_availability: AvailabilityCallback | None = None,
        on_baseline: BaselineCallback | None = None,
        poll_interval_ms: int = 0,
        debounce_samples: int = 0,
        backoff_max_ms: int = 0,
        auto_power_manage: bool = True,
    ) -> None: ...
```

#### Parameters

`registry`

The registry to bind the context to, or `None` to use the global registry. This corresponds to the `registry` member of [`PrismConfig`](../c/context-management.md#prismconfig).

`on_availability`

A function invoked when a backend's availability changes, or `None` to perform no polling. It receives a `BackendId`, the backend's name, and a `bool` that is `True` if the backend became available.

`on_baseline`

A function invoked once the baseline has been established, or `None`. It is ignored when `on_availability` is `None`.

`poll_interval_ms`, `debounce_samples`, `backoff_max_ms`, and `auto_power_manage`

These correspond to the members of `PrismConfig` with the prefix `availability_`, and are ignored when `on_availability` is `None`.

#### Exceptions

Raises `RuntimeError` if the context could not be created.

#### Remarks

The sampling model and the purpose of the baseline are described in the chapter on [background availability enumeration](../c/backend-enumeration.md).

The binding does not invoke `on_availability` or `on_baseline` on Prism's poll thread. Notifications are transferred to an internal thread running an `asyncio` event loop, so a callback MAY block without delaying the poll thread, and MUST provide its own synchronization for shared state. The callback MAY be a coroutine function, in which case its coroutine is run on that event loop. At most one coroutine runs at a time for each backend. If a further notification for that backend arrives while one is running, its coroutine replaces any coroutine that was waiting, so only the most recent notification is processed. Exceptions raised by a callback are logged to the `prism.dispatch` logger and are otherwise discarded. Coroutines which have not finished are cancelled when the context is destroyed.

#### Example

```python
import prism

def on_change(backend, name, available):
    print(f"{name} is now {'available' if available else 'unavailable'}")

context = prism.Context(on_availability=on_change)
```

### `Context.backends_count`

The number of backends in the registry. This read-only property wraps [`prism_registry_count`](../c/registry-functions.md#prism_registry_count).

#### Syntax

```python
@property
def backends_count(self) -> int: ...
```

### `Context.id_of`

Returns the identifier of a backend, given its position in the registry or its name. This method wraps [`prism_registry_id_at`](../c/registry-functions.md#prism_registry_id_at) and [`prism_registry_id`](../c/registry-functions.md#prism_registry_id).

#### Syntax

```python
def id_of(self, index_or_name: int | str) -> BackendId: ...
```

#### Parameters

`index_or_name`

The position of the backend in the registry, or its name.

#### Return Value

Returns the identifier of the backend.

#### Exceptions

Raises `KeyError` if no backend matches, and `TypeError` if the argument is neither an `int` nor a `str`.

### `Context.name_of`

Returns the name of a backend. This method wraps [`prism_registry_name`](../c/registry-functions.md#prism_registry_name).

#### Syntax

```python
def name_of(self, backend_id: BackendId) -> str: ...
```

#### Parameters

`backend_id`

The identifier of the backend.

#### Return Value

Returns the name of the backend.

#### Exceptions

Raises `KeyError` if the identifier is not in the registry.

### `Context.priority_of`

Returns the priority of a backend. This method wraps [`prism_registry_priority`](../c/registry-functions.md#prism_registry_priority).

#### Syntax

```python
def priority_of(self, backend_id: BackendId) -> int: ...
```

#### Parameters

`backend_id`

The identifier of the backend.

#### Return Value

Returns the value returned by the C function, including its error value, as this method raises no exception.

### `Context.exists`

Reports whether a backend is in the registry. This method wraps [`prism_registry_exists`](../c/registry-functions.md#prism_registry_exists).

#### Syntax

```python
def exists(self, backend_id: BackendId) -> bool: ...
```

#### Parameters

`backend_id`

The identifier of the backend.

#### Return Value

Returns `True` if the backend is in the registry, and `False` otherwise.

### `Context.get`

Returns a cached backend instance if one exists. This method wraps [`prism_registry_get`](../c/registry-functions.md#prism_registry_get).

#### Syntax

```python
def get(self, backend_id: BackendId) -> Backend | None: ...
```

#### Parameters

`backend_id`

The identifier of the backend.

#### Return Value

Returns the backend, or `None` if no cached instance exists. A missing instance is an ordinary result and does not raise an exception.

### `Context.create`

Creates and initializes a new instance of a backend. This method wraps [`prism_registry_create`](../c/registry-functions.md#prism_registry_create) and `prism_backend_initialize`.

#### Syntax

```python
def create(self, backend_id: BackendId) -> Backend: ...
```

#### Parameters

`backend_id`

The identifier of the backend.

#### Return Value

Returns the initialized backend.

#### Exceptions

Raises `ValueError` if the backend could not be created, and the appropriate subclass of `PrismError` if it could not be initialized.

### `Context.create_best`

Creates and initializes the highest-priority backend able to initialize. This method wraps [`prism_registry_create_best`](../c/registry-functions.md#prism_registry_create_best).

#### Syntax

```python
def create_best(self) -> Backend: ...
```

#### Return Value

Returns the initialized backend.

#### Exceptions

Raises `ValueError` if no backend could be initialized.

### `Context.acquire`

Acquires a cached or new instance of a backend and initializes it. This method wraps [`prism_registry_acquire`](../c/registry-functions.md#prism_registry_acquire) and `prism_backend_initialize`.

#### Syntax

```python
def acquire(self, backend_id: BackendId) -> Backend: ...
```

#### Parameters

`backend_id`

The identifier of the backend.

#### Return Value

Returns the initialized backend.

#### Exceptions

Raises `ValueError` if the backend could not be obtained, and the appropriate subclass of `PrismError` if it could not be initialized.

### `Context.acquire_best`

Acquires the highest-priority backend able to initialize. This method wraps [`prism_registry_acquire_best`](../c/registry-functions.md#prism_registry_acquire_best).

#### Syntax

```python
def acquire_best(self) -> Backend: ...
```

#### Return Value

Returns the initialized backend.

#### Exceptions

Raises `ValueError` if no backend could be obtained.

### `Context.pause_availability_polling`

Pauses availability polling. This method wraps [`prism_availability_poll_pause`](../c/backend-enumeration.md#prism_availability_poll_pause).

#### Syntax

```python
def pause_availability_polling(self) -> None: ...
```

### `Context.resume_availability_polling`

Resumes availability polling. This method wraps [`prism_availability_poll_resume`](../c/backend-enumeration.md#prism_availability_poll_resume).

#### Syntax

```python
def resume_availability_polling(self) -> None: ...
```

### `Context.auto_power_supported`

Reports whether polling can follow operating-system power transitions. This static method wraps [`prism_availability_auto_power_supported`](../c/backend-enumeration.md#prism_availability_auto_power_supported).

#### Syntax

```python
@staticmethod
def auto_power_supported() -> bool: ...
```

#### Return Value

Returns the value returned by the C function.
