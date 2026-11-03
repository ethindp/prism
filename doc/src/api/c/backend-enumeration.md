## Background Availability Enumeration

The set of backends registered in a registry is fixed, but whether a given backend is usable at this moment is not. A screen reader may start or stop while an application is running, and Prism reflects this through the runtime availability bit `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`, which a backend reports through its feature flags. Because an application holds backend instances directly rather than routing every call through the library, a backend that was available when it was acquired may quietly become unavailable, and nothing in the backend portion of the API, besides the `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` bit, will announce that change, unless the application explicitly asks for this information.

Background availability enumeration closes that gap. When configured, a Prism context runs an internal thread that periodically samples each backend's runtime availability and invokes an application-supplied callback whenever a backend transitions between available and unavailable. The feature is opt-in: a context whose configuration supplies no availability callback runs no such thread and incurs no cost.

The polling thread, the callback, and the sampling policy are all configured through the `PrismConfig` structure passed to `prism_init`; the relevant members are described in the chapter on context management. This chapter describes the callback type, the sampling model, and the functions that control the polling thread at runtime.

### Sampling model

The polling thread performs a scan at a configurable interval. In each scan it samples the runtime availability of every backend in the registry and compares each sample against the last state it confirmed for that backend. The model has the following properties:

1. The callback is invoked only when a backend's confirmed availability changes. A backend that remains available or remains unavailable across many scans produces no callbacks.
2. The first scan after the thread starts establishes a baseline without invoking the availability callback. A backend that is already available when the context is created therefore does not produce a spurious notification. An application that needs to know the initial availability of a backend MUST query it directly. The first scan runs on the poll thread after `prism_init` has returned, so it is not ordered with respect to such direct queries. A change that occurs between a direct query and the first scan's sample of the same backend becomes part of the baseline and is never reported. The section on `PrismAvailabilityBaselineCallback` describes how an application detects this condition.
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

### `PrismAvailabilityBaselineCallback`

The type of a function invoked once the poll thread has established its baseline.

#### Syntax

```c
typedef void(PRISM_CALL *PrismAvailabilityBaselineCallback)(void *userdata);
```

#### Parameters

`userdata`

The opaque pointer supplied as `availability_userdata` in the `PrismConfig` that configured polling. Prism does not interpret this value.

#### Remarks

The baseline callback is invoked exactly once per context, on the poll thread, after the first scan has completed and before the first invocation of the availability callback. The first scan and the baseline callback occur even if polling is paused before the first scan begins. The baseline callback is not invoked if the context is shut down before the first scan completes.

The baseline callback conveys no information about any backend. It identifies neither which backends were sampled nor the state observed for any of them. Its only meaning is that the baseline now exists, which fixes the point from which the availability callback reports changes.

During the first scan, the poll thread samples each backend once, one backend after another, and records the sample as that backend's confirmed state. A backend that cannot be instantiated, or whose sample cannot be taken, retains a confirmed state of unavailable. After the baseline callback has been invoked, any change in a backend's availability relative to its confirmed state is reported through the availability callback, subject to the debouncing described in the section on the sampling model and to the rules described in the section on interaction with pausing. A change that occurred before the first scan sampled a particular backend is not reported, because the sample already reflects it.

The baseline callback is therefore useful only to an application that meets both of the following conditions:

1. Before the baseline callback is invoked, the application observes the availability of one or more backends directly, for example by testing `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` in the result of `prism_backend_get_features`, and retains a decision derived from that observation, such as which backend to use.
2. The application relies on the availability callback, rather than on observations made each time the decision is used, to learn when that decision has become stale.

Such an application cannot determine from the availability callback alone whether a backend changed state between its observation and the first scan's sample of that backend. When the baseline callback is invoked, the application SHOULD repeat every direct observation made before the callback that underlies a retained decision, and revise the decision accordingly. Because the callback does not identify any backend, no observation can be omitted on the grounds that the corresponding backend did not change. A repeated observation is made after the first scan sampled the backend in question, so any later change relative to the baseline is reported through the availability callback. The application MAY consequently receive a notification for a transition that it has already observed through a repeated observation, and SHOULD treat such a notification as harmless.

An application that makes no direct observations before the baseline callback is invoked, or that does not retain decisions derived from them, or that observes availability each time it acts on it, has no use for the baseline callback and MAY leave `availability_baseline_callback` set to `NULL`. An application MAY also avoid the condition entirely by deferring its first direct observation until the baseline callback has been invoked, at the cost of waiting for the first scan to complete.

The constraints that apply to the availability callback apply equally to the baseline callback: it MUST NOT block, it MUST synchronize any shared state that it accesses, and it MUST NOT call `prism_shutdown` on the owning context. Because the baseline callback runs on the poll thread, it MUST NOT repeat observations on a backend instance that is used from another thread without external synchronization, as described in the chapter on thread safety. An application typically records in the callback that its observations are to be repeated, and repeats them on the thread that owns the affected backend instances.

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
