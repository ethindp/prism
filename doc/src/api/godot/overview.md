# Godot API Reference

## Introduction

The Godot binding is distributed as an add-on, which requires Godot 4.5 or later. Installation is described in the Getting Started chapter.

The binding consists of two classes: `GodotPrismContext`, of which the extension registers one instance as the singleton `Prism`, and `GodotPrismBackend`. Both are documented in the editor's help. The chapters that follow are generated from the same class reference. Requirements of individual backends are given in the [backend-specific notes](../c/backend-notes.md).

## Correspondence with the C API

The `Prism` singleton owns a context, so the application neither creates nor destroys one. Registry functions are methods of the singleton, and backend functions are methods of `GodotPrismBackend`, named without their prefix: `prism_registry_create_best(ctx)` is `Prism.create_best()`, and `prism_backend_speak(backend, text, interrupt)` is `backend.speak(text, interrupt)`. A pair of functions that gets and sets a value is a property, as `backend.volume` is for `prism_backend_get_volume` and `prism_backend_set_volume`.

Constants keep their C names and are constants of the class that uses them. `PrismError` values and `PrismBackendFeature` flags are constants of `GodotPrismBackend`, as in `GodotPrismBackend.PRISM_OK`. `PrismLogLevel` values are constants of `GodotPrismContext`, as in `GodotPrismContext.PRISM_LOG_LEVEL_WARN`. Backend identifiers are constants of `GodotPrismContext` without the `PRISM_BACKEND_` prefix: `PRISM_BACKEND_NVDA` is `GodotPrismContext.NVDA`.

Backend identifiers are `int` values. Godot integers are signed, so an identifier whose highest bit is set is negative. Identifiers SHOULD be treated as opaque.

## Differences from the C API

* Backends are initialized when they are obtained. There is no equivalent of `prism_backend_initialize`; to retry after a failure, obtain the backend again.
* Methods that can fail return a `PrismError` value. Property getters and setters print errors instead.
* A backend is released with its last reference, and speech in progress is stopped first if the backend supports stopping. A reference MUST be kept for as long as speech is expected to continue.
* Availability changes and log messages are delivered through signals emitted on the main thread.
* Custom backends cannot be written in GDScript. Plugins can be loaded from shared libraries, including from the project's PCK.
* `speak_to_stream` synthesizes speech to an `AudioStreamWAV`.
