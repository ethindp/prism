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

This function MAY be called regardless of the state of Prism.

#### Example

```c
PrismError err = prism_backend_speak(backend, "Hello", true);
if (err != PRISM_OK) {
    fprintf(stderr, "Error: %s\n", prism_error_string(err));
}
```

### prism_version

Returns the version of the loaded Prism library as an encoded integer.

#### Syntax

```c
uint32_t prism_version(void);
```

#### Parameters

This function has no parameters.

#### Return Value

Returns the version of the loaded Prism library as an unsigned 32-bit integer. This function always returns a valid version value and does not use a sentinel value to indicate failure.

#### Remarks

This function reports the release version of the Prism library that is actually linked into or loaded by the calling process. The returned value describes the library binary, not the version of the Prism headers with which the application was compiled. This distinction is particularly important when Prism is used as a shared library, since the version of Prism installed on the user's system MAY differ from the version against which the application was originally built.

Prism follows [Semantic Versioning](https://semver.org/). A Prism release version therefore consists of major, minor, and patch components. Once Prism has reached a stable major version, an increment to the major component indicates an incompatible change to the public API, an increment to the minor component indicates the addition of functionality in a backward-compatible manner, and an increment to the patch component indicates a backward-compatible correction. While the major version is zero, Prism is considered to be under initial development in accordance with Semantic Versioning, and compatibility between minor releases MUST NOT be assumed merely because the major version remains zero.

The value returned by `prism_version` encodes the major, minor, and patch components of the Prism release version into the low 24 bits of the returned integer. The major version occupies bits 16 through 23, the minor version occupies bits 8 through 15, and the patch version occupies bits 0 through 7. Bits 24 through 31 are reserved and are currently zero. The encoded representation is equivalent to:

```c
(major << 16) | (minor << 8) | patch
```

For example, Prism version `1.4.7` is represented by the value `0x00010407`.

The encoded representation also permits the core major, minor, and patch version to be compared using an ordinary unsigned integer comparison. Because the major component occupies the most significant portion of the representation, followed by the minor and patch components, the numeric ordering of encoded values is the same as the Semantic Versioning ordering of their corresponding major, minor, and patch components. An application that requires at least Prism version `1.4.0`, for example, MAY compare the result of `prism_version` against the value representing `1.4.0`.

The version is determined when Prism is built and remains constant for the lifetime of the loaded library. Repeated calls therefore return the same value.

This function does not require Prism to be initialized.

### prism_version_string

Returns the version of the loaded Prism library as a human-readable string.

#### Syntax

```c
const char *prism_version_string(void);
```

#### Parameters

This function has no parameters.

#### Return Value

Returns a pointer to a null-terminated string containing the release version of the loaded Prism library. This function never returns `NULL`.

#### Remarks

This function provides the human-readable representation of the Prism release version reported numerically by `prism_version`. The returned string identifies the Prism library that is actually linked into or loaded by the current process. It therefore reflects the version of the library binary at runtime rather than necessarily reflecting the version of the Prism headers that were used when the calling application was compiled.

The returned string is owned by Prism and is statically allocated. It remains valid for the lifetime of the loaded library. Applications MUST NOT modify the contents of the returned string and MUST NOT attempt to free or otherwise deallocate the returned pointer. An application that requires a modifiable copy of the version string MUST copy it into storage owned by the application.
