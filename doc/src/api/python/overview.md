# Python API Reference

## Introduction

The Python binding is distributed as the `prismatoid` package and imported as `prism`. It requires Python 3.10 or later and depends on `cffi`. Installation is described in the Getting Started chapter.

The binding is documented in its docstrings, which are available through `help()`, as in `help(prism.Context)`, and in editors that display docstrings. The chapters that follow are generated from the docstrings. Requirements of individual backends are given in the [backend-specific notes](../c/backend-notes.md).

## Correspondence with the C API

Registry functions are methods of `Context`, and backend functions are methods of `Backend`, named without their prefix: `prism_registry_create_best(ctx)` is `context.create_best()`, and `prism_backend_speak(backend, text, interrupt)` is `backend.speak(text, interrupt)`. A pair of functions that gets and sets a value is a property, as `Backend.volume` is for `prism_backend_get_volume` and `prism_backend_set_volume`. A function that only gets a value is a read-only property, as `Backend.speaking` is for `prism_backend_is_speaking`. Logging and version functions are functions of the `prism` module.

Enumeration members are named after the C constants without their prefix, and have the same values: `PRISM_BACKEND_NVDA` is `prism.BackendId.NVDA`, and `PRISM_LOG_LEVEL_WARN` is `prism.LogLevel.WARN`. Feature bits are fields of `prism.BackendFeatures`, named after the constants in lower case without their prefix: `PRISM_BACKEND_SUPPORTS_SPEAK` is `supports_speak`.

Errors are raised as exceptions. Each `PrismError` value has a subclass of `prism.PrismError` named after it, defined in `prism.common`, and each subclass also derives from the closest built-in exception. Methods that obtain a backend raise `ValueError` where the C function returns `NULL`.

Text is passed as `str` and encoded as UTF-8. It MUST NOT contain a NUL character, and MUST NOT be empty where text is required.

## Differences from the C API

* Backends are initialized when they are obtained. There is no equivalent of `prism_backend_initialize`.
* Contexts and backends are released when their objects are destroyed. There are no equivalents of `prism_shutdown` and `prism_backend_free`. A reference to a `Backend` MUST be kept for as long as speech is expected to continue.
* Availability callbacks run on a thread of the binding, not on Prism's poll thread, and MAY be coroutine functions.
* Log messages are forwarded to the `logging` module.
* Custom backends are subclasses of `CustomBackend`. Their features are determined by the methods they override.
