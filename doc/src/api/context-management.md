## Context Management

### `PrismConfig`

A struct containing configuration parameters for Prism or it's back-ends to use.

#### Syntax

```c
typedef struct {
  uint8_t version;
  PrismRegistry *registry;
} PrismConfig;
```

#### Members

`version`

The version of this structure. This field MUST NOT be modified.

`registry`

The registry the created context will be bound to. This MAY be `NULL`, in which case the context uses the global registry. If non-null, it MUST be a registry obtained from `prism_registry_freeze`. This field was added in version 3 of this structure.

#### Remarks

This struct contains configuration information for Prism. The version field will be incremented by `1` whenever a new field is added or removed.

Certain fields in this structure may only be available on certain platforms. Attempting to read or write to them on platforms where these fields are not useful is a compilation error.

The `registry` field is how custom backends are made visible to a context. The context takes its own reference to the registry; the caller's reference is unaffected. Any number of contexts MAY be bound to the same registry, and all of them share that registry's backend cache.

### prism_config_init

Creates a new configuration structure which can be passed to `prism_init`.

#### Syntax

```c
PrismConfig prism_config_init(void);
```

#### Parameters

This function has no parameters.

#### Returns

A stack-allocated `PrismConfig` struct, returned by value.

#### Remarks

This function can be used to create a `PrismConfig` struct which can later be passed to `prism_init`. It is not an error to not call this function or to pass `NULL` to `prism_init`. This struct can be used to perform configuration of Prism before the library is initialized; for example, it is used to pass platform-specific data to the library for back-ends to use.

### prism_init

Creates a new Prism context.

#### Syntax

```c
PrismContext *prism_init(PrismConfig* cfg);
```

#### Parameters

`cfg`

The configuration struct created by `prism_config_init`. This MAY be `NULL`.

#### Return Value

Returns a pointer to a newly allocated `PrismContext` on success. Returns `NULL` if memory allocation fails or if the configuration's `version` field is greater than the version Prism was built against.

#### Remarks

`prism_init` is the entry point to the Prism library. Before calling any other Prism function (except `prism_error_string` and the functions to construct a registry for custom backends), an application MUST call `prism_init` to obtain a context.

Each call to `prism_init` creates an independent context. Multiple contexts MAY exist simultaneously. A context is bound at creation to the registry named by the configuration's `registry` field, or to the global registry when that field is `NULL` or no configuration is supplied at all. Contexts bound to the same registry share its cache: a cached backend instance created through one such context is visible from the others. Contexts bound to different registries share nothing.

Configurations from older versions of this library are accepted; fields that a caller's version of the structure does not contain are treated as absent. A configuration whose `version` is newer than the library itself is rejected, since the library cannot know what the unknown fields mean.

The returned context is owned by the caller. When the context is no longer needed, it MUST be passed to `prism_shutdown` to release resources. Failure to call `prism_shutdown` results in a memory leak.

Because `prism_init` only allocates a small control structure and obtains a reference to the global backend registry, it is a lightweight operation that is unlikely to fail except under severe memory pressure.

Applications typically call `prism_init` once at startup and `prism_shutdown` once at exit. However, there is no prohibition against creating and destroying contexts multiple times throughout the application's lifetime. However, note that doing so (or having multiple contexts) provides no actual benefit.

#### Example

```c
PrismContext *ctx = prism_init(NULL);
if (!ctx) {
    fprintf(stderr, "Failed to initialize Prism\n");
    return 1;
}
/* Use Prism... */
prism_shutdown(ctx);
```

### prism_shutdown

Destroys a Prism context and releases associated resources.

#### Syntax

```c
void prism_shutdown(PrismContext *ctx);
```

#### Parameters

`ctx`

The context to destroy. This parameter MAY be `NULL`.

#### Return Value

This function does not return a value.

#### Remarks

`prism_shutdown` releases all resources associated with a Prism context. After this function returns, the `ctx` pointer is invalid and MUST NOT be used for any purpose, including being passed to `prism_shutdown` again.

If `ctx` is `NULL`, this function has no effect. This allows applications to unconditionally call `prism_shutdown` in cleanup code without checking whether initialization succeeded.

This function does not automatically free backend instances that were obtained from the context. If any backends obtained via `prism_registry_create`, `prism_registry_create_best`, `prism_registry_acquire`, or `prism_registry_acquire_best` are still alive when `prism_shutdown` is called, those backends remain valid and may continue to be used. However, once `prism_shutdown` returns, no new backends can be obtained using that context.

The relationship between contexts and backends is asymmetric: contexts do not own backends. The registry the context was bound to is released when the context is destroyed; the global registry continues to exist as long as the process runs, while an application-constructed registry is finalized once the last context bound to it has been shut down and the application has released its own reference.

If multiple threads are using the same context, the application MUST ensure that no other thread is calling any Prism function on that context when `prism_shutdown` is called. Calling `prism_shutdown` while another thread is in the middle of a registry operation results in undefined behavior.

#### Example

```c
/* At application shutdown */
if (backend) {
    prism_backend_stop(backend);
    prism_backend_free(backend);
    backend = NULL;
}
prism_shutdown(ctx);
ctx = NULL;
```
