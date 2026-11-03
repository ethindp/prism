## Audio Format Functions

For backends that support `prism_backend_speak_to_memory`, these functions provide information about the audio format of the generated samples.

### prism_backend_get_channels

Retrieves the number of audio channels produced by the backend.

#### Syntax

```c
PrismError prism_backend_get_channels(PrismBackend *backend, size_t *out_channels);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`out_channels`

Pointer to receive the channel count. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Channel count was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support audio format queries. |

#### Remarks

This function returns the number of audio channels in the audio produced by `prism_backend_speak_to_memory`. Common values are 1 (mono) and 2 (stereo).

The channel count depends on the backend and the selected voice. Most TTS engines produce mono output, but some voices may produce stereo.

### prism_backend_get_sample_rate

Retrieves the sample rate of audio produced by the backend.

#### Syntax

```c
PrismError prism_backend_get_sample_rate(PrismBackend *backend, size_t *out_sample_rate);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`out_sample_rate`

Pointer to receive the sample rate in Hz. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Sample rate was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support audio format queries. |

#### Remarks

This function returns the sample rate of the audio produced by `prism_backend_speak_to_memory`, measured in samples per second (Hz). Common values include 16000, 22050, 44100, and 48000.

The sample rate depends on the backend and the selected voice. Different voices may produce audio at different sample rates.

Applications that process the audio data (for example, to mix it with other audio or to resample it) need to know the sample rate to perform correct processing.

### prism_backend_get_bit_depth

Retrieves the native bit depth of audio produced by the backend.

#### Syntax

```c
PrismError prism_backend_get_bit_depth(PrismBackend *backend, size_t *out_bit_depth);
```

#### Parameters

`backend`

The backend instance. This parameter MUST NOT be `NULL`.

`out_bit_depth`

Pointer to receive the bit depth. This parameter MUST NOT be `NULL`.

#### Return Value

| Value | Meaning |
| --- | --- |
| `PRISM_OK` | Bit depth was retrieved. |
| `PRISM_ERROR_NOT_INITIALIZED` | The backend has not been initialized. |
| `PRISM_ERROR_NOT_IMPLEMENTED` | The backend does not support audio format queries. |

#### Remarks

This function returns the native bit depth of the audio produced by the backend. Common values are 8 and 16.

This value represents the backend's native format, not the format delivered to the callback. Samples delivered to `PrismAudioCallback` are always 32-bit floating-point values in the range [-1.0, 1.0], regardless of the native bit depth.

The bit depth is informational. Applications typically do not need this value unless they are converting the audio to a specific format for storage or transmission.
