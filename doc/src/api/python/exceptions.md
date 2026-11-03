## Exceptions

### `PrismError`

The base class of the exceptions which report a `PrismError` value. The meanings of the values are given in the C API's chapter on [error codes](../c/error-codes.md).

#### Syntax

```python
class PrismError(Exception):
    def __init__(self, code: int, message: str | None = None) -> None: ...
    code: int
    message: str | None
```

#### Members

`code`

The numeric value of the error.

`message`

The description of the error, as returned by [`prism_error_string`](../c/utilities.md#prism_error_string), or the binding's own description where the binding detected the error itself.

### Subclasses

The following subclasses are defined in `prism.common`. Where a second base class is given, the exception MAY also be caught as that class.

| `PrismError` value | Exception | Additional base class |
| --- | --- | --- |
| `PRISM_ERROR_NOT_INITIALIZED` | `PrismNotInitializedError` | `RuntimeError` |
| `PRISM_ERROR_INVALID_PARAM` | `PrismInvalidParamError` | `ValueError` |
| `PRISM_ERROR_NOT_IMPLEMENTED` | `PrismNotImplementedError` | `NotImplementedError` |
| `PRISM_ERROR_NO_VOICES` | `PrismNoVoicesError` | None |
| `PRISM_ERROR_VOICE_NOT_FOUND` | `PrismVoiceNotFoundError` | `LookupError` |
| `PRISM_ERROR_SPEAK_FAILURE` | `PrismSpeakError` | `IOError` |
| `PRISM_ERROR_MEMORY_FAILURE` | `PrismMemoryError` | `MemoryError` |
| `PRISM_ERROR_RANGE_OUT_OF_BOUNDS` | `PrismRangeError` | `IndexError` |
| `PRISM_ERROR_INTERNAL` | `PrismInternalError` | `RuntimeError` |
| `PRISM_ERROR_NOT_SPEAKING` | `PrismNotSpeakingError` | None |
| `PRISM_ERROR_NOT_PAUSED` | `PrismNotPausedError` | None |
| `PRISM_ERROR_ALREADY_PAUSED` | `PrismAlreadyPausedError` | None |
| `PRISM_ERROR_INVALID_UTF8` | `PrismInvalidUtf8Error` | `UnicodeError` |
| `PRISM_ERROR_INVALID_OPERATION` | `PrismInvalidOperationError` | `RuntimeError` |
| `PRISM_ERROR_ALREADY_INITIALIZED` | `PrismAlreadyInitializedError` | `RuntimeError` |
| `PRISM_ERROR_BACKEND_NOT_AVAILABLE` | `PrismBackendNotAvailableError` | `RuntimeError` |
| `PRISM_ERROR_UNKNOWN` | `PrismUnknownError` | None |
| `PRISM_ERROR_INVALID_AUDIO_FORMAT` | `PrismInvalidAudioFormatError` | `RuntimeError` |
| `PRISM_ERROR_INTERNAL_BACKEND_LIMIT_EXCEEDED` | `PrismInternalBackendLimitExceededError` | `RuntimeError` |
| `PRISM_ERROR_BACKEND_ENTERED_UNDEFINED_STATE` | `PrismBackendEnteredUndefinedStateError` | `RuntimeError` |
| `PRISM_ERROR_LIBRARY_LOAD_FAILED` | `PrismLibraryLoadFailedError` | `OSError` |
| `PRISM_ERROR_LIBRARY_INVALID` | `PrismLibraryInvalidError` | `ImportError` |
| `PRISM_ERROR_INCOMPATIBLE_ABI` | `PrismIncompatibleAbiError` | `RuntimeError` |

A value with no corresponding entry is reported as `PrismUnknownError`.
