## Error Codes

When a function returns a `PrismError`, the `prism_error_string` function can be used to return a human-readable string error message which can be bubbled up to applications. The numeric values of error codes are part of the ABI and will not change in future versions. Errors MAY, however, be added in future.

| Error | Value | Description |
| --- | --- | --- |
| `PRISM_OK` | 0 | Operation completed without error |
| `PRISM_ERROR_NOT_INITIALIZED` | 1 | The backend was not initialized; `prism_backend_initialize` was not called or failed |
| `PRISM_ERROR_INVALID_PARAM` | 2 | An invalid parameter was passed to a function |
| `PRISM_ERROR_NOT_IMPLEMENTED` | 3 | The operation is not supported by the selected backend |
| `PRISM_ERROR_NO_VOICES` | 4 | No voices are available for this backend |
| `PRISM_ERROR_VOICE_NOT_FOUND` | 5 | The specified voice was not found |
| `PRISM_ERROR_SPEAK_FAILURE` | 6 | Speech synthesis failed |
| `PRISM_ERROR_MEMORY_FAILURE` | 7 | Memory allocation failed |
| `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` | 8 | A parameter value exceeded its valid range |
| `PRISM_ERROR_INTERNAL` | 9 | An internal backend error occurred |
| `PRISM_ERROR_NOT_SPEAKING` | 10 | Attempted to stop or pause when not speaking |
| `PRISM_ERROR_NOT_PAUSED` | 11 | Attempted to resume when not paused |
| `PRISM_ERROR_ALREADY_PAUSED` | 12 | Attempted to pause when already paused |
| `PRISM_ERROR_INVALID_UTF8` | 13 | A string parameter contained invalid UTF-8 |
| `PRISM_ERROR_INVALID_OPERATION` | 14 | The operation is invalid in the current state |
| `PRISM_ERROR_ALREADY_INITIALIZED` | 15 | Attempted to initialize an already-initialized backend |
| `PRISM_ERROR_BACKEND_NOT_AVAILABLE` | 16 | The backend is not available on this system |
| `PRISM_ERROR_UNKNOWN` | 17 | An unspecified error occurred |
| `PRISM_ERROR_INVALID_AUDIO_FORMAT` | 18 | Either the backend speech engine or backend voice have an audio format that Prism does not know how to handle, or the parameters that the underlying speech engine provided to Prism were nonsensical. |

The constant `PRISM_ERROR_COUNT` equals the total number of error codes and MAY be used for bounds checking or table sizing. This constant may increase in future versions as new error codes are added.
