## Background Availability Enumeration

The set of backends registered in a registry is fixed, but whether a given backend is usable at this moment is not. A screen reader may start or stop while an application is running, and Prism reflects this through the runtime availability bit `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`, which a backend reports through its feature flags. Because an application holds backend instances directly rather than routing every call through the library, a backend that was available when it was acquired may quietly become unavailable, and nothing in the backend portion of the API, besides the `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` bit, will announce that change, unless the application explicitly asks for this information.

Background availability enumeration closes that gap. When configured, a Prism context runs an internal thread that periodically samples each backend's runtime availability and invokes an application-supplied callback whenever a backend transitions between available and unavailable. The feature is opt-in: a context whose configuration supplies no availability callback runs no such thread and incurs no cost.

The polling thread, the callback, and the sampling policy are all configured through the `PrismConfig` structure passed to `prism_init`; the relevant members are described in the chapter on context management. This chapter describes the callback type, the sampling model, and the functions that control the polling thread at runtime.

### Sampling model

The polling thread performs a scan at a configurable interval. In each scan it samples the runtime availability of every backend in the registry and compares each sample against the last state it confirmed for that backend. The model has the following properties:

1. The callback is invoked only when a backend's confirmed availability changes. A backend that remains available or remains unavailable across many scans produces no callbacks.
2. The first scan after the thread starts establishes a baseline without invoking the callback. A backend that is already available when the context is created therefore does not produce a spurious notification. An application that needs to know the initial availability of a backend MUST query it directly.
3. A transition is confirmed only after the new state has been observed on a configurable number of consecutive scans. This absorbs momentary glitches that would otherwise produce a pair of spurious notifications.
4. When configured with an upper bound above the base interval, the sampling interval grows while availability is unchanging and returns to the base interval the instant any sample disagrees with the confirmed state or a transition is confirmed. Backoff reduces the frequency of wakeups during long periods of inactivity without delaying the detection of a change once one begins to occur.
5. The interval between scans is realized using the most efficient timer facility the platform provides, and the thread permits the operating system to align its wakeups with other timer activity. This allows a mostly-idle poll to avoid forcing dedicated wakeups. Coalescing applies whether or not backoff is enabled.


### `PrismAvailabilityCallback`

The type of a function invoked when a backend's runtime availability changes.

#### Syntax

```c
typedef void(PRISM_CALL *PrismAvailabilityCallback)(void *userdata,
                                                    PrismBackendId backend,
                                                    const char *name,
                                                    bool available);
```

#### Parameters

`userdata`

The opaque pointer supplied as `availability_userdata` in the `PrismConfig` that configured polling. Prism does not interpret this value.

`backend`

The identifier of the backend whose availability changed.

`name`

The name of the backend, as a null-terminated string. This string points into the registry and is valid only for the duration of the call.

`available`

`true` if the backend has become available, `false` if it has become unavailable.

#### Remarks

The callback is invoked from Prism's internal poll thread, not from a thread owned by the application. Callback implementations MUST provide their own synchronization if they touch shared state. A callback SHOULD do as little work as possible and MUST NOT block: the poll thread cannot perform further scans until the callback returns.

The callback MUST NOT call `prism_shutdown` on the context that owns the poll thread. Shutting a context down joins its poll thread, so a callback that does this deadlocks against itself. Read-only registry queries and backend acquisition on the same context are safe to call re-entrantly from within the callback.

The `name` pointer is owned by Prism and is valid only for the duration of the call; therefore, a callback that needs to retain it MUST copy it. The value passed as `backend` is a stable identifier and MAY be retained freely.

An application typically responds to a callback by discarding a backend instance it can no longer use and, when a preferred backend becomes available, acquiring it. The callback is a notification that the application's cached choice of backend may be stale, and does not itself change any backend instance the application holds.

### Interaction with pausing

The poll thread MAY be paused and resumed at runtime, either automatically in response to operating-system power transitions or under explicit application control. While paused, the thread performs no scans and consumes no processor time.

When the thread resumes, it performs an immediate re-synchronizing scan rather than waiting for the next interval. Because an arbitrary amount of time may have passed while the thread was paused, this scan is not debounced: any backend whose availability differs from the state last reported to the application produces a callback at once. Pausing and resuming therefore never causes a real change to be missed, though a change that occurs and then reverses entirely within a paused interval is not reported, since only the net difference is observed on resume.

### prism_availability_poll_pause

Pauses the availability poll thread.

#### Syntax

```c
void prism_availability_poll_pause(PrismContext *ctx);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

#### Return Value

This function does not return a value.

#### Remarks

This function suspends availability polling for the given context. While paused, the poll thread parks and consumes no processor time. A scan already in progress when this function is called is allowed to complete; the pause takes effect at the following scan.

This function is safe to call from any thread. It is a no-op if the context was not configured with an availability callback or if polling is already paused.

Pausing is intended for applications that wish to suppress polling while they are backgrounded or otherwise idle, particularly on platforms where automatic power management is unavailable (see `prism_availability_auto_power_supported`). Such applications typically drive `prism_availability_poll_pause` and `prism_availability_poll_resume` from operating-system lifecycle events.

### prism_availability_poll_resume

Resumes the availability poll thread.

#### Syntax

```c
void prism_availability_poll_resume(PrismContext *ctx);
```

#### Parameters

`ctx`

The Prism context. This parameter MUST NOT be `NULL`.

#### Return Value

This function does not return a value.

#### Remarks

This function resumes availability polling for a context previously paused with `prism_availability_poll_pause`. On resume, the poll thread performs an immediate re-synchronizing scan and invokes the availability callback for every backend whose availability differs from the state last reported, without debouncing, as described in the section on interaction with pausing. The sampling interval is reset to its base value.

This function is safe to call from any thread. It is a no-op if the context was not configured with an availability callback or if polling is not paused.

### prism_availability_auto_power_supported

Reports whether this build can pause and resume polling automatically in response to operating-system power transitions.

#### Syntax

```c
bool prism_availability_auto_power_supported(void);
```

#### Parameters

This function has no parameters.

#### Return Value

Returns `true` if the `availability_auto_power_manage` configuration option is honored on this build, and `false` otherwise.

#### Remarks

When this function returns `true`, setting `availability_auto_power_manage` in `PrismConfig` causes Prism to pause the poll thread when the system suspends and resume it when the system wakes, with no further action required from the application.

When it returns `false`, the option has no effect. This is the case on platforms where a library-level component cannot observe power transitions, and on builds compiled without power-management support. On such platforms, an application that wishes to avoid polling while the machine is unattended MUST drive `prism_availability_poll_pause` and `prism_availability_poll_resume` itself, from whatever lifecycle notifications it receives from the operating system.

This function reflects a compile-time and platform property and MAY be called at any time, including before `prism_init`.
