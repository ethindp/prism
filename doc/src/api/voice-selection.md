## Voice Selection Functions

Backends that support multiple voices provide functions to enumerate and select voices. Voices are identified by zero-based indices that are valid only for the current voice list; calling `prism_backend_refresh_voices` may invalidate previously obtained indices.

Screen reader backends do not support these functions.

### prism_backend_refresh_voices

Refreshes the backend's internal voice list.

#### Syntax

```c
PrismError prism_backend_refresh_voices(PrismBackend *backend);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Voice list was refreshed. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support voice enumeration. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred. |

#### Remarks

This function queries the system for the current list of available voices and updates the backend's internal voice list. Applications SHOULD call this function if voices may have been added to or removed from the system since the backend was initialized.

On most systems, voices are installed through system settings or third-party software. If a user installs a new voice while an application is running, that voice MAY NOT appear in the backend's voice list until `prism_backend_refresh_voices` is called, although this behavior is backend-dependent.

After calling this function, previously obtained voice indices may no longer be valid. A voice that was at index 3 before the refresh may be at a different index (or may no longer exist) after the refresh. Applications that store voice preferences SHOULD store the voice name rather than the index, and look up the index after refreshing.

Some backends perform voice enumeration during initialization and do not support refreshing. These backends return `PRISM_OK` but may not actually update their voice list.

### prism_backend_count_voices

Returns the number of voices available from the backend.

#### Syntax

```c
PrismError prism_backend_count_voices(PrismBackend *backend, size_t *out_count);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`out_count`

Pointer to receive the voice count. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Voice count was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support voice enumeration. |

#### Remarks

This function returns the number of voices currently available from the backend. Voice indices range from 0 to count-1 inclusive.

A return value of 0 indicates that no voices are available. This may occur if no TTS voices are installed on the system, or if the backend does not enumerate voices.

The count reflects the state of the voice list at the time of the call. If voices are added or removed from the system, the count may change after a call to `prism_backend_refresh_voices`.

### prism_backend_get_voice_name

Retrieves the human-readable name of a voice.

#### Syntax

```c
PrismError prism_backend_get_voice_name(
    PrismBackend *backend,
    size_t voice_id,
    const char **out_name
);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`voice_id`

The zero-based index of the voice.

`out_name`

Pointer to receive the voice name. This parameter MUST NOT be `NULL`. On success, receives a pointer to a null-terminated string.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Voice name was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` | `voice_id` is out of range. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support voice enumeration. |

#### Remarks

This function retrieves the human-readable name of a voice, such as "Microsoft David" or "Samantha".

The returned string is owned by the backend and remains valid only until the next call to `prism_backend_get_voice_name` or `prism_backend_get_voice_language` on the same backend instance. Applications that need to retain the name MUST copy it.

Voice names are determined by the TTS engine and are typically localized. The same voice may have different names on different language versions of the operating system.

### prism_backend_get_voice_language

Retrieves the language code or language string of a voice.

#### Syntax

```c
PrismError prism_backend_get_voice_language(
    PrismBackend *backend,
    size_t voice_id,
    const char **out_language
);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`voice_id`

The zero-based index of the voice.

`out_language`

Pointer to receive the language code or language string. This parameter MUST NOT be `NULL`. On success, receives a pointer to a null-terminated string.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Language was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` | `voice_id` is out of range. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support voice enumeration. |

#### Remarks

This function retrieves the language code or language string of a voice. The format is typically a BCP 47 language tag such as "en-US", "de-DE", or "ja-JP", but the exact format depends on the backend and the underlying TTS engine.

The returned string follows the same lifetime rules as `prism_backend_get_voice_name`: it is valid only until the next call to either function on the same backend.

### prism_backend_set_voice

Selects a voice to use for subsequent speech synthesis.

#### Syntax

```c
PrismError prism_backend_set_voice(PrismBackend *backend, size_t voice_id);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`voice_id`

The zero-based index of the voice to select.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Voice was selected. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` | `voice_id` is out of range. |
| `PRISM_ERROR_VOICE_NOT_FOUND` | The voice does not exist. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support voice selection. |

#### Remarks

This function selects a voice to use for subsequent speech synthesis. The voice persists until changed and applies to all subsequent speech.

If speech is currently playing when the voice is changed, the change may or may not affect the current speech depending on the backend. Most backends apply the new voice to subsequent speech operations only.

If `voice_id` is out of range (greater than or equal to the count returned by `prism_backend_count_voices`), the function returns `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` or `PRISM_ERROR_VOICE_NOT_FOUND`. The distinction between these errors is not significant; both indicate that the voice does not exist.

Changing the voice may affect the audio format returned by `prism_backend_get_channels`, `prism_backend_get_sample_rate`, and `prism_backend_get_bit_depth`. Applications using `prism_backend_speak_to_memory` SHOULD query the audio format after changing the voice.

### prism_backend_get_voice

Retrieves the index of the currently selected voice.

#### Syntax

```c
PrismError prism_backend_get_voice(PrismBackend *backend, size_t *out_voice_id);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`out_voice_id`

Pointer to receive the voice index. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Voice index was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support voice selection. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred. |

#### Remarks

This function retrieves the index of the currently selected voice. If `prism_backend_set_voice` has not been called, the returned value is the index of the backend's default voice.

The returned index can be used with `prism_backend_get_voice_name` and `prism_backend_get_voice_language` to retrieve information about the current voice.

If the voice list has been refreshed since the voice was selected, the returned index reflects the voice's position in the new list, which may differ from its position in the old list. If the selected voice no longer exists after a refresh, the backend's behavior is undefined.
