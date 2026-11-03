# Python API Reference

## Introduction

The Python binding is distributed as the `prismatoid` package and imported as `prism`. It requires Python 3.10 or later and depends on `cffi`. Installation is described in the Getting Started chapter.

This reference documents only what is particular to Python. Each entry names the C function or type on which it is built, and the semantics of the operation, including its preconditions, threading behavior, and backend-specific behavior, are those given in the [C API Reference](../c/overview.md), which is not repeated here. The requirements of individual backends are given in the [backend-specific notes](../c/backend-notes.md), and the thread-safety rules for backend instances are given in the chapter on [thread safety](../c/thread-safety.md).

## Differences from the C API

The binding differs from the C API in the following respects:

* Failures are reported by raising exceptions rather than by returning error codes. See [Exceptions](./exceptions.md).
* Backends are initialized when they are obtained, so the binding has no equivalent of `prism_backend_initialize`.
* Native resources are released when the corresponding Python objects are destroyed, rather than by explicit calls to `prism_shutdown` or `prism_backend_free`. An application MUST retain a reference to a `Backend` for as long as it expects speech to continue.
* Availability callbacks are not invoked on Prism's poll thread. See [`Context`](./context.md#context).

## Contents of the module

The `prism` module exports the classes, functions, and types documented in the following chapters. The subclasses of `PrismError` are defined in `prism.common`.
