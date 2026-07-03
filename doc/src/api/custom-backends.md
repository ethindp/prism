## Custom Backends

This chapter specifies the interface through which an application supplies its own backend implementations to Prism, and the obligations each party assumes toward the other.

### Definitions

For the purposes of this chapter, the following terms have the meanings given below.

1. A custom backend is a backend implemented by the application and described to Prism by a table of function pointers of type `PrismBackendVTable`.
2. A registration is the record Prism retains of one successful call to `prism_registry_builder_add_backend`, comprising the copied vtable, the `userdata` pointer, the `userdata_free` function, the declared feature set, and the backend's name, identifier, and priority.
3. A builder is a mutable collection of registrations, represented by a `PrismRegistryBuilder`. A builder is at all times either live or spent. A builder is live when created and becomes spent when successfully frozen. No operation returns a spent builder to the live state.
4. A registration is referenced by the live builder that holds it, by any registry frozen from that builder, and by every backend instance created from it. A registration becomes unreferenced when the last of these referents ceases to exist.
5. A registry is finalized when its reference count reaches zero. Finalization releases the registry's backend cache and the registry's references to its registrations. Finalization does not affect backend instances, which remain valid until freed.

### General

A custom backend participates in a registry on the same terms as a compiled-in backend. It has an identifier derived from its name by the hash function described in the chapter on backend identifiers, a priority, and a declared feature set, and it participates in the priority-ordered selection performed by `prism_registry_create_best` and `prism_registry_acquire_best`. No restriction is placed on the priority of a custom backend relative to the compiled-in backends.

Custom backends are never present in the global registry. An application wishing to use one constructs a registry as follows:

1. It creates a builder with `prism_registry_builder_new`.
2. Registrations can then be added with `prism_registry_builder_add_backend`.
3. Once the builder has been populated with all registrations the application wishes to perform, it freezes the builder with `prism_registry_freeze`.
4. The registry is bound to one or more contexts through the `registry` field of `PrismConfig`.

A newly created builder already contains an entry for every compiled-in backend, so a frozen registry is always a superset of the global registry.

A registry cannot be modified after it is frozen. An application requiring a different set of custom backends constructs a new registry.

### Guarantees made to the implementation

Prism validates the following before consulting the implementation. A call that fails validation returns the indicated error without any vtable function being invoked.

* Every string argument is non-null, null-terminated, and valid UTF-8. A call supplying invalid UTF-8 fails with `PRISM_ERROR_INVALID_UTF8`.
* Every argument to `set_volume`, `set_rate`, and `set_pitch` is finite and lies within `[0.0, 1.0]`. A call supplying any other value fails with `PRISM_ERROR_RANGE_OUT_OF_BOUNDS`.
* No member other than `initialize`, `create`, `destroy`, and `is_supported` is invoked before an invocation of `initialize` has succeeded on the instance in question (or, if the `initialize` member is null, before initialization has trivially succeeded). A call arriving earlier fails with `PRISM_ERROR_NOT_INITIALIZED`.
* `initialize` is not invoked on an instance whose initialization has already succeeded. Such a call fails with `PRISM_ERROR_ALREADY_INITIALIZED`.
* `pause` is not invoked on an instance Prism considers paused, and `resume` is not invoked on an instance Prism does not consider paused. Such calls fail with `PRISM_ERROR_ALREADY_PAUSED` and `PRISM_ERROR_NOT_PAUSED` respectively. An instance is considered paused after a successful `pause`, and ceases to be considered paused after a successful `resume` or a successful `stop`.

Prism further guarantees two invariants that involve no per-call validation. First, vtable functions are never invoked concurrently for the same instance, provided the application observes the constraints given in the chapter on thread safety. Second, every invocation of `destroy` arising from a given registration precedes the invocation of `userdata_free` for that registration.

An implementation MAY rely on the guarantees in this section and need not defend against their violation. State that Prism cannot determine from the sequence of calls it makes, in particular whether speech is in progress, is not tracked by Prism and remains the responsibility of the implementation.

### Requirements on implementations

An implementation of a custom backend MUST adhere to the following.

1. A vtable function MUST return a defined `PrismError` value. Prism substitutes `PRISM_ERROR_UNKNOWN` for any return value outside the defined range rather than propagate it to the application.
2. A vtable function MUST NOT return abnormally across the Prism boundary. Unwinding through Prism, whether by a C++ exception, by `longjmp`, or by any other means, results in undefined behavior.
3. `speak_to_memory` MUST deliver audio synchronously: every invocation of the callback it receives MUST occur before it returns. Retaining the callback pointer or its userdata beyond that point, or invoking either afterwards, likewise results in undefined behavior. This requirement is stricter than the corresponding contract for compiled-in backends, some of which deliver audio asynchronously.
4. Values reported through `get_volume`, `get_rate`, and `get_pitch` SHOULD lie within `[0.0, 1.0]`, and audio samples delivered through the `speak_to_memory` callback SHOULD lie within `[-1.0, 1.0]`. Prism tolerates limited departures from both. For parameter values, a finite out-of-range value is clamped into range, and a non-finite value causes the call to fail with `PRISM_ERROR_BACKEND_ENTERED_UNDEFINED_STATE`. For audio samples, a finite out-of-range sample is clamped, and a non-finite sample is replaced with silence. This sanitization exists so that a defective implementation cannot corrupt the application and MUST NOT be relied upon.
5. The function pointers in a registered vtable MUST remain valid until the registration is unreferenced. Prism has no means of detecting that the code behind a vtable has been unloaded; unloading it early leaves dangling function pointers that Prism may later invoke.

### `PrismBackendVTable`

A table of function pointers implementing a custom backend.

#### Syntax

```c
typedef struct PrismBackendVTable {
  size_t size;
  void *(*create)(void *userdata);
  void (*destroy)(void *instance);
  bool (*is_supported)(void *instance);
  PrismError (*initialize)(void *instance);
  PrismError (*speak)(void *instance, const char *text, bool interrupt);
  PrismError (*speak_to_memory)(void *instance, const char *text,
                                PrismAudioCallback callback,
                                void *callback_userdata);
  PrismError (*braille)(void *instance, const char *text);
  PrismError (*output)(void *instance, const char *text, bool interrupt);
  PrismError (*stop)(void *instance);
  PrismError (*pause)(void *instance);
  PrismError (*resume)(void *instance);
  PrismError (*is_speaking)(void *instance, bool *out_speaking);
  PrismError (*set_volume)(void *instance, float volume);
  PrismError (*get_volume)(void *instance, float *out_volume);
  PrismError (*set_rate)(void *instance, float rate);
  PrismError (*get_rate)(void *instance, float *out_rate);
  PrismError (*set_pitch)(void *instance, float pitch);
  PrismError (*get_pitch)(void *instance, float *out_pitch);
  PrismError (*refresh_voices)(void *instance);
  PrismError (*count_voices)(void *instance, size_t *out_count);
  PrismError (*get_voice_name)(void *instance, size_t voice_id,
                               const char **out_name);
  PrismError (*get_voice_language)(void *instance, size_t voice_id,
                                   const char **out_language);
  PrismError (*set_voice)(void *instance, size_t voice_id);
  PrismError (*get_voice)(void *instance, size_t *out_voice_id);
  PrismError (*get_channels)(void *instance, size_t *out_channels);
  PrismError (*get_sample_rate)(void *instance, size_t *out_sample_rate);
  PrismError (*get_bit_depth)(void *instance, size_t *out_bit_depth);
} PrismBackendVTable;
```

#### Members

`size`

The size of this structure as known to the application. This member MUST be set to `sizeof(PrismBackendVTable)`. Prism reads at most `size` bytes from the structure. If `size` is smaller than the size of the structure as this version of Prism defines it, the members beyond `size` are treated as null; if it is larger, the additional bytes are ignored. This scheme permits the structure to grow in later library versions without invalidating applications compiled against earlier ones.

`create`

An optional function producing a per-instance state pointer. If non-null, Prism invokes it exactly once for each backend instance constructed from the registration, passing the registration's `userdata`, and thereafter passes the returned pointer as the `instance` argument to every other member invoked for that instance. Should `create` return `NULL`, construction of the instance fails. If `create` is null, the registration's `userdata` pointer is passed as the `instance` argument directly, and all instances of the backend consequently share it.

`destroy`

An optional function releasing a state pointer previously returned by `create`. If both `create` and `destroy` are non-null, Prism invokes `destroy` exactly once for each backend instance, at the time the instance is freed. `destroy` is never invoked if `create` is null.

`is_supported`

An optional runtime availability probe. If non-null, Prism invokes it to determine the `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` bit reported by `prism_backend_get_features`; if null, the bit declared at registration is reported unchanged. Because `prism_backend_get_features` MAY be called before initialization, `is_supported` MAY be invoked before `initialize` has succeeded, and an implementation of it MUST NOT assume the instance has been initialized. This member designates no operation and is therefore exempt from the feature consistency requirement of `prism_registry_builder_add_backend`.

`initialize` through `get_bit_depth`

Optional functions implementing the corresponding backend operations. Each carries the contract of the `prism_backend_` function of the same name, except that its first argument is the instance pointer. A null member denotes an unimplemented operation, for which Prism returns `PRISM_ERROR_NOT_IMPLEMENTED` without invoking the implementation, exactly as for a compiled-in backend. As the sole exception, a null `initialize` causes initialization to succeed trivially, for the benefit of implementations requiring no setup.

### prism_registry_builder_new

Creates a new registry builder seeded with the compiled-in backends.

#### Syntax

```c
PrismRegistryBuilder *prism_registry_builder_new(void);
```

#### Parameters

This function has no parameters.

#### Return Value

Returns a pointer to a newly allocated, live `PrismRegistryBuilder` on success. Returns `NULL` if memory allocation fails.

#### Remarks

The returned builder contains an entry for every backend compiled into the library, at the priority each backend declares. The builder is owned by the caller and MUST eventually be released with `prism_registry_builder_free`, whether or not it is frozen first.

A builder is not a registry. It cannot be named by the `registry` field of `PrismConfig`, and no function outside this chapter accepts one. Its sole purpose is to accumulate registrations and be frozen.

### prism_registry_builder_add_backend

Adds a custom backend to a registry builder.

#### Syntax

```c
PrismError prism_registry_builder_add_backend(PrismRegistryBuilder *builder,
                                              const char *name, int priority,
                                              uint64_t features,
                                              const PrismBackendVTable *vtable,
                                              void *userdata,
                                              void (*userdata_free)(void *),
                                              PrismBackendId *out_id);
```

#### Parameters

`builder`

The builder to add the backend to. This parameter MUST NOT be `NULL`.

`name`

The backend's name, as a null-terminated UTF-8 string. This parameter MUST NOT be `NULL` and MUST NOT be empty. The backend's identifier is derived from this name in the same manner as for compiled-in backends. The string is copied and therefore may be freed after this function returns.

`priority`

The backend's priority. Higher values indicate higher priority, exactly as for compiled-in backends.

`features`

The feature set the backend declares, formed by ORing `PRISM_BACKEND_*` feature constants together. The declared set MUST satisfy the consistency requirement given under Remarks.

`vtable`

The vtable implementing the backend. This parameter MUST NOT be `NULL`, and its `size` member MUST be set as specified above. The structure is copied during this call.

`userdata`

An opaque pointer passed to the backend's `create` function, or used directly as the instance pointer if no `create` function is supplied. This parameter MAY be `NULL`.

`userdata_free`

An optional function releasing `userdata`, subject to the invocation guarantee given under Remarks. This parameter MAY be `NULL`.

`out_id`

An optional pointer receiving the identifier assigned to the backend. This parameter MAY be `NULL`. It is written only when the function returns `PRISM_OK`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | The backend was added to the builder. |
| `PRISM_ERROR_INVALID_PARAM` | The vtable's `size` member was zero or the declared feature set was inconsistent with the vtable. |
| `PRISM_ERROR_INVALID_UTF8` | `name` contains invalid UTF-8 sequences. |
| `PRISM_ERROR_INVALID_OPERATION` | The builder is spent, or a backend with the same name or the same identifier is already present in the builder. |
| `PRISM_ERROR_MEMORY_FAILURE` | Memory allocation failed. |

#### Remarks

For every feature constant that designates an operation, the corresponding vtable member MUST be non-null if and only if the feature is declared. A registration violating this requirement is rejected during this call, with the consequence that no context can ever observe a backend whose declared features and implemented operations disagree. The `is_supported` member is exempt, as it designates no operation.

Every builder begins with the compiled-in backends present, so their names and identifiers are always reserved and MUST NOT be used by any custom backend. A custom backend whose name collides with one of them is rejected as a duplicate.

If `userdata_free` is non-null, Prism invokes it with `userdata` exactly once per call to this function, whatever the outcome. On failure, the invocation occurs before the call returns. On success, it is deferred until the registration becomes unreferenced, and occurs synchronously on the thread whose action caused the registration to become unreferenced; it is unspecified which library function performs the invocation when more than one could. This invocation is the only notification the application receives that `userdata`, and the code behind the vtable's function pointers, are no longer needed. Applications that load speech engines dynamically SHOULD tie unloading to it.

Adding a backend constructs no instance of it. The `create` member, if any, is first invoked when an instance is created from a registry frozen from this builder.

### prism_registry_freeze

Freezes a builder, producing an immutable registry.

#### Syntax

```c
PrismRegistry *prism_registry_freeze(PrismRegistryBuilder *builder);
```

#### Parameters

`builder`

The builder to freeze. This parameter MUST NOT be `NULL`.

#### Return Value

Returns a pointer to a newly created `PrismRegistry` on success. Returns `NULL` if the builder is spent or if memory allocation fails.

#### Remarks

On success, the builder's registrations are transferred to the returned registry and the builder becomes spent. A spent builder holds no registrations, and no operations may be performed on it other than freeing it.

The returned registry is reference-counted, with an initial count of one owned by the caller. Each context bound to the registry holds an additional reference for the lifetime of the context. The caller MUST eventually release its reference with `prism_registry_release`. The registry is finalized when the last reference, whichever party holds it, is released.

#### Example

The following program defines a minimal custom backend that writes speech requests to a log file, registers it, and speaks through it. Because the vtable supplies no `create` function, the single `LogEngine` allocated by the application serves as the instance state directly, and `log_engine_free` receives it when the registration becomes unreferenced.

```c
#include <stdio.h>
#include <stdlib.h>
#include <prism.h>

typedef struct {
    FILE *log;
} LogEngine;

static PrismError log_speak(void *instance, const char *text, bool interrupt) {
    (void)interrupt;
    LogEngine *engine = (LogEngine*)instance;
    fprintf(engine->log, "speak: %s\n", text);
    fflush(engine->log);
    return PRISM_OK;
}

static PrismError log_stop(void *instance) {
    (void)instance;
    return PRISM_OK;
}

static void log_engine_free(void *userdata) {
    LogEngine *engine = (LogEngine*)userdata;
    fclose(engine->log);
    free(engine);
}

static const PrismBackendVTable log_vtable = {
    .size = sizeof(PrismBackendVTable),
    .speak = log_speak,
    .stop = log_stop,
};

int main(void) {
    LogEngine *engine = malloc(sizeof(LogEngine));
    if (!engine) {
        return 1;
    }
    engine->log = fopen("speech.log", "w");
    if (!engine->log) {
        free(engine);
        return 1;
    }
    PrismRegistryBuilder *builder = prism_registry_builder_new();
    if (!builder) {
        log_engine_free(engine);
        return 1;
    }
    PrismBackendId id;
    PrismError err = prism_registry_builder_add_backend(
        builder, "Example Logger", 10,
        PRISM_BACKEND_SUPPORTS_SPEAK | PRISM_BACKEND_SUPPORTS_STOP,
        &log_vtable, engine, log_engine_free, &id);
    if (err != PRISM_OK) {
        /* Ownership of engine has already passed to Prism; userdata_free
           has been invoked. Only the builder remains to clean up. */
        fprintf(stderr, "Registration failed: %s\n", prism_error_string(err));
        prism_registry_builder_free(builder);
        return 1;
    }
    PrismRegistry *registry = prism_registry_freeze(builder);
    prism_registry_builder_free(builder);
    if (!registry) {
        return 1;
    }
    PrismConfig cfg = prism_config_init();
    cfg.registry = registry;
    PrismContext *ctx = prism_init(&cfg);
    if (!ctx) {
        prism_registry_release(registry);
        return 1;
    }
    PrismBackend *backend = prism_registry_create(ctx, id);
    if (backend) {
        prism_backend_initialize(backend);
        prism_backend_speak(backend, "Hello from a custom backend", true);
        prism_backend_free(backend);
    }
    prism_shutdown(ctx);
    prism_registry_release(registry);
    return 0;
}
```

### prism_registry_builder_free

Releases a registry builder.

#### Syntax

```c
void prism_registry_builder_free(PrismRegistryBuilder *builder);
```

#### Parameters

`builder`

The builder to release. This parameter MAY be `NULL`.

#### Return Value

This function does not return a value.

#### Remarks

This function releases a builder in any state. After it returns, the `builder` pointer is invalid, and any use of it results in undefined behavior.

Releasing a spent builder frees bookkeeping only. Releasing a live builder discards the registrations it holds; each discarded registration thereby becomes unreferenced, with the consequence for `userdata_free` specified under `prism_registry_builder_add_backend`.

If `builder` is `NULL`, this function has no effect.

### prism_registry_retain

Increments the reference count of a registry.

#### Syntax

```c
PrismRegistry *prism_registry_retain(PrismRegistry *registry);
```

#### Parameters

`registry`

The registry to retain. This parameter MAY be `NULL`.

#### Return Value

Returns `registry`.

#### Remarks

This function increments the registry's reference count and returns the same pointer, permitting inline use wherever a retained copy is wanted. It is provided primarily for language bindings and other code that manages registry lifetime by reference counting. Every successful retain MUST be balanced by exactly one later call to `prism_registry_release`.

If `registry` is `NULL`, this function has no effect and returns `NULL`.

### prism_registry_release

Decrements the reference count of a registry, finalizing it when the count reaches zero.

#### Syntax

```c
void prism_registry_release(PrismRegistry *registry);
```

#### Parameters

`registry`

The registry to release. This parameter MAY be `NULL`.

#### Return Value

This function does not return a value.

#### Remarks

When the reference count reaches zero, the registry is finalized as defined at the beginning of this chapter. A registration held by the finalized registry becomes unreferenced at that moment only if no backend instance created from it remains alive; otherwise it becomes unreferenced when the last such instance is freed. Backend instances are unaffected by finalization and remain valid until freed.

After the count reaches zero, the `registry` pointer is invalid, and any use of it results in undefined behavior. An application MUST NOT release more references than it holds. Releasing the application's own reference while contexts bound to the registry remain alive is permitted; the registry then survives until those contexts have been shut down.

If `registry` is `NULL`, this function has no effect.
