# Prism .NET Bindings

This directory contains the .NET bindings for the Prism speech library.

## Prerequisites

Requires the .NET 10.0 SDK or later.

## Build Process

We have a script that handles the retrieval of pre-built binaries from GitHub releases. It identifies the latest release, downloads the SDK package, and extracts the appropriate shared libraries for supported platforms into a gitignored staging folder.

The Prism.Net build process is configured to execute the staging script automatically during the packaging process.
To build and package the library, use the following commands:

```
cd Prism.Net
dotnet pack
```

The script includes a cache that records the staged version in a version.txt file. Subsequent builds skip the download process if the local staging directory is up to date with the latest GitHub release.
