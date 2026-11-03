## Prism Singleton

The global `Prism` singleton is an instance of `GodotPrismContext`. It owns a context bound to the global registry, or to a registry containing plugins once [`load_backend_library`](#load_backend_library) has succeeded.

### Constants

#### `PrismBackendId`

The identifiers of the backends, corresponding to the constants described in the C API's chapter on [backend identifiers](../c/backend-identifiers.md). They are accessed through the class, as in `GodotPrismContext.ORCA`.

`INVALID`, `SAPI`, `AV_SPEECH`, `VOICE_OVER`, `SPEECH_DISPATCHER`, `NVDA`, `JAWS`, `ONE_CORE`, `ORCA`, `ANDROID_TTS`, `ANDROID_SCREEN_READER`, `WEB_SPEECH`, `UIA`, `ZDSR`, `ZOOM_TEXT`, `BOY_PC_READER`, `PC_TALKER`, `SENSE_READER`, `SYSTEM_ACCESS`, `WINDOW_EYES`, `SPIEL`, and `GDK_SPEECH`.

#### `PrismLogLevel`

The severities described under [`PrismLogLevel`](../c/logging.md#prismloglevel), accessed as in `GodotPrismContext.PRISM_LOG_LEVEL_WARN`.

`PRISM_LOG_LEVEL_TRACE`, `PRISM_LOG_LEVEL_DEBUG`, `PRISM_LOG_LEVEL_INFO`, `PRISM_LOG_LEVEL_WARN`, `PRISM_LOG_LEVEL_ERROR`, and `PRISM_LOG_LEVEL_NONE`.

### Properties

#### `backends_count`

```gdscript
var backends_count: int
```

The number of backends in the registry. This read-only property is equivalent to `get_backends_count`.

### Signals

#### `backend_availability_changed`

Emitted when a backend's availability changes, while monitoring is active. This signal corresponds to [`PrismAvailabilityCallback`](../c/backend-enumeration.md#prismavailabilitycallback).

```gdscript
signal backend_availability_changed(id: int, name: String, available: bool)
```

#### `availability_baseline_established`

Emitted once for each period of monitoring, when the baseline has been established. This signal corresponds to [`PrismAvailabilityBaselineCallback`](../c/backend-enumeration.md#prismavailabilitybaselinecallback), whose remarks describe when it is useful.

```gdscript
signal availability_baseline_established()
```

#### `log_message_received`

Emitted for each log message, while enabled through [`set_log_signal_enabled`](#set_log_signal_enabled). This signal corresponds to [`PrismLogCallback`](../c/logging.md#prismlogcallback).

```gdscript
signal log_message_received(level: int, source: String, message: String)
```

### Methods

### get_backends_count

Returns the number of backends in the registry. This method wraps [`prism_registry_count`](../c/registry-functions.md#prism_registry_count).

#### Syntax

```gdscript
func get_backends_count() -> int
```

### id_at

Returns the identifier of the backend at a position in the registry. This method wraps [`prism_registry_id_at`](../c/registry-functions.md#prism_registry_id_at).

#### Syntax

```gdscript
func id_at(index: int) -> int
```

#### Parameters

`index`

The position in the registry.

#### Return Value

Returns the identifier of the backend. Returns `INVALID` if `index` is out of range.

### id_of

Returns the identifier of the backend with a given name. This method wraps [`prism_registry_id`](../c/registry-functions.md#prism_registry_id).

#### Syntax

```gdscript
func id_of(name: String) -> int
```

#### Parameters

`name`

The name of the backend.

#### Return Value

Returns the identifier of the backend. Returns `INVALID` if `name` is empty or no backend has that name.

### name_of

Returns the name of a backend. This method wraps [`prism_registry_name`](../c/registry-functions.md#prism_registry_name).

#### Syntax

```gdscript
func name_of(id: int) -> String
```

#### Parameters

`id`

The identifier of the backend, which MUST be in the registry unless it is `INVALID`.

#### Return Value

Returns the name of the backend. Returns an empty string if `id` is `INVALID` or is not in the registry.

### priority_of

Returns the priority of a backend. This method wraps [`prism_registry_priority`](../c/registry-functions.md#prism_registry_priority).

#### Syntax

```gdscript
func priority_of(id: int) -> int
```

#### Parameters

`id`

The identifier of the backend.

#### Return Value

Returns the value returned by the C function.

### has

Reports whether a backend is in the registry. This method wraps [`prism_registry_exists`](../c/registry-functions.md#prism_registry_exists).

#### Syntax

```gdscript
func has(id: int) -> bool
```

#### Parameters

`id`

The identifier of the backend.

#### Return Value

Returns `true` if the backend is in the registry, and `false` otherwise.

### get_available_backends

Enumerates the backends in the registry.

#### Syntax

```gdscript
func get_available_backends() -> Array[Dictionary]
```

#### Return Value

Returns one `Dictionary` for each backend, in registry order, with the keys `"id"`, `"name"`, and `"priority"`.

#### Remarks

Despite its name, this method reports membership in the registry and does not indicate that a backend can be used. Use [`is_backend_available`](#is_backend_available) for that purpose.

### is_backend_available

Reports whether a backend is presently usable, without initializing it.

#### Syntax

```gdscript
func is_backend_available(id: int) -> bool
```

#### Parameters

`id`

The identifier of the backend.

#### Return Value

Returns `true` if a temporary instance of the backend reports `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`, and `false` otherwise, including when the backend is not in the registry.

#### Remarks

The result is advisory, as described in the chapter on [backend-specific notes](../c/backend-notes.md).

### get_backend

Returns a cached backend instance if one exists. This method wraps [`prism_registry_get`](../c/registry-functions.md#prism_registry_get).

#### Syntax

```gdscript
func get_backend(id: int) -> GodotPrismBackend
```

#### Parameters

`id`

The identifier of the backend.

#### Return Value

Returns the backend, or `null` if no cached instance exists.

### create

Creates and initializes a new instance of a backend. This method wraps [`prism_registry_create`](../c/registry-functions.md#prism_registry_create) and `prism_backend_initialize`.

#### Syntax

```gdscript
func create(id: int) -> GodotPrismBackend
```

#### Parameters

`id`

The identifier of the backend.

#### Return Value

Returns the backend, or `null` if it could not be created.

#### Remarks

If initialization fails, the failure is reported through Godot's error log and the backend is still returned. Its operations then return `PRISM_ERROR_NOT_INITIALIZED`. Applications that have not chosen a specific backend SHOULD use [`create_best`](#create_best).

### create_best

Creates the highest-priority backend able to initialize. This method wraps [`prism_registry_create_best`](../c/registry-functions.md#prism_registry_create_best).

#### Syntax

```gdscript
func create_best() -> GodotPrismBackend
```

#### Return Value

Returns the backend, or `null` if no backend could be initialized.

#### Example

```gdscript
extends Node

var backend: GodotPrismBackend

func _ready() -> void:
	backend = Prism.create_best()
	if backend == null:
		push_error("No speech backend is available")
		return
	var err := backend.output("Hello from Prism.")
	if err != GodotPrismBackend.PRISM_OK:
		push_error("Output failed: %s" % Prism.get_error_string(err))
```

### acquire

Acquires a cached or new instance of a backend and initializes it. This method wraps [`prism_registry_acquire`](../c/registry-functions.md#prism_registry_acquire) and `prism_backend_initialize`.

#### Syntax

```gdscript
func acquire(id: int) -> GodotPrismBackend
```

#### Parameters

`id`

The identifier of the backend.

#### Return Value

Returns the backend, or `null` if it could not be obtained. The remarks under [`create`](#create) apply.

### acquire_best

Acquires the highest-priority backend able to initialize. This method wraps [`prism_registry_acquire_best`](../c/registry-functions.md#prism_registry_acquire_best).

#### Syntax

```gdscript
func acquire_best() -> GodotPrismBackend
```

#### Return Value

Returns the backend, or `null` if no backend could be obtained.

### load_backend_library

Loads the backends of a plugin into the singleton's registry. This method wraps [`prism_registry_builder_add_library`](../c/shared-library-backends.md#prism_registry_builder_add_library).

#### Syntax

```gdscript
func load_backend_library(path: String, priority_override: int = -1) -> PrismError
```

#### Parameters

`path`

The path to the shared library. The `res://` and `user://` prefixes MAY be used where they refer to real filesystem locations; a library within an exported project's PCK file cannot be loaded.

`priority_override`

A priority to assign to every backend the plugin supplies. A negative value honors the priorities the plugin declares.

#### Return Value

Returns `PRISM_OK` on success. Returns `PRISM_ERROR_INVALID_PARAM` if `path` is empty or `priority_override` exceeds the range of a C `int`, `PRISM_ERROR_NOT_IMPLEMENTED` on platforms without support for shared library backends, `PRISM_ERROR_MEMORY_FAILURE` or `PRISM_ERROR_INTERNAL` if the new registry or context could not be created, and otherwise the [error](./errors.md) returned by the C function.

#### Remarks

Since a registry cannot change once it is frozen, a successful call constructs a new registry, containing the libraries loaded by earlier calls, and replaces the singleton's context. Availability monitoring is restarted if it was active, which establishes a new baseline. Backends obtained before the call remain valid. On failure, the singleton is unchanged, with one exception: if the libraries load but availability monitoring cannot be restarted, the new registry is retained, monitoring remains stopped, and the method returns `PRISM_ERROR_INTERNAL`.

The requirements in the section on [security and deployment](../c/shared-library-backends.md#security-and-deployment) apply.

### start_availability_monitoring

Begins background availability monitoring.

#### Syntax

```gdscript
func start_availability_monitoring(
	poll_interval_ms: int = 0,
	debounce_samples: int = 0,
	backoff_max_ms: int = 0,
	auto_power_manage: bool = true
) -> PrismError
```

#### Parameters

`poll_interval_ms`, `debounce_samples`, `backoff_max_ms`, and `auto_power_manage`

These correspond to the members of [`PrismConfig`](../c/context-management.md#prismconfig) with the prefix `availability_`.

#### Return Value

Returns `PRISM_OK` on success, `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` if a numeric argument cannot be represented as an unsigned 32-bit integer, `PRISM_ERROR_INVALID_OPERATION` if monitoring is already active, and `PRISM_ERROR_INTERNAL` if the monitoring context could not be created.

#### Remarks

The sampling model is described in the chapter on [background availability enumeration](../c/backend-enumeration.md). Results are delivered through the [`backend_availability_changed`](#backend_availability_changed) and [`availability_baseline_established`](#availability_baseline_established) signals.

#### Example

```gdscript
func _ready() -> void:
	Prism.backend_availability_changed.connect(_on_availability_changed)
	var err := Prism.start_availability_monitoring()
	if err != GodotPrismBackend.PRISM_OK:
		push_error("Monitoring failed: %s" % Prism.get_error_string(err))

func _on_availability_changed(id: int, backend_name: String, available: bool) -> void:
	print("%s: %s" % [backend_name, available])
```

### stop_availability_monitoring

Ends availability monitoring. This method has no effect if monitoring is not active.

#### Syntax

```gdscript
func stop_availability_monitoring() -> void
```

### pause_availability_monitoring

Pauses availability monitoring. This method wraps [`prism_availability_poll_pause`](../c/backend-enumeration.md#prism_availability_poll_pause), and has no effect if monitoring is not active.

#### Syntax

```gdscript
func pause_availability_monitoring() -> void
```

### resume_availability_monitoring

Resumes availability monitoring. This method wraps [`prism_availability_poll_resume`](../c/backend-enumeration.md#prism_availability_poll_resume), and has no effect if monitoring is not active.

#### Syntax

```gdscript
func resume_availability_monitoring() -> void
```

### is_availability_monitoring

Reports whether availability monitoring is active.

#### Syntax

```gdscript
func is_availability_monitoring() -> bool
```

### is_auto_power_management_supported

Reports whether monitoring can follow operating-system power transitions. This static method wraps [`prism_availability_auto_power_supported`](../c/backend-enumeration.md#prism_availability_auto_power_supported).

#### Syntax

```gdscript
static func is_auto_power_management_supported() -> bool
```

### set_log_signal_enabled

Enables or disables delivery of log messages through the [`log_message_received`](#log_message_received) signal. This method wraps [`prism_set_log_handler`](../c/logging.md#prism_set_log_handler).

#### Syntax

```gdscript
func set_log_signal_enabled(enabled: bool) -> void
```

#### Parameters

`enabled`

Whether to deliver log messages through the signal.

#### Remarks

Enabling the signal replaces the handler in effect, including the one installed through the `PRISM_LOG` environment variable. Disabling it restores that handler. No messages are delivered until the threshold is raised through [`set_log_level`](#set_log_level), unless it has already been raised by other means.

`prism_log_shutdown` is not exposed, since logging is shared by the entire process and cannot be restarted afterwards.

#### Example

```gdscript
func _ready() -> void:
	Prism.log_message_received.connect(_on_log_message)
	Prism.set_log_signal_enabled(true)
	Prism.set_log_level(GodotPrismContext.PRISM_LOG_LEVEL_INFO)

func _on_log_message(_level: int, source: String, message: String) -> void:
	print("%s: %s" % [source, message])
```

### is_log_signal_enabled

Reports whether log messages are being delivered through the signal.

#### Syntax

```gdscript
func is_log_signal_enabled() -> bool
```

### set_log_level

Sets the minimum severity of messages to deliver. This static method wraps [`prism_set_log_level`](../c/logging.md#prism_set_log_level).

#### Syntax

```gdscript
static func set_log_level(level: int) -> int
```

#### Parameters

`level`

A `PrismLogLevel` constant.

#### Return Value

Returns the previous threshold.

### write_log

Emits a log message. This static method wraps [`prism_log`](../c/logging.md#prism_log).

#### Syntax

```gdscript
static func write_log(level: int, source: String, message: String) -> PrismError
```

#### Parameters

`level`

A `PrismLogLevel` constant.

`source`

The component producing the message.

`message`

The text of the message.

#### Return Value

Returns `PRISM_OK`, or `PRISM_ERROR_INVALID_PARAM` if `source` or `message` is empty.

### flush_log

Blocks until queued log messages have been processed. This static method wraps [`prism_log_flush`](../c/logging.md#prism_log_flush).

#### Syntax

```gdscript
static func flush_log() -> void
```

#### Remarks

Signals for the messages concerned are emitted during the next idle processing of the main thread, and so have not yet been emitted when this method returns.

### get_version

Returns the version of the loaded library as an encoded integer. This static method wraps [`prism_version`](../c/utilities.md#prism_version).

#### Syntax

```gdscript
static func get_version() -> int
```

### get_version_string

Returns the version of the loaded library as a string. This static method wraps [`prism_version_string`](../c/utilities.md#prism_version_string).

#### Syntax

```gdscript
static func get_version_string() -> String
```

### Unsupported functionality

The implementation of custom backends in GDScript is not supported. The vtable functions of a [custom backend](../c/custom-backends.md) may be invoked on threads other than the main thread, including the poll thread, and at times when script execution cannot safely proceed. Custom backends for use with Godot SHOULD instead be written in a language that compiles to a shared library, and loaded through [`load_backend_library`](#load_backend_library).
