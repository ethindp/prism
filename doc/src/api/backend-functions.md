## Backend Functions

Backend functions operate on individual backend instances obtained from the registry.

### prism_backend_free

Releases a backend instance.

#### Syntax

```c
void prism_backend_free(PrismBackend *backend);
```

#### Parameters

`backend`

The backend to free. This parameter MAY be `NULL`.

#### Return Value

This function does not return a value.

#### Remarks

`prism_backend_free` releases the caller's handle to a backend instance. The behavior depends on how the backend was obtained:

* For backends obtained via `prism_registry_create` or `prism_registry_create_best`, calling `prism_backend_free` immediately destroys the backend and releases all associated resources. After the call returns, the `backend` pointer is invalid and MUST NOT be used.
* For backends obtained via `prism_registry_acquire` or `prism_registry_acquire_best`, the backend uses reference counting. Calling `prism_backend_free` decrements the reference count. The backend is destroyed only when the reference count reaches zero. If other handles to the same instance exist (from other calls to `prism_registry_acquire`), the backend remains alive and those handles remain valid.

If `backend` is `NULL`, this function has no effect.

This function does not explicitly stop any speech that may be in progress. Whether this function stops speech or not is backend-dependent. Applications that want to ensure speech is stopped before releasing the backend SHOULD call `prism_backend_stop` first.

After calling `prism_backend_free`, the `backend` pointer MUST NOT be used for any purpose. Passing a freed pointer to any Prism function results in undefined behavior.

### prism_backend_get_features

Returns a bitmask of all features supported by this backend, as well as other information.

#### Syntax

```c
uint64_t prism_backend_get_features(PrismBackend *backend);
```

#### Parameters

`backend`

The backend to query. This parameter MUST NOT be NULL.

#### Returns

A bitmask of feature information.

#### Remarks

This function MAY be called regardless of backend initialization state.

The returned bitmask indicates which functions are implemented by the backend and whether the underlying engine is currently available. If a bit is set, the corresponding function is implemented. If clear, calls to that function return `PRISM_ERROR_NOT_IMPLEMENTED`.

To determine runtime availability, this function MAY perform lightweight probes such as COM class factory lookups, RPC endpoint queries, D-Bus name ownership checks, or process enumeration. These operations are minimized but are not cost-free. Callers SHOULD cache the result and re-query only when the `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` bit is relevant.

Bit 1 is reserved.

The following bits are defined:

| Bit | Description |
| --- | --- |
| `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` | The underlying engine or service is available. This determination is advisory; `prism_backend_initialize` MAY still fail. |
| `PRISM_BACKEND_SUPPORTS_SPEAK` | `prism_backend_speak` is implemented. |
| `PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY` | `prism_backend_speak_to_memory` is implemented. |
| `PRISM_BACKEND_SUPPORTS_BRAILLE` | `prism_backend_braille` is implemented. |
| `PRISM_BACKEND_SUPPORTS_OUTPUT` | `prism_backend_output` is implemented. |
| `PRISM_BACKEND_SUPPORTS_IS_SPEAKING` | `prism_backend_is_speaking` is implemented. |
| `PRISM_BACKEND_SUPPORTS_STOP` | `prism_backend_stop` is implemented. |
| `PRISM_BACKEND_SUPPORTS_PAUSE` | `prism_backend_pause` is implemented. |
| `PRISM_BACKEND_SUPPORTS_RESUME` | `prism_backend_resume` is implemented. |
| `PRISM_BACKEND_SUPPORTS_SET_VOLUME` | `prism_backend_set_volume` is implemented. |
| `PRISM_BACKEND_SUPPORTS_GET_VOLUME` | `prism_backend_get_volume` is implemented. |
| `PRISM_BACKEND_SUPPORTS_SET_RATE` | `prism_backend_set_rate` is implemented. |
| `PRISM_BACKEND_SUPPORTS_GET_RATE` | `prism_backend_get_rate` is implemented. |
| `PRISM_BACKEND_SUPPORTS_SET_PITCH` | `prism_backend_set_pitch` is implemented. |
| `PRISM_BACKEND_SUPPORTS_GET_PITCH` | `prism_backend_get_pitch` is implemented. |
| `PRISM_BACKEND_SUPPORTS_REFRESH_VOICES` | `prism_backend_refresh_voices` is implemented. |
| `PRISM_BACKEND_SUPPORTS_COUNT_VOICES` | `prism_backend_count_voices` is implemented. |
| `PRISM_BACKEND_SUPPORTS_GET_VOICE_NAME` | `prism_backend_get_voice_name` is implemented. |
| `PRISM_BACKEND_SUPPORTS_GET_VOICE_LANGUAGE` | `prism_backend_get_voice_language` is implemented. |
| `PRISM_BACKEND_SUPPORTS_GET_VOICE` | `prism_backend_get_voice` is implemented. |
| `PRISM_BACKEND_SUPPORTS_SET_VOICE` | `prism_backend_set_voice` is implemented. |
| `PRISM_BACKEND_SUPPORTS_GET_CHANNELS` | `prism_backend_get_channels` is implemented. |
| `PRISM_BACKEND_SUPPORTS_GET_SAMPLE_RATE` | `prism_backend_get_sample_rate` is implemented. |
| `PRISM_BACKEND_SUPPORTS_GET_BIT_DEPTH` | `prism_backend_get_bit_depth` is implemented. |
| `PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK` | Reserved. |
| `PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY` | The backend trims leading and trailing silence from the audio stream before delivering it to the audio callback. |
| `PRISM_BACKEND_SUPPORTS_SPEAK_SSML` | Reserved. |
| `PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY_SSML` | Reserved. |

### prism_backend_name

Returns the human-readable name of a backend.

#### Syntax

```c
const char *prism_backend_name(PrismBackend *backend);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

#### Return Value

Returns a pointer to a null-terminated string containing the backend name on success.

#### Remarks

This function may be called on a backend that has not been initialized.

The returned string is owned by the backend and remains valid for the lifetime of the backend instance. Applications MUST NOT modify or free the returned string.

The name returned by this function is the same as would be returned by `prism_registry_name` for the backend's ID. It is provided as a convenience so that applications do not need to maintain a separate mapping from backend instances to IDs.

#### Example

```c
PrismBackend *backend = prism_registry_create_best(ctx);
if (backend) {
    printf("Selected backend: %s\n", prism_backend_name(backend));
}
```

### prism_backend_initialize

Initializes a backend instance.

#### Syntax

```c
PrismError prism_backend_initialize(PrismBackend *backend);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Initialization succeeded. |
| `PRISM_ERROR_ALREADY_INITIALIZED` | The backend was already initialized. |
| `PRISM_ERROR_BACKEND_NOT_AVAILABLE` | The backend's underlying system component is unavailable. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred during initialization. |
| `PRISM_ERROR_MEMORY_FAILURE` | Memory allocation failed during initialization. |
| `PRISM_ERROR_UNKNOWN` | An unspecified error occurred. |

#### Remarks

This function prepares a backend instance for use. It MUST be called after creating a backend with `prism_registry_create` and before calling any function that performs speech synthesis or queries backend state (except `prism_backend_name`, `prism_backend_free`, and `prism_backend_get_features`).

For backends obtained via `prism_registry_create_best` or `prism_registry_acquire_best`, the backend is already initialized when returned. Calling `prism_backend_initialize` on such a backend returns `PRISM_ERROR_ALREADY_INITIALIZED`. Applications MAY treat this error as non-fatal.

The `PRISM_ERROR_BACKEND_NOT_AVAILABLE` error indicates that the backend cannot function because a required system component is missing or not running.

When a backend returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`, the backend instance is still valid but cannot be used for speech. Applications SHOULD free the backend and try a different one, or use `prism_registry_create_best` to automatically find a working backend.

Initialization may involve allocating system resources, connecting to services, or performing other setup operations. These operations may take a noticeable amount of time on some systems. Applications that require responsive UI SHOULD perform initialization on a background thread.

Once initialized, a backend remains initialized until it is freed. There is no function to explicitly uninitialize a backend.

### prism_backend_speak

Synthesizes speech from the given text and plays it through the default audio output.

#### Syntax

```c
PrismError prism_backend_speak(PrismBackend *backend, const char *text, bool interrupt);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`text`

The text to speak. This parameter MUST NOT be `NULL` and MUST be a valid null-terminated UTF-8 string.

`interrupt`

Specifies whether to interrupt currently playing speech. If `true`, any speech in progress is stopped before the new speech begins. If `false`, behavior is backend-dependent.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Speech synthesis was successfully initiated. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_INVALID_UTF8` | `text` contains invalid UTF-8 sequences. |
| `PRISM_ERROR_SPEAK_FAILURE` | Speech synthesis failed. |
| `PRISM_ERROR_BACKEND_NOT_AVAILABLE` | The backend's system component is no longer available. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support speech synthesis. |

#### Remarks

This is the primary function for producing speech output. The text is synthesized using the backend's current voice, rate, pitch, and volume settings, and played through the system's default audio output device.

Depending on the backend, the function MAY return as soon as speech synthesis has been initiated, and it MAY not wait for the speech to complete. Applications that need to know when speech has finished SHOULD poll `prism_backend_is_speaking` or use `prism_backend_speak_to_memory` with a callback.

The `interrupt` parameter controls behavior when speech is already in progress:

- If `interrupt` is `true`, any current speech is stopped immediately (equivalent to calling `prism_backend_stop`) before the new speech begins. The new speech will be the only speech playing.
- If `interrupt` is `false`, behavior depends on the backend. Most backends will queue the new speech to play after the current speech finishes. Some backends may mix the new speech with the current speech. Screen reader backends typically honor their own interrupt settings.

The `text` parameter MUST be valid UTF-8. Prism validates the encoding before passing text to the backend. If the text contains invalid UTF-8 sequences (such as overlong encodings, surrogate halves, or sequences that decode to values above U+10FFFF), the function returns `PRISM_ERROR_INVALID_UTF8` without initiating speech.

Some backends impose length limits on text. Extremely long strings may be truncated or may cause errors. Applications that need to speak very long texts SHOULD break them into smaller segments.

Screen reader backends may route speech through the screen reader's own speech system, which may apply additional processing such as rate boosting or punctuation handling. This is generally the desired behavior when a screen reader is in use.

#### Example

```c
/* Speak with interrupt */
PrismError err = prism_backend_speak(backend, "Alert: system error detected", true);
if (err != PRISM_OK) {
    fprintf(stderr, "Speech failed: %s\n", prism_error_string(err));
}
/* Queue additional speech */
prism_backend_speak(backend, "Please check the log for details.", false);
```

### prism_backend_speak_to_memory

Synthesizes speech from the given text and delivers the audio samples to a callback function.

#### Syntax

```c
PrismError prism_backend_speak_to_memory(
    PrismBackend *backend,
    const char *text,
    PrismAudioCallback callback,
    void *userdata
);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`text`

The text to synthesize. This parameter MUST NOT be `NULL` and MUST be a valid null-terminated UTF-8 string.

`callback`

The function to receive audio data. This parameter MUST NOT be `NULL`. See the "Audio Callback" section for the callback signature and semantics.

`userdata`

An arbitrary pointer passed to the callback. This parameter MAY be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Synthesis completed and audio was delivered to the callback. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_INVALID_UTF8` | `text` contains invalid UTF-8 sequences. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support memory synthesis. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred during synthesis. |
| `PRISM_ERROR_INVALID_AUDIO_FORMAT` | The audio format which the underlying engine returned to Prism cannot be understood by Prism, or it's parameters are nonsensical. |

#### Remarks

This function synthesizes speech but instead of playing it through the audio device, it delivers the raw audio samples to a callback function. This allows applications to process, analyze, store, or transmit the audio data.

Unlike `prism_backend_speak`, this function is always synchronous, regardless of the backend selected: it does not return until synthesis is complete and all audio data has been delivered to the callback. For long texts, this may take a significant amount of time. Applications that require responsive UI SHOULD call this function from a background thread.

The callback may be invoked multiple times during a single call to `prism_backend_speak_to_memory`, with each invocation delivering a portion of the audio. Alternatively, some backends may invoke the callback exactly once with all the audio data. Applications MUST be prepared for either pattern.

The callback may be invoked from a different thread than the one that called `prism_backend_speak_to_memory`. This depends on the backend; some backends synthesize in the calling thread, while others use background threads. Callback implementations MUST be thread-safe.

Audio samples are delivered as 32-bit floating-point values normalized to the range [-1.0, 1.0], regardless of the backend's native format. Multi-channel audio is interleaved: for stereo, samples alternate left-right-left-right.

Not all backends support this function.

To determine the audio format before synthesis, use `prism_backend_get_channels`, `prism_backend_get_sample_rate`, and `prism_backend_get_bit_depth`. Note that the bit depth returned by `prism_backend_get_bit_depth` reflects the native format; samples delivered to the callback are always 32-bit float.

#### Example

```c
void audio_callback(void *userdata, const float *samples, 
                    size_t sample_count, size_t channels, size_t sample_rate) {
    FILE *file = (FILE *)userdata;
    /* Write samples to file or process them */
    fwrite(samples, sizeof(float), sample_count, file);
}
FILE *output = fopen("speech.raw", "wb");
PrismError err = prism_backend_speak_to_memory(backend, "Hello", audio_callback, output);
fclose(output);
```

### prism_backend_braille

Outputs text to a connected braille display.

#### Syntax

```c
PrismError prism_backend_braille(PrismBackend *backend, const char *text);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`text`

The text to display. This parameter MUST NOT be `NULL` and MUST be a valid null-terminated UTF-8 string.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Text was successfully sent to the braille display. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_INVALID_UTF8` | `text` contains invalid UTF-8 sequences. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support braille output. |
| `PRISM_ERROR_BACKEND_NOT_AVAILABLE` | The backend's system component is no longer available. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred. |

#### Remarks

This function sends text to a braille display connected to the system. It is primarily supported by screen reader backends, which have their own braille display drivers, although not all screen reader backends support this function. Standalone TTS backends typically return `PRISM_ERROR_NOT_IMPLEMENTED`.

Braille output is independent of speech output. Calling this function does not affect any speech that may be in progress, and calling speech functions does not affect braille output.

The text is displayed on the braille display according to the screen reader's braille settings, which may include translation to contracted braille. The exact behavior depends on the screen reader's configuration.

If no braille display is connected, some backends return success (the text is simply discarded), while others return an error. Applications SHOULD NOT rely on the return value to determine whether a braille display is present.

For applications that want to output both speech and braille simultaneously, use `prism_backend_output` instead of calling `prism_backend_speak` and `prism_backend_braille` separately.

### prism_backend_output

Outputs text using all available modalities supported by the backend.

#### Syntax

```c
PrismError prism_backend_output(PrismBackend *backend, const char *text, bool interrupt);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`text`

The text to output. This parameter MUST NOT be `NULL` and MUST be a valid null-terminated UTF-8 string.

`interrupt`

Specifies whether to interrupt current output. If `true`, any output in progress is stopped.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Output was successfully initiated. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_INVALID_UTF8` | `text` contains invalid UTF-8 sequences. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support this operation. |
| `PRISM_ERROR_SPEAK_FAILURE` | Output failed. |

#### Remarks

This function is a convenience wrapper that outputs text through all modalities supported by the backend. For TTS-only backends, it is equivalent to `prism_backend_speak`. For screen reader backends, it outputs to both speech and braille.

The `interrupt` parameter has the same semantics as in `prism_backend_speak`: if `true`, current output is stopped before new output begins.

If either speech or braille output fails, the function may still return `PRISM_OK` if the other modality succeeded. Applications that require confirmation that both modalities succeeded SHOULD call `prism_backend_speak` and `prism_backend_braille` separately and check both return values.

### prism_backend_stop

Immediately stops any currently playing speech.

#### Syntax

```c
PrismError prism_backend_stop(PrismBackend *backend);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Speech was stopped or no speech was playing. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support stopping. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred. |

#### Remarks

This function immediately halts any speech currently being produced by the backend. Any queued speech is also discarded. After this function returns, `prism_backend_is_speaking` will return `false`.

Most backends return `PRISM_OK` even if no speech was playing when the function was called. However, applications SHOULD NOT rely on this behavior; some backends may return `PRISM_ERROR_NOT_SPEAKING`.

For screen reader backends, this function stops speech being produced through that backend's API. It does not stop speech initiated by the screen reader itself (such as when the user navigates with keyboard commands).

This function is synchronous: when it returns, speech has stopped. However, due to audio buffering, a brief moment of audio may play after the function returns on some systems.

Calling `prism_backend_stop` while synthesis is in progress via `prism_backend_speak_to_memory` has undefined behavior. Applications SHOULD NOT mix synchronous memory synthesis with asynchronous playback operations.

### prism_backend_pause

Pauses currently playing speech.

#### Syntax

```c
PrismError prism_backend_pause(PrismBackend *backend);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Speech was paused. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_SPEAKING` | No speech is currently playing. |
| `PRISM_ERROR_ALREADY_PAUSED` | Speech is already paused. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support pausing. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred. |

#### Remarks

This function pauses speech at its current position. The speech can be resumed from the same position by calling `prism_backend_resume`. Unlike `prism_backend_stop`, pausing does not discard queued speech.

Not all backends support pause and resume functionality.

The function returns `PRISM_ERROR_NOT_SPEAKING` if no speech is currently playing. Applications that want to pause unconditionally (pausing if speaking, doing nothing otherwise) SHOULD check for this error.

The function returns `PRISM_ERROR_ALREADY_PAUSED` if speech is already paused. Attempting to pause multiple times without an intervening resume is not allowed.

While paused, the backend remains in a paused state until either `prism_backend_resume` or `prism_backend_stop` is called. Calling `prism_backend_speak` with `interrupt` set to `true` while paused will stop the paused speech and start new speech. Calling `prism_backend_speak` with `interrupt` set to `false` while paused will queue the new speech; it will be spoken after the paused speech is resumed and completes.

### prism_backend_resume

Resumes previously paused speech.

#### Syntax

```c
PrismError prism_backend_resume(PrismBackend *backend);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Speech was resumed. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_PAUSED` | Speech is not currently paused. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support resuming. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred. |

#### Remarks

This function resumes speech that was previously paused by `prism_backend_pause`. Speech continues from the position where it was paused.

The function returns `PRISM_ERROR_NOT_PAUSED` if speech is not currently paused. This occurs if `prism_backend_pause` was never called or if `prism_backend_stop` was called after pausing.

After resuming, `prism_backend_is_speaking` will return `true` (assuming speech has not finished in the brief time since resuming).

### prism_backend_is_speaking

Queries whether the backend is currently producing speech output.

#### Syntax

```c
PrismError prism_backend_is_speaking(PrismBackend *backend, bool *out_speaking);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`out_speaking`

Pointer to receive the speaking state. This parameter MUST NOT be `NULL`. On success, set to `true` if speech is currently playing, `false` otherwise.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | The speaking state was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend cannot report speaking state. |

#### Remarks

This function queries whether the backend is currently producing speech output. It returns `true` if speech is playing, `false` if speech has finished or no speech was started.

Paused speech is not considered "speaking". If speech is paused, this function sets `out_speaking` to `false`.

Not all backends can accurately report their speaking state. Some backends, particularly screen reader backends, may return `PRISM_ERROR_NOT_IMPLEMENTED`. For these backends, applications cannot determine when speech has finished.

Due to the asynchronous nature of audio playback, there may be a brief delay between when the backend finishes generating audio and when this function returns `false`. Applications SHOULD NOT assume precise timing.
