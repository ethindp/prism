## Shared Library Backends

This chapter specifies the mechanism through which an application loads custom backends from a shared library at run time, and the obligations each party assumes toward the other. A backend supplied in this way is a custom backend in every respect: every definition, guarantee, and requirement of the chapter on custom backends applies to it unchanged, and this chapter specifies only what is particular to delivery through a shared library.

### General

A plugin is a shared library that supplies custom backends to Prism at run time. Every plugin exports one function, the plugin entry point, named `prism_plugin_query`. Prism calls it to obtain backend descriptors.

A backend descriptor is a `PrismPluginBackend` structure. It names one backend and gives its priority, feature set, and vtable.

Loading is the sequence of operations Prism performs in one call to `prism_registry_builder_add_library`. Prism opens the library, resolves the entry point, obtains the descriptors, examines each, and adds a backend for each. This chapter describes that sequence; an implementation need only behave as if it carried the sequence out in this order.

Prism obtains a plugin's descriptors by calling the entry point with successive indices, beginning at zero. Enumeration stops at the first index for which the entry point returns `NULL`. A plugin MAY supply any number of backends, subject to the limits described under Implementation limits.

For each descriptor obtained, Prism adds a backend to the builder. The backend is added as though the application had called `prism_registry_builder_add_backend` with the descriptor's name, priority, feature set, and vtable. It is thereafter indistinguishable from a directly registered custom backend.

An application loads a plugin as follows:

1. It creates a builder with `prism_registry_builder_new`, or reuses one it is still populating.
2. It calls `prism_registry_builder_add_library`, naming the shared library.
3. Once the builder holds every backend the application wishes to register, it freezes the builder with `prism_registry_freeze`.
4. The registry is bound to one or more contexts through the `registry` field of `PrismConfig`.

A plugin's backends are added to a builder only. They are never present in the global registry.

Loading is atomic. If any part of it fails, no backend from the library is added, and the library is unloaded before `prism_registry_builder_add_library` returns. A partially loaded plugin is never observable.

### ABI compatibility

The plugin ABI comprises the layout and semantics of `PrismPluginHost`, of `PrismPluginBackend`, of the plugin entry point, and of every type they refer to. Two members of each structure govern compatibility: `abi_version` and `struct_size`. Both occupy fixed offsets in every generation, so either party can always read them.

The `abi_version` member gives an ABI generation. Prism increments it only for a change that is not backward compatible, such as reordering a member or changing the entry point's signature. Appending a member to the end of a structure does not increment it. The generation this version of Prism implements is `PRISM_PLUGIN_ABI_VERSION`.

The `struct_size` member gives the size of the structure as the party supplying it defines it. Within one generation, a structure MAY grow by the addition of members at its end. The reader consults at most `struct_size` bytes and treats members beyond that point as absent. This is the scheme `PrismBackendVTable` uses through its own `size` member.

Compatibility is settled during loading, and either side MAY refuse. A plugin that requires a newer host than the one presented declines by returning `NULL` at index 0, and SHOULD first record its reason through the host descriptor's `log` member. Prism rejects a descriptor whose generation exceeds `PRISM_PLUGIN_ABI_VERSION`, or falls below the oldest generation it accepts.

A plugin declaring a generation Prism accepts MUST have been compiled against a layout-compatible definition of every shared structure. The two definitions may differ only in members appended beyond `struct_size`.

### Host services

Prism gives every backend it registers from a plugin a services object. A services object is a `PrismPluginServices` structure, particular to one backend, through which that backend reaches the facilities Prism offers it.

A backend receives its services object through the instance context passed to its `create` member. A backend supplied by a plugin MUST therefore provide a `create` member. The convention by which a registration's `userdata` serves as the instance pointer directly is not available to it.

Every function of a services object takes that object as its first argument. Prism thereby supplies, on the backend's behalf, what it would otherwise require the backend to state. The source a diagnostic is recorded under is such a value.

A services object is valid from the moment its backend is registered until the library has been unloaded. Prism releases it only after unloading completes, so code the library runs while unloading MAY still use it. A backend MAY retain its services object throughout that period.

A plugin MUST NOT call the functions Prism exports. It reaches Prism through the host descriptor and through its backends' services objects. A plugin therefore uses only the type and macro definitions of the Prism header, and does not link the library.

### Threads and apartments

Prism calls the plugin entry point only on the thread that calls `prism_registry_builder_add_library`, only during that call, and never concurrently. The entry point MUST NOT assume any particular thread, COM apartment, or platform main context. It MUST NOT retain the host descriptor beyond the call.

The `is_supported` member of a plugin's vtable MAY be invoked from Prism's internal availability poll thread, described in the chapter on background availability enumeration. It MAY also be invoked from application threads. The poll thread is not owned by the application. A plugin MUST NOT assume that it has entered any particular COM apartment, that any platform main context is current on it, or that it is the thread on which the instance was created.

Some plugins wrap a resource with thread affinity, such as a COM object with apartment affinity or a handle bound to an event loop. Such a plugin MUST confine its use of that resource within `is_supported` to the single call, for example by acquiring and releasing it there. An `is_supported` that relies on affinity established on another thread results in undefined behavior.

Every member of the vtable other than `is_supported` is subject to the single-instance constraint given in the chapter on thread safety.

### Library lifetime

Prism opens the library once and shares it among every backend registered from it. The library remains loaded for as long as any registration derived from it remains referenced, as that term is defined in the chapter on custom backends.

A descriptor's vtable function pointers reside in the library's image, as does the name string Prism copies during loading. They remain valid for as long as the library remains loaded. Unloading the image while a registration referring to it remains leaves dangling function pointers that Prism may later invoke, and results in undefined behavior.

Whether Prism unloads the library once no registration derived from it remains referenced is unspecified, as is whether the platform honors the request. A plugin MUST NOT assume that unloading happens at any particular moment, and MUST tolerate a platform that does not unload the library at all.

Prism does not free the object a descriptor's `userdata` member designates. A plugin requiring that object to be released at a determined time SHOULD tie the release to the library's own unloading, through whatever mechanism the platform provides.

### Implementation limits

This chapter describes the model Prism presents to a plugin. It does not oblige Prism to accept every plugin the model admits.

Prism MAY impose limits on what a plugin supplies. The number of descriptors one plugin may offer, and the length of a backend's name, are limits of this kind. A plugin that exceeds a limit is rejected, and loading fails as it does for any other malformed plugin. Exceeding a limit does not result in undefined behavior. Which limits Prism imposes, and whether it imposes any, is unspecified.

Prism MAY refuse to load a library for reasons this chapter does not describe. Code signing requirements, sandbox restrictions, loader policy, and Prism's own security posture are reasons of this kind. A refusal on any such ground is reported as a load failure and is not distinguished from any other.

Lastly, Prism MAY examine a descriptor more strictly than this chapter requires, and MAY reject one this chapter does not require it to reject. Prism does not accept a descriptor this chapter requires it to reject.

### Security and deployment

Opening a shared library runs that library's initialization code before Prism reads any descriptor. Static constructors, load notifications, and image initialization routines all run at that point. An application MUST treat `prism_registry_builder_add_library` as equivalent to running code of the library's choosing. An application MUST NOT load a library from a location it does not trust.

The validation Prism performs on descriptors guards the registry against a malformed plugin. It is not a security boundary. It does not constrain code that has already run.

Loading code at run time is restricted on some platforms and prohibited on others. Application store policies commonly forbid it, and sandboxed environments commonly prevent it. An application SHOULD establish that this mechanism is permitted where it will be deployed before relying on it. An application that cannot rely on it registers its backends directly instead, as described in the chapter on custom backends.

### PrismPluginHost

A structure describing the loading Prism library, passed to a plugin's entry point.

#### Syntax

```c
typedef struct PrismPluginHost {
  uint64_t abi_version;
  uint32_t struct_size;
  uint32_t reserved;
  void(PRISM_CALL *log)(const PrismPluginHost *self, PrismLogLevel level,
                        const char *message);
} PrismPluginHost;
```

#### Members

`abi_version`

The plugin ABI generation this implementation provides, equal to the `PRISM_PLUGIN_ABI_VERSION` against which Prism was compiled. A plugin MAY compare this value against its own requirements and decline to supply backends if it requires a newer host.

`struct_size`

The size of this structure as Prism understands it. A plugin MUST consult at most this many bytes and MUST treat any member beyond it as absent.

`reserved`

Reserved for future use. Prism sets this member to zero, and a plugin MUST ignore it.

`log`

A function through which the plugin MAY record diagnostics during the entry point call. Its first argument MUST be the host descriptor Prism supplied; the plugin does not supply a source, and Prism records the diagnostic under a source that names the library being loaded. This member is never `NULL`. A plugin SHOULD use it to state the reason for declining a host, and MUST NOT retain the pointer beyond the entry point call.

### PrismPluginBackend

A descriptor supplied by a plugin to describe a single backend.

#### Syntax

```c
typedef struct PrismPluginBackend {
  uint64_t abi_version;
  uint32_t struct_size;
  uint32_t reserved;
  const char *name;
  int priority;
  uint64_t features;
  const PrismBackendVTable *vtable;
  void *userdata;
  uint64_t plugin_version;
} PrismPluginBackend;
```

#### Members

`abi_version`

The plugin ABI generation this descriptor was built against. A plugin MUST set this member to the `PRISM_PLUGIN_ABI_VERSION` against which it was compiled. Prism rejects a descriptor whose generation it does not accept, as described under ABI compatibility.

`struct_size`

The size of this structure as the plugin understands it. A plugin MUST set this member to `sizeof(PrismPluginBackend)`. Prism consults at most this many bytes.

`reserved`

Reserved for future use. A plugin MUST set this member to zero.

`name`

The backend's name, as a null-terminated UTF-8 string, subject to the same requirements and consequences as the `name` parameter of `prism_registry_builder_add_backend`. The backend's identifier is derived from it by the hash function described in the chapter on backend identifiers. The string is copied during loading.

`priority`

The backend's priority. Higher values indicate higher priority. This value MUST be non-negative unless the loading call supplies a priority override, in which case it is ignored.

`features`

The feature set the backend declares, formed by ORing `PRISM_BACKEND_*` feature constants together. It MUST be consistent with the vtable in the sense required of any custom backend: a feature constant that designates an operation MUST be declared if and only if the corresponding vtable member is non-null.

`vtable`

The vtable implementing the backend, subject to every requirement placed on a vtable by the chapter on custom backends. This member MUST NOT be `NULL`, and its `size` member MUST be set as that chapter requires. The vtable MUST provide a `create` member, as described under Host services. The vtable is copied during loading; the code it names remains valid while the library remains loaded.

`userdata`

An opaque pointer that Prism delivers to the backend's `create` member as the `userdata` member of a `PrismPluginInstanceContext` (see PrismPluginInstanceContext). It has the meaning the `userdata` parameter of `prism_registry_builder_add_backend` would have, and permits several descriptors that share one vtable to be distinguished at instance creation. This member MAY be `NULL`. Prism does not free it.

`plugin_version`

An informational version identifying the plugin's own release, distinct from the ABI generation and used for no compatibility decision. The value is three 16-bit components, major, minor, and patch, packed most-significant first, with the least-significant 16 bits reserved and set to zero. A plugin MAY set it to zero if it has no version to report.

### PrismPluginServices

A structure through which a backend supplied by a plugin reaches the facilities Prism makes available to it. Prism provides one to each backend it registers from a plugin, as described under Host services.

#### Syntax

```c
typedef struct PrismPluginServices {
  uint32_t struct_size;
  uint32_t reserved;
  void(PRISM_CALL *log)(const PrismPluginServices *self, PrismLogLevel level,
                        const char *message);
} PrismPluginServices;
```

#### Members

`struct_size`

The size of this structure as Prism understands it. A backend MUST consult at most this many bytes and MUST treat any member beyond it as absent. A later generation MAY append members after those defined here.

`reserved`

Reserved for future use. Prism sets this member to zero, and a backend MUST ignore it.

`log`

A function through which the backend records diagnostics. Its first argument MUST be the services object Prism supplied. Prism records the diagnostic under the name of the backend to which the services object belongs; the backend does not supply a source. This member is never `NULL`.

### PrismPluginInstanceContext

The structure Prism passes as the `userdata` argument to the `create` member of a backend supplied by a plugin. The context is valid only for the duration of the `create` call; the services object it names is valid for the longer period given under Host services.

#### Syntax

```c
typedef struct PrismPluginInstanceContext {
  uint32_t struct_size;
  uint32_t reserved;
  const PrismPluginServices *services;
  void *userdata;
} PrismPluginInstanceContext;
```

#### Members

`struct_size`

The size of this structure as Prism understands it. A backend MUST consult at most this many bytes and MUST treat any member beyond it as absent.

`reserved`

Reserved for future use. Prism sets this member to zero, and a backend MUST ignore it.

`services`

The backend's services object. A backend MAY retain it for the period given under Host services. This member is never `NULL`.

`userdata`

The value of the `userdata` member of the descriptor from which the backend was registered.

### prism_plugin_query

The function a plugin exports, through which Prism enumerates the plugin's backends. Every plugin MUST export this function.

#### Syntax

```c
const PrismPluginBackend *prism_plugin_query(const PrismPluginHost *host,
                                             size_t index);
```

#### Parameters

`host`

A pointer to a host descriptor Prism supplies. This pointer is never `NULL` and is valid only for the duration of the call.

`index`

The zero-based index of the descriptor Prism requests. Prism begins at zero and increments by one on each successive call within a single load.

#### Return Value

Returns a pointer to the backend descriptor at `index` when one exists. Returns `NULL` when `index` is past the last descriptor the plugin supplies, which terminates enumeration. A plugin returns `NULL` at index 0 to supply no backends, whether because it has none to offer or because it declines the presented host.

#### Remarks

The returned descriptor MUST remain valid until the entry point is next called or loading returns, whichever comes first. A plugin typically returns a pointer to a static descriptor, which satisfies this trivially. The vtable a descriptor names, and any object its `userdata` designates, are subject to the longer lifetimes described under Library lifetime.

A plugin MUST return descriptors for a contiguous range of indices beginning at zero. If the entry point returns `NULL` for one index and a non-null descriptor for a greater index, the enumeration SHALL be incomplete.

A plugin MUST export this function with C language linkage, the `PRISM_CALL` calling convention, and the external visibility the platform requires of a dynamically resolved symbol: `__declspec(dllexport)` or a module-definition file on Windows, and default symbol visibility elsewhere. The entry point is subject to the requirements given under Threads and apartments.

### prism_registry_builder_add_library

Loads a plugin from a shared library and adds each backend it supplies to a registry builder.

#### Syntax

```c
PrismError prism_registry_builder_add_library(PrismRegistryBuilder *builder,
                                              const char *path,
                                              int priority_override,
                                              size_t *out_count);
```

#### Parameters

`builder`

The builder to add the plugin's backends to. This parameter MUST NOT be `NULL`.

`path`

The filesystem path to the shared library, as a null-terminated UTF-8 string. This parameter MUST NOT be `NULL`. On platforms whose native path encoding is not UTF-8, Prism converts the path as the platform requires. The string is not retained after the call.

`priority_override`

A priority to assign to every backend the plugin supplies, overriding the priority each declares. A non-negative value is applied to all of the plugin's backends. A negative value directs Prism to honor the priority each descriptor declares, which then MUST itself be non-negative.

`out_count`

An optional pointer receiving the number of backends added. This parameter MAY be `NULL`. It is written only when the function returns `PRISM_OK`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | The plugin was loaded and all of its backends were added to the builder. |
| `PRISM_ERROR_INVALID_PARAM` | `priority_override` was negative and a descriptor declared a negative priority; or a descriptor declared a feature set inconsistent with its vtable, a null vtable, a vtable whose `size` member was zero, or a vtable without a `create` member. |
| `PRISM_ERROR_INVALID_UTF8` | `path`, or a descriptor's name, contains invalid UTF-8. |
| `PRISM_ERROR_LIBRARY_LOAD_FAILED` | The shared library could not be opened. Among the causes are that no file exists at `path`, that it is not a loadable image, that it was built for a different architecture, and that its initialization code failed. |
| `PRISM_ERROR_LIBRARY_INVALID` | The library was opened but does not export `prism_plugin_query`. |
| `PRISM_ERROR_INCOMPATIBLE_ABI` | The plugin declined the host by returning `NULL` at index 0, or a descriptor declared an ABI generation this implementation does not accept. |
| `PRISM_ERROR_INVALID_OPERATION` | The builder is spent, or a backend the plugin supplies has the same name or identifier as a backend already present in the builder, whether compiled-in, previously registered, or supplied by an earlier index of the same plugin. |
| `PRISM_ERROR_MEMORY_FAILURE` | Memory allocation failed. |

#### Remarks

Loading is atomic. On any error, no backend from the plugin is added, and the library is unloaded before the function returns. On success, one backend is added for each descriptor the plugin supplied, in the order the plugin supplied them, and `out_count`, if provided, receives their number.

The library is opened once and shared among every backend loaded from it. Prism unloads it when the last registration derived from the plugin becomes unreferenced, as defined in the chapter on custom backends.

The identifiers Prism assigns to the loaded backends are those their names hash to. An application that knows a plugin's backend names can resolve their identifiers from the frozen registry with `prism_registry_id`; an application that does not can enumerate them with `prism_registry_count` and `prism_registry_id_at`.

Opening a shared library executes that library's initialization code before Prism reads any descriptor or performs any validation, as described under Security and deployment.

#### Example

The following source, compiled as a shared library, is a minimal plugin that supplies a single backend. The backend takes its services object from the instance context in `create`, records a diagnostic through it, and writes speech requests to standard output.

```c
#include <prism.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define DEMO_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define DEMO_EXPORT __attribute__((visibility("default")))
#else
#define DEMO_EXPORT
#endif

typedef struct {
    const PrismPluginServices *services;
} demo_instance;

static void *PRISM_CALL demo_create(void *userdata) {
    const PrismPluginInstanceContext *ctx = userdata;
    demo_instance *self = calloc(1, sizeof *self);
    if (!self) {
        return NULL;
    }
    self->services = ctx->services;
    self->services->log(self->services, PRISM_LOG_LEVEL_INFO, "instance created");
    return self;
}

static void PRISM_CALL demo_destroy(void *instance) {
    free(instance);
}

static PrismError PRISM_CALL demo_speak(void *instance, const char *text,
                                        bool interrupt) {
    demo_instance *self = instance;
    (void)interrupt;
    self->services->log(self->services, PRISM_LOG_LEVEL_TRACE, text);
    printf("speak: %s\n", text);
    return PRISM_OK;
}

static PrismError PRISM_CALL demo_stop(void *instance) {
    (void)instance;
    return PRISM_OK;
}

static const PrismBackendVTable demo_vtable = {
    .size = sizeof(PrismBackendVTable),
    .create = demo_create,
    .destroy = demo_destroy,
    .speak = demo_speak,
    .stop = demo_stop,
};

static const PrismPluginBackend demo_backend = {
    .abi_version = PRISM_PLUGIN_ABI_VERSION,
    .struct_size = sizeof(PrismPluginBackend),
    .reserved = 0,
    .name = "Demo",
    .priority = 10,
    .features = PRISM_BACKEND_SUPPORTS_SPEAK | PRISM_BACKEND_SUPPORTS_STOP,
    .vtable = &demo_vtable,
    .userdata = NULL,
    .plugin_version = 0,
};

DEMO_EXPORT const PrismPluginBackend *prism_plugin_query(
    const PrismPluginHost *host, size_t index) {
    (void)host;
    return index == 0 ? &demo_backend : NULL;
}
```

The following loads that plugin and freezes a registry containing it alongside the compiled-in backends.

```c
PrismRegistryBuilder *builder = prism_registry_builder_new();
if (!builder) {
    return 1;
}
size_t count = 0;
PrismError err =
    prism_registry_builder_add_library(builder, "./demo_plugin.so", -1, &count);
if (err != PRISM_OK) {
    fprintf(stderr, "Failed to load plugin: %s\n", prism_error_string(err));
    prism_registry_builder_free(builder);
    return 1;
}
PrismRegistry *registry = prism_registry_freeze(builder);
prism_registry_builder_free(builder);
if (!registry) {
    return 1;
}
/* Bind the registry to a context as shown in the chapter on custom backends. */
```