## Backend Registry Functions

The backend registry maintains the list of available backends and provides facilities for creating and managing backend instances. Backends are registered at library load time; the registry contents cannot be modified at runtime by application code.

### prism_registry_count

Returns the number of backends registered in the registry.

#### Syntax

```c
size_t prism_registry_count(PrismContext *ctx);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

#### Return Value

Returns the number of registered backends.

#### Remarks

The return value represents the number of backends that were compiled into the Prism library and registered themselves at load time. This count is constant for the lifetime of the process; backends cannot be registered or unregistered at runtime.

A return value of zero indicates that no backends are available, which typically means Prism was compiled without any backend support or is running on a platform for which no backends were configured. Applications SHOULD check for this condition and report an appropriate error to the user.

Note that the number of registered backends does not necessarily equal the number of usable backends. Some backends may fail to initialize because their underlying system component is not installed. Use `prism_registry_create_best` or `prism_registry_acquire_best` to find a backend that actually works.

### prism_registry_id_at

Returns the backend ID at the specified index in the registry.

#### Syntax

```c
PrismBackendId prism_registry_id_at(PrismContext *ctx, size_t index);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

`index`

The zero-based index into the backend list. This value MUST be less than the value returned by `prism_registry_count`.

#### Return Value

Returns the backend ID at the specified index on success. Returns `PRISM_BACKEND_INVALID` if `index` is out of range.

#### Remarks

Backends are stored in the registry in descending priority order. Index 0 corresponds to the highest-priority backend, index 1 to the second-highest, and so on. This ordering is determined at compile time based on the priority values assigned to each backend.

The priority ordering reflects a general preference hierarchy. Screen reader backends (such as NVDA, JAWS, and Orca) typically have higher priority than standalone TTS backends (such as SAPI and OneCore) because, when a screen reader is running, applications generally want to route speech through it rather than speaking independently. However, applications that have specific requirements MAY ignore this ordering and select backends by ID or name.

The mapping between indices and IDs is stable for the lifetime of the process. The ID at a given index will not change unless the process is restarted.

### prism_registry_id

Looks up a backend by name and returns its ID.

#### Syntax

```c
PrismBackendId prism_registry_id(PrismContext *ctx, const char *name);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

`name`

The backend name to look up. This parameter MUST NOT be `NULL` and MUST be a valid null-terminated string.

#### Return Value

Returns the backend ID corresponding to the given name on success. Returns `PRISM_BACKEND_INVALID` if no backend with the given name exists.

#### Remarks

Backend names are human-readable strings such as "SAPI", "NVDA", "Speech Dispatcher", etc. Name comparison is exact and case-sensitive. The name "sapi" will not match a backend named "SAPI".

If the application knows the backend it wants at compile time, using the predefined `PRISM_BACKEND_*` constants is more efficient than calling this function, as it avoids the string comparison overhead.

The names used by Prism backends are:

| Backend | Name |
| --- | --- |
| SAPI | `"SAPI"` |
| AVSpeech | `"AVSpeech"` |
| VoiceOver | `"VoiceOver"` |
| Speech Dispatcher | `"Speech Dispatcher"` |
| NVDA | `"NVDA"` |
| JAWS | `"JAWS"` |
| OneCore | `"OneCore"` |
| Orca | `"Orca"` |
| Android text-to-speech services | `"AndroidTextToSpeech"` |
| Android screen readers (via Android's accessibility manager) | `"AndroidScreenReader"` |
| Web SpeechSynthesis API | `"WebSpeechSynthesis"` |
| UIAutomation | `"UIA"` |
| Zhengdu Screen Reader | `"ZDSR"` |
| ZoomText | `"ZoomText"` |

### prism_registry_name

Returns the human-readable name of a backend given its ID.

#### Syntax

```c
const char *prism_registry_name(PrismContext *ctx, PrismBackendId id);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

`id`

The backend ID to look up.

#### Return Value

Returns a pointer to a null-terminated string containing the backend name on success. Returns `NULL` if the ID is not found in the registry.

#### Remarks

The returned string is owned by the registry and remains valid for the lifetime of the process. Applications MUST NOT modify or free the returned string.

This function is the inverse of `prism_registry_id`. Given an ID obtained from any source (such as `prism_registry_id_at`, a predefined constant, or storage), this function returns the corresponding human-readable name.

### prism_registry_priority

Returns the priority value of a backend.

#### Syntax

```c
int prism_registry_priority(PrismContext *ctx, PrismBackendId id);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

`id`

The backend ID to look up.

#### Return Value

Returns the priority value of the backend on success. Returns `-1` if the ID is not found in the registry.

#### Remarks

Priority values are positive integers assigned to backends at compile time. Higher values indicate higher priority. When `prism_registry_create_best` or `prism_registry_acquire_best` is called, Prism attempts to create backends in descending priority order until one succeeds.

The priority system is designed to select the most appropriate backend automatically. Screen reader backends are assigned higher priorities because, when a user is running a screen reader, they generally expect all speech to go through it. If no screen reader is running, those backends will fail to initialize, and Prism will fall back to a standalone TTS backend.

Applications MAY use priority values to inform user interface decisions, such as grouping backends by type or indicating which backend will be selected by default. However, applications SHOULD NOT rely on specific priority values, as they may change in future versions of Prism.

### prism_registry_exists

Checks whether a backend with the given ID exists in the registry.

#### Syntax

```c
bool prism_registry_exists(PrismContext *ctx, PrismBackendId id);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

`id`

The backend ID to check.

#### Return Value

Returns `true` if a backend with the given ID exists in the registry. Returns `false` if the backend does not exist or if `id` is `PRISM_BACKEND_INVALID`.

#### Remarks

This function checks only whether the backend is registered, not whether it can be successfully initialized. A backend may exist in the registry but fail to initialize because its underlying system component is not installed or not running.

For the predefined `PRISM_BACKEND_*` constants, this function effectively checks whether Prism was compiled with support for that backend and whether it is available on the current platform.

### prism_registry_get

Retrieves a cached backend instance if one exists.

#### Syntax

```c
PrismBackend *prism_registry_get(PrismContext *ctx, PrismBackendId id);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

`id`

The backend ID to retrieve.

#### Return Value

Returns a pointer to the cached backend instance if one exists and is still alive. Returns `NULL` if no cached instance exists, if the cached instance has been freed, or if the ID is not found in the registry.

#### Remarks

This function provides read-only access to the backend cache. It does not create a new instance; it only returns an existing instance that was previously created by `prism_registry_acquire` or `prism_registry_acquire_best` and has not yet been freed.

The cache uses weak references internally. When all handles to a cached backend have been passed to `prism_backend_free`, the backend is destroyed and removed from the cache. Subsequent calls to `prism_registry_get` will return `NULL` until a new instance is created via `prism_registry_acquire`.

Note that the returned handle shares ownership with other handles to the same instance. The caller SHOULD call `prism_backend_free` when done, but the underlying backend will not be destroyed until all handles have been freed.

### prism_registry_create

Creates a new backend instance.

#### Syntax

```c
PrismBackend *prism_registry_create(PrismContext *ctx, PrismBackendId id);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

`id`

The backend ID to instantiate.

#### Return Value

Returns a pointer to a newly created backend instance on success. Returns `NULL` if the ID is not found in the registry or if memory allocation fails.

#### Remarks

Each call to `prism_registry_create` creates a new, independent backend instance regardless of whether other instances of the same backend exist. This is in contrast to `prism_registry_acquire`, which returns a cached instance if one is available.

The returned backend is not yet initialized. Applications MUST call `prism_backend_initialize` before using any other backend function except `prism_backend_name` and `prism_backend_free`. Attempting to use an uninitialized backend will result in `PRISM_ERROR_NOT_INITIALIZED`.

The caller assumes ownership of the returned backend and MUST eventually pass it to `prism_backend_free` to release resources. Failure to do so results in a resource leak.

Creating a backend instance does not guarantee that the backend will successfully initialize. Some backends require system components that may not be installed or may not be running.

If an application wants to create whichever backend will work without specifying a particular one, use `prism_registry_create_best` instead.

#### Example

```c
PrismBackend *backend = prism_registry_create(ctx, PRISM_BACKEND_SAPI);
if (!backend) {
    fprintf(stderr, "Failed to create SAPI backend\n");
    return;
}
PrismError err = prism_backend_initialize(backend);
if (err != PRISM_OK) {
    fprintf(stderr, "Failed to initialize: %s\n", prism_error_string(err));
    prism_backend_free(backend);
    return;
}
/* Backend is ready to use */
```

### prism_registry_create_best

Creates a new instance of the highest-priority backend that successfully initializes.

#### Syntax

```c
PrismBackend *prism_registry_create_best(PrismContext *ctx);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

#### Return Value

Returns a pointer to a newly created and initialized backend instance on success. Returns `NULL` if no backend could be created and initialized.

#### Remarks

This function iterates through all registered backends in descending priority order. For each backend, it attempts to create an instance and initialize it. If initialization succeeds, that backend is returned immediately. If initialization fails, the instance is discarded and the next backend is tried.

Unlike `prism_registry_create`, the returned backend is already initialized. Applications do not need to call `prism_backend_initialize` and SHOULD NOT do so, as it will return `PRISM_ERROR_ALREADY_INITIALIZED`.

This function is the recommended way to obtain a backend when the application does not have a specific preference. It automatically handles the common case where screen reader backends should be preferred when a screen reader is running but TTS backends should be used as a fallback

The caller assumes ownership of the returned backend and MUST eventually pass it to `prism_backend_free`.

### prism_registry_acquire

Acquires a backend instance, reusing a cached instance if available or creating a new one otherwise.

#### Syntax

```c
PrismBackend *prism_registry_acquire(PrismContext *ctx, PrismBackendId id);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

`id`

The backend ID to acquire.

#### Return Value

Returns a pointer to a backend instance (either existing or newly created) on success. Returns `NULL` if the ID is not found in the registry or if creation fails.

#### Remarks

This function implements a simple caching mechanism. When called, it first checks whether a cached instance of the requested backend exists and is still alive. If so, it returns a handle to that instance. If not, it creates a new instance and stores it in the cache before returning.

Unlike `prism_registry_create_best`, this function does not automatically initialize the backend. The returned backend may be uninitialized (if it was just created) or initialized (if it was retrieved from the cache and a previous caller initialized it). Applications SHOULD call `prism_backend_initialize` and handle `PRISM_ERROR_ALREADY_INITIALIZED` gracefully.

The returned handle shares ownership with other handles to the same cached instance. When `prism_backend_free` is called, it releases the caller's reference; the underlying backend is destroyed only when all references have been released.

Multiple calls to `prism_registry_acquire` with the same ID may return handles to the same underlying instance. This means that changes made through one handle (such as setting the voice or rate) will be visible through other handles. Applications that require isolated backend state SHOULD use `prism_registry_create` instead.

### prism_registry_acquire_best

Acquires the highest-priority backend that successfully initializes, reusing a cached instance if available.

#### Syntax

```c
PrismBackend *prism_registry_acquire_best(PrismContext *ctx);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

#### Return Value

Returns a pointer to an initialized backend instance on success. Returns `NULL` if no backend could be acquired and initialized.

#### Remarks

This function combines the automatic backend selection of `prism_registry_create_best` with the caching behavior of `prism_registry_acquire`.

When called, it iterates through backends in priority order. For each backend, it first checks the cache. If a cached instance exists, it returns that instance immediately (the cached instance is assumed to be usable since it was previously initialized). If no cached instance exists, it attempts to create and initialize a new instance. If initialization succeeds, the instance is cached and returned. If initialization fails, the next backend is tried.

The returned backend is always initialized. Applications SHOULD NOT call `prism_backend_initialize` on the returned backend.

This is the most convenient function for applications that simply want a working TTS backend without any specific preferences.

The caching behavior means that repeated calls to this function will return the same backend instance (assuming it has not been freed). This is typically the desired behavior, as it ensures consistent voice settings across the application.
