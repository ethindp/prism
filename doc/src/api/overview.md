# C API Reference

## Introduction

The C API is how Prism clients consume the Prism library. The C++ interface is internal and never exposed to front-end users.

The C API consists of three layers: context management, the backend registry, and individual backends.

A Prism context encapsulates access to the backend registry and serves as the root object for all Prism operations. Applications MUST create a context before performing any other Prism operations and SHOULD destroy it when Prism functionality is no longer needed.

The backend registry is the central authority for which backends are available on the platform Prism is running on. Backends are sorted in priority order, with highest-priority backends coming before lower-priority ones.

Backends are individual TTS engines or screen readers. Backends are retrieved from the backend registry. Backends are identified by a unique 64-bit ID, distinct from all other backends. The invalid ID, `0`, or `PRISM_BACKEND_INVALID`, is an always-invalid backend ID and will never be returned by the registry or used by a backend, and is used to signify internal errors where an API does not return a `PrismError`.
