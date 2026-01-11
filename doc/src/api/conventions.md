## API Conventions

### Naming Conventions

Within the Prism API, the following conventions are always constant:

* Context management functions use the prefix `prism_`.
* Backend registry functions use the prefix `prism_registry_`.
* Backend functions use the prefix `prism_backend_`.
* Error utility functions use the prefix `prism_error_`.
* Types use the prefix `Prism` followed by PascalCase naming.
* Macros and constants use the prefix `PRISM_` followed by SCREAMING_SNAKE_CASE.

### Return Value Conventions

The vast majority of functions return a `PrismError`. However, some don't, and in those circumstances, a sentinel value is returned to indicate failure:

* For boolean functions, `false` is returned when errors occur.
* For functions returning `size_t`, `SIZE_MAX` is returned on error.
* For functions returning signed integers, `-1` is returned on error.
* For functions returning pointers or character strings, `NULL` is returned on error.
* For functions returning `PrismBackendId`, `PRISM_BACKEND_INVALID` is returned on error.

Functions returning `PrismError` return `PRISM_OK` on success. Any other value indicates an error condition, unless explicitly specified.

### Static Analysis Support

Prism employs compiler-specific annotations to assist developers in catching errors at compile time. These annotations are transparent to application code, but compilers that recognize them will issue warnings or errors when code violates the documented contracts.

All functions returning `PrismError` are annotated such that the compiler will warn if the return value is discarded. Since nearly every Prism function can fail, ignoring a return value almost always represents a programming error. Similarly, functions that allocate resources (such as `prism_init` or `prism_registry_create`) are annotated to warn if their return value is ignored, since failing to capture the returned pointer results in a resource leak.

Functions that require non-null pointer arguments are annotated with the indices of those parameters. Compilers that support this annotation will warn if a null pointer is passed to such a parameter. Note that passing `NULL` to any function where `NULL` is prohibited is always a programming error and results in undefined behavior. Compilers MAY make optimization assumptions based on these invariants being upheld.

Functions that return newly allocated memory are annotated to inform the compiler that the returned pointer does not alias any other pointer. This enables certain optimizations and also serves as documentation that the caller is responsible for freeing the returned memory.

Functions that accept [printf](https://en.cppreference.com/w/c/io/fprintf)-style format strings (reserved for future diagnostic functions) are annotated such that the compiler can validate format specifiers against the provided arguments.

Functions or types that are deprecated are annotated with a message explaining the deprecation and suggesting alternatives. The compiler will warn when deprecated elements are used.

Functions that take NULL-terminated strings are annotated to assist in static analysis. Programmers MAY use these when using more advanced analysis features (such as GCC/Clang's `-fanalyze` command-line argument) to ensure that strings are always NULL-terminated.

These annotations are supported on GCC, Clang, and MSVC to varying degrees. On compilers that do not support a particular annotation, that annotation has no effect. Applications SHOULD enable compiler warnings and treat warnings from these annotations as errors during development.

### Ownership and lifetime requirements

The library does it's best to avoid copying unless absolutely required. On some back-ends, this is unavoidable. Programmers MUST be aware that, when passing in strings or other data, the caller owns the passed-in argument. The lifetime of the argument must exceed that of the function call. Although some back-ends may copy strings, this MUST NOT be assumed to occur.

When the library returns pointers to data in memory, the library owns that data. The caller MUST NOT free or otherwise deallocate the returned memory unless explicitly specified in this manual (for example, a context requires explicit deallocation, as does a back-end, but a back-end name MUST NOT be freed).
