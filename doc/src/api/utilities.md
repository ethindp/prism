## Utility Functions

### prism_error_string

Returns a human-readable description of an error code.

#### Syntax

```c
const char *prism_error_string(PrismError error);
```

#### Parameters

`error`

The error code to describe.

#### Return Value

Returns a pointer to a null-terminated string describing the error. Never returns `NULL`.

#### Remarks

This function converts a `PrismError` code to a human-readable string suitable for display to users or inclusion in log messages.

The returned string is statically allocated and remains valid for the lifetime of the process. Applications MUST NOT modify or free the returned string.

If `error` is not a valid error code (for example, if it is greater than or equal to `PRISM_ERROR_COUNT`), the function returns the string "Unknown error".

This function is the only Prism function that may be called without first calling `prism_init`. It is safe to call at any time.

#### Example

```c
PrismError err = prism_backend_speak(backend, "Hello", true);
if (err != PRISM_OK) {
    fprintf(stderr, "Error: %s\n", prism_error_string(err));
}
```
