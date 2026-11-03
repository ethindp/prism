## GodotPrismBackend

A backend instance, which derives from `RefCounted`. Instances are obtained from the [`Prism` singleton](./prism-singleton.md). The instance stops any speech in progress, if the backend supports doing so, and is released through [`prism_backend_free`](../c/backend-functions.md#prism_backend_free) when the last reference to it is released. An instance is subject to the rules given in the chapter on [thread safety](../c/thread-safety.md).

### Constants

#### `PrismBackendFeature`

The bits described under [`prism_backend_get_features`](../c/backend-functions.md#prism_backend_get_features), accessed as in `GodotPrismBackend.PRISM_BACKEND_SUPPORTS_SPEAK`. Each bit has the name given in that section.

### Properties

The following properties are provided. A property without a setter is read-only.

| Property | Type | Getter | Setter |
| --- | --- | --- | --- |
| `features` | `int` | `get_features` | None |
| `name` | `String` | `get_name` | None |
| `speaking` | `bool` | `get_speaking` | None |
| `volume` | `float` | `get_volume` | `set_volume` |
| `pitch` | `float` | `get_pitch` | `set_pitch` |
| `rate` | `float` | `get_rate` | `set_rate` |
| `voice` | `int` | `get_voice` | `set_voice` |
| `voices_count` | `int` | `get_voices_count` | None |
| `channels` | `int` | `get_channels` | None |
| `sample_rate` | `int` | `get_sample_rate` | None |
| `bit_depth` | `int` | `get_bit_depth` | None |

### Methods

### is_valid

Reports whether the instance refers to a backend.

#### Syntax

```gdscript
func is_valid() -> bool
```

#### Remarks

This method does not report whether the backend was initialized successfully.

### speak

Speaks text. This method wraps [`prism_backend_speak`](../c/backend-functions.md#prism_backend_speak).

#### Syntax

```gdscript
func speak(text: String, interrupt: bool = false) -> PrismError
```

#### Parameters

`text`

The text to speak, which MUST NOT be empty.

`interrupt`

Whether to interrupt speech in progress.

#### Return Value

Returns the [result](./errors.md) of the C function. Returns `PRISM_ERROR_INVALID_PARAM` if `text` is empty, and `PRISM_ERROR_NOT_IMPLEMENTED` if the backend does not support the operation.

#### Example

```gdscript
var err := backend.speak("Hello from Prism.", true)
if err != GodotPrismBackend.PRISM_OK:
	push_error("Speech failed: %s" % Prism.get_error_string(err))
```

### speak_to_memory

Synthesizes text and delivers the audio to a function. This method wraps [`prism_backend_speak_to_memory`](../c/backend-functions.md#prism_backend_speak_to_memory).

#### Syntax

```gdscript
func speak_to_memory(text: String, callback: Callable) -> PrismError
```

#### Parameters

`text`

The text to synthesize, which MUST NOT be empty.

`callback`

A function receiving a `PackedFloat32Array` of samples, the number of channels, and the sample rate.

#### Return Value

Returns the [result](./errors.md) of the C function. Returns `PRISM_ERROR_INVALID_PARAM` if `text` is empty or `callback` is not valid, and `PRISM_ERROR_NOT_IMPLEMENTED` if the backend does not support the operation.

#### Remarks

The audio is accumulated until synthesis has finished. `callback` is then called once, on the calling thread, before this method returns. It is not called if synthesis fails.

### speak_to_stream

Synthesizes text and returns it as an audio stream. This method has no direct equivalent in the C API.

#### Syntax

```gdscript
func speak_to_stream(text: String) -> AudioStreamWAV
```

#### Parameters

`text`

The text to synthesize.

#### Return Value

Returns a 16-bit `AudioStreamWAV`. Returns `null` if `text` is empty, the backend does not support the operation, synthesis fails, or no audio was produced. Failures other than empty text and empty audio are reported through Godot's error log.

#### Remarks

The stream is marked as stereo if the backend produced more than one channel. Audio with more than two channels is not represented correctly.

### braille

Presents text on a braille display. This method wraps [`prism_backend_braille`](../c/backend-functions.md#prism_backend_braille).

#### Syntax

```gdscript
func braille(text: String) -> PrismError
```

#### Parameters

`text`

The text to present, which MUST NOT be empty.

#### Return Value

Returns the [result](./errors.md) of the C function. Returns `PRISM_ERROR_INVALID_PARAM` if `text` is empty, and `PRISM_ERROR_NOT_IMPLEMENTED` if the backend does not support the operation.

### output

Presents text through every modality the backend supports. This method wraps [`prism_backend_output`](../c/backend-functions.md#prism_backend_output).

#### Syntax

```gdscript
func output(text: String, interrupt: bool = false) -> PrismError
```

#### Parameters

`text`

The text to present, which MUST NOT be empty.

`interrupt`

Whether to interrupt output in progress.

#### Return Value

Returns the [result](./errors.md) of the C function. Returns `PRISM_ERROR_INVALID_PARAM` if `text` is empty, and `PRISM_ERROR_NOT_IMPLEMENTED` if the backend does not support the operation.

### stop

Stops speech. This method wraps [`prism_backend_stop`](../c/backend-functions.md#prism_backend_stop).

#### Syntax

```gdscript
func stop() -> PrismError
```

#### Return Value

Returns the [result](./errors.md) of the C function, or `PRISM_ERROR_NOT_IMPLEMENTED` if the backend does not support the operation.

### pause

Pauses speech. This method wraps [`prism_backend_pause`](../c/backend-functions.md#prism_backend_pause).

#### Syntax

```gdscript
func pause() -> PrismError
```

#### Return Value

Returns the [result](./errors.md) of the C function, or `PRISM_ERROR_NOT_IMPLEMENTED` if the backend does not support the operation.

### resume

Resumes speech. This method wraps [`prism_backend_resume`](../c/backend-functions.md#prism_backend_resume).

#### Syntax

```gdscript
func resume() -> PrismError
```

#### Return Value

Returns the [result](./errors.md) of the C function, or `PRISM_ERROR_NOT_IMPLEMENTED` if the backend does not support the operation.

### get_features

Returns the backend's features. This method wraps [`prism_backend_get_features`](../c/backend-functions.md#prism_backend_get_features) and queries the backend on each call.

#### Syntax

```gdscript
func get_features() -> int
```

### has_feature

Reports whether the backend has all of the given features.

#### Syntax

```gdscript
func has_feature(flag: int) -> bool
```

#### Parameters

`flag`

One or more `PrismBackendFeature` bits.

#### Return Value

Returns `true` if every given bit is set, and `false` otherwise.

### get_name

Returns the name of the backend. This method wraps [`prism_backend_name`](../c/backend-functions.md#prism_backend_name).

#### Syntax

```gdscript
func get_name() -> String
```

### get_speaking

Reports whether the backend is speaking. This method wraps [`prism_backend_is_speaking`](../c/backend-functions.md#prism_backend_is_speaking).

#### Syntax

```gdscript
func get_speaking() -> bool
```

#### Return Value

Returns `true` if the backend reports that it is speaking. Returns `false` otherwise, including when the backend does not support the query or the query fails. The value `false` therefore does not establish that speech has finished.

### set_volume, set_pitch, and set_rate

Set the voice parameters. These methods wrap the corresponding functions described in the chapter on [voice parameters](../c/voice-parameters.md).

#### Syntax

```gdscript
func set_volume(volume: float) -> void
func set_pitch(pitch: float) -> void
func set_rate(rate: float) -> void
```

#### Remarks

Failures, including lack of support for the operation, are reported through Godot's error log.

### get_volume, get_pitch, and get_rate

Return the voice parameters. These methods wrap the corresponding functions described in the chapter on [voice parameters](../c/voice-parameters.md).

#### Syntax

```gdscript
func get_volume() -> float
func get_pitch() -> float
func get_rate() -> float
```

#### Return Value

Returns the value of the parameter. Returns `0.5` if the backend does not support the operation. Returns `NAN` if the operation fails, and reports the failure through Godot's error log.

### refresh_voices

Refreshes the voice list. This method wraps `prism_backend_refresh_voices`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```gdscript
func refresh_voices() -> PrismError
```

#### Return Value

Returns the [result](./errors.md) of the C function, or `PRISM_ERROR_NOT_IMPLEMENTED` if the backend does not support the operation.

### get_voices_count

Returns the number of voices. This method wraps `prism_backend_count_voices`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```gdscript
func get_voices_count() -> int
```

#### Return Value

Returns the number of voices, or `0` if the backend does not support the operation or the operation fails.

### get_voice_name

Returns the name of a voice. This method wraps `prism_backend_get_voice_name`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```gdscript
func get_voice_name(index: int) -> String
```

#### Parameters

`index`

The index of the voice.

#### Return Value

Returns the name of the voice, or an empty string if the backend does not support the operation or the operation fails.

### get_voice_language

Returns the language of a voice. This method wraps `prism_backend_get_voice_language`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```gdscript
func get_voice_language(index: int) -> String
```

#### Parameters

`index`

The index of the voice.

#### Return Value

Returns the language of the voice, or an empty string if the backend does not support the operation or the operation fails.

### get_voices

Returns the backend's voices. This method has no direct equivalent in the C API.

#### Syntax

```gdscript
func get_voices() -> Array[Dictionary]
```

#### Return Value

Returns one `Dictionary` for each voice, with the keys `"index"`, `"name"`, and `"language"`.

### find_voice

Locates a voice by name. This method has no direct equivalent in the C API.

#### Syntax

```gdscript
func find_voice(pattern: String) -> int
```

#### Parameters

`pattern`

Text to search for within the name of each voice, without regard to case.

#### Return Value

Returns the index of the first matching voice, or `-1` if `pattern` is empty or no voice matches.

### find_voice_by_language

Locates a voice by language. This method has no direct equivalent in the C API.

#### Syntax

```gdscript
func find_voice_by_language(prefix: String) -> int
```

#### Parameters

`prefix`

The text with which the voice's language begins, without regard to case.

#### Return Value

Returns the index of the first matching voice, or `-1` if `prefix` is empty or no voice matches.

### set_voice

Selects a voice. This method wraps `prism_backend_set_voice`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```gdscript
func set_voice(index: int) -> void
```

#### Parameters

`index`

The index of the voice.

#### Remarks

Failures, including lack of support for the operation, are reported through Godot's error log.

### get_voice

Returns the index of the current voice. This method wraps `prism_backend_get_voice`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```gdscript
func get_voice() -> int
```

#### Return Value

Returns the index, or `-1` if the backend does not support the operation or the operation fails.

### get_channels, get_sample_rate, and get_bit_depth

Return the backend's audio format. These methods wrap the corresponding functions described in the chapter on [audio format](../c/audio-format.md).

#### Syntax

```gdscript
func get_channels() -> int
func get_sample_rate() -> int
func get_bit_depth() -> int
```

#### Return Value

Returns the value, or `-1` if the backend does not support the operation or the operation fails.
