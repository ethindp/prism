## Context Management

### prism_init

Creates a new Prism context.

#### Syntax

```c
PrismContext *prism_init(void);
```

#### Parameters

This function has no parameters.

#### Return Value

Returns a pointer to a newly allocated `PrismContext` on success. Returns `NULL` if memory allocation fails.

#### Remarks

`prism_init` is the entry point to the Prism library. Before calling any other Prism function (except `prism_error_string`), an application MUST call `prism_init` to obtain a context.

Each call to `prism_init` creates an independent context. Multiple contexts MAY exist simultaneously, and each context provides access to the same global backend registry, including its cache. A cached backend instance created through one context is visible from all other contexts.

The returned context is owned by the caller. When the context is no longer needed, it MUST be passed to `prism_shutdown` to release resources. Failure to call `prism_shutdown` results in a memory leak.

Because `prism_init` only allocates a small control structure and obtains a reference to the global backend registry, it is a lightweight operation that is unlikely to fail except under severe memory pressure.

Applications typically call `prism_init` once at startup and `prism_shutdown` once at exit. However, there is no prohibition against creating and destroying contexts multiple times throughout the application's lifetime. However, note that doing so (or having multiple contexts) provides no actual benefit.

#### Example

```c
PrismContext *ctx = prism_init();
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

The relationship between contexts and backends is asymmetric: contexts do not own backends, but the underlying backend registry (which is global) continues to exist as long as the process runs. Applications that wish to ensure all speech is stopped and all resources are released SHOULD explicitly call `prism_backend_stop` and `prism_backend_free` on all backends before calling `prism_shutdown`.

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
