# Godot API Reference

## Introduction

The Godot binding is distributed as an add-on, which requires Godot 4.5 or later. Installation is described in the Getting Started chapter.

This reference documents only what is particular to Godot. Each entry names the C function or type on which it is built, and the semantics of the operation, including its preconditions, threading behavior, and backend-specific behavior, are those given in the [C API Reference](../c/overview.md), which is not repeated here. The requirements of individual backends are given in the [backend-specific notes](../c/backend-notes.md), and the thread-safety rules for backend instances are given in the chapter on [thread safety](../c/thread-safety.md).

## Differences from the C API

The binding differs from the C API in the following respects:

* The application does not create or destroy a context. The extension creates the [`Prism` singleton](./prism-singleton.md) when it is loaded, and destroys it when it is unloaded.
* Backends are initialized when they are obtained, so the binding has no equivalent of `prism_backend_initialize`. An application retries after a failure by obtaining the backend again.
* Operations that can fail return a `PrismError` value, as described in [Error Values](./errors.md), and applications SHOULD inspect it. Getters and setters instead report failures through Godot's error log.
* A backend is released when the last reference to it is released, and any speech in progress is stopped if the backend supports doing so. An application MUST therefore retain a reference to a backend for as long as it expects speech to continue, which generally means holding it in a member variable rather than a local variable.
* Availability changes and log messages are delivered through signals on the main thread, not through callbacks on Prism's internal threads. A signal MAY consequently be emitted slightly after the event it reports.

Backend identifiers are represented as `int` values. Since Godot's integers are signed, an identifier whose highest bit is set is represented as a negative number, and SHOULD be treated as opaque.

The binding consists of two classes: [`GodotPrismContext`](./prism-singleton.md), the class of the `Prism` singleton, of which applications SHOULD NOT create further instances, and [`GodotPrismBackend`](./backend.md), a backend instance.
