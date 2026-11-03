## Voice Parameter Functions

Prism normalizes voice parameters (volume, rate, and pitch) to a floating-point range of 0.0 to 1.0 inclusive. This normalization allows applications to work with consistent ranges regardless of the underlying backend's native parameter ranges.

The value 0.5 represents the default or neutral setting for rate and pitch. For volume, 0.5 represents the system default volume level, though the perceptual "middle" volume may vary by backend.

Parameter changes take effect on subsequent speech. Changing parameters while speech is in progress may or may not affect the current speech, depending on the backend.

### prism_backend_set_volume

Sets the speech volume.

#### Syntax

```c
PrismError prism_backend_set_volume(PrismBackend *backend, float volume);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`volume`

The volume level, in the range [0.0, 1.0]. A value of 0.0 is silent; 1.0 is maximum volume.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Volume was set. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` | `volume` is outside [0.0, 1.0]. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support volume control. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred. |

#### Remarks

This function sets the volume level for speech synthesis. The volume affects how loud the synthesized speech will be.

The volume is independent of system volume settings. Setting the Prism volume to 1.0 does not override or affect the system volume or the volume of other applications.

Setting volume to 0.0 produces silent output. The backend still synthesizes speech, but no sound is heard.

The volume persists until changed. Once set, it applies to all subsequent speech until a new volume is set or the backend is freed.

Screen reader backends do not support volume control.

The parameter is validated before being passed to the backend. If `volume` is negative, greater than 1.0, NaN, or infinity, the function returns `PRISM_ERROR_RANGE_OUT_OF_BOUNDS`.

### prism_backend_get_volume

Retrieves the current speech volume.

#### Syntax

```c
PrismError prism_backend_get_volume(PrismBackend *backend, float *out_volume);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`out_volume`

Pointer to receive the volume. This parameter MUST NOT be `NULL`. On success, receives a value in the range [0.0, 1.0].

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Volume was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support volume control. |

#### Remarks

This function retrieves the current volume setting. The returned value is in the same normalized range [0.0, 1.0] used by `prism_backend_set_volume`.

If `prism_backend_set_volume` has not been called, the returned value represents the backend's default volume.

The value is retrieved from the backend's internal state, not from any system setting. It reflects the most recent value set by `prism_backend_set_volume`, or the default if no value has been set.

### prism_backend_set_rate

Sets the speech rate (speed).

#### Syntax

```c
PrismError prism_backend_set_rate(PrismBackend *backend, float rate);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`rate`

The speech rate, in the range [0.0, 1.0]. A value of 0.0 is the slowest rate; 1.0 is the fastest; 0.5 is the default.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Rate was set. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` | `rate` is outside [0.0, 1.0]. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support rate control. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred. |

#### Remarks

This function sets the speech rate, which controls how fast the synthesized speech is spoken. Lower values produce slower speech; higher values produce faster speech.

The rate affects the tempo of speech but not the pitch. At extreme rates, speech may become difficult to understand.

The normalized range [0.0, 1.0] is mapped to each backend's native rate range using a piecewise linear transformation that preserves the midpoint. This ensures that 0.5 always corresponds to the backend's default rate, regardless of how the native range is defined.

The rate persists until changed and applies to all subsequent speech.

Screen reader backends do not support this function.

### prism_backend_get_rate

Retrieves the current speech rate.

#### Syntax

```c
PrismError prism_backend_get_rate(PrismBackend *backend, float *out_rate);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`out_rate`

Pointer to receive the rate. This parameter MUST NOT be `NULL`. On success, receives a value in the range [0.0, 1.0].

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Rate was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support rate control. |

#### Remarks

This function retrieves the current rate setting. The returned value is in the same normalized range [0.0, 1.0] used by `prism_backend_set_rate`.

If `prism_backend_set_rate` has not been called, the returned value is the default rate for the selected backend.

Screen reader backends do not support this function.

### prism_backend_set_pitch

Sets the speech pitch.

#### Syntax

```c
PrismError prism_backend_set_pitch(PrismBackend *backend, float pitch);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`pitch`

The pitch, in the range [0.0, 1.0].

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Pitch was set. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` | `pitch` is outside [0.0, 1.0]. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support pitch control. |
| `PRISM_ERROR_INTERNAL` | An internal error occurred. |

#### Remarks

This function sets the speech pitch, which controls the fundamental frequency of the synthesized voice. Lower values produce a deeper voice; higher values produce a higher voice.

The pitch affects the perceived voice quality without changing the rate. Extreme pitch values may produce unnatural-sounding speech.

Not all backends support pitch control. Some TTS engines apply pitch at the voice level and do not allow runtime adjustment. SAPI supports pitch control; AVSpeech supports pitch control within a limited range; screen reader backends do not support pitch control.

The pitch persists until changed and applies to all subsequent speech.

### prism_backend_get_pitch

Retrieves the current speech pitch.

#### Syntax

```c
PrismError prism_backend_get_pitch(PrismBackend *backend, float *out_pitch);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`out_pitch`

Pointer to receive the pitch. This parameter MUST NOT be `NULL`. On success, receives a value in the range [0.0, 1.0].

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Pitch was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support pitch control. |

#### Remarks

This function retrieves the current pitch setting. The returned value is in the same normalized range [0.0, 1.0] used by `prism_backend_set_pitch`.

If `prism_backend_set_pitch` has not been called, the returned value is the default pitch for the selected backend.

Screen reader backends do not support this function.
