# Wine Bridges

Prism provides Wine bridges so that a Windows application using Prism under Wine or Proton can use speech and accessibility services supplied by the Linux or BSD host. The bridge is transparent to the application. A bridged backend has the same backend name, backend identifier, feature reporting, initialization rules, and public C API as the corresponding backend on a native platform. Applications MUST NOT call or load a bridge component directly; they select the backend through the registry and use the ordinary `prism_backend_*` functions.

The bridge mechanism applies only to Windows builds of Prism running under Wine or Proton. It does not make a Linux backend available to a program running on native Windows nor does it change the behavior of the corresponding native Linux or BSD backend. A backend may be present in a Windows registry even when its bridge cannot be used in the current process. Applications SHOULD therefore treat registry membership and runtime availability as separate questions, as they do for every other Prism backend.

## Supported backends

The following backends can operate through a Wine bridge:

| Backend | Host requirement | Bridged capabilities |
| --- | --- | --- |
| Orca | A usable desktop session bus and a running Orca release exposing its remote-control speech interface. | Speech, combined output, and stop. |
| Speech Dispatcher | A Speech Dispatcher service reachable from the Unix session in which Wine is running. | Speech, combined output, stop, pause and resume, rate, pitch and volume control, and voice enumeration, selection, and refresh. |
| Spiel | A usable desktop session bus and at least one reachable Spiel speech provider. | Speech, combined output, stop, pause and resume, speaking-state queries, rate, pitch and volume control, and voice enumeration, selection, and refresh. |

The host requirement is evaluated at run time. A bridge being installed does not imply that the service behind it is available, and a successful availability probe does not guarantee that the service will remain available until initialization or for the lifetime of the backend instance. Applications MUST handle `PRISM_ERROR_BACKEND_NOT_AVAILABLE` and other ordinary backend errors in the same manner as they would on a native platform.

Wine and Proton may run applications inside environments that restrict access to the host user's session services. In such an environment, a bridge is unavailable if the required service cannot be reached from the Wine process, even if that service is running elsewhere on the host. Prism does not require applications to distinguish this case from the service simply not running.

## Building the bridges

Wine bridges are built on an x86 or x86-64 Linux or BSD host by enabling `PRISM_BUILD_WINELIBS`. The build host MUST provide a Wine development toolchain. A bridge is produced only for a backend whose host-side development dependency is available to the bridge build; consequently, a build MAY contain some of the bridges listed above and omit others.

A normal source build can request the bridge payload as follows:

```text
cmake -S . -B build -DPRISM_BUILD_WINELIBS=ON
cmake --build build
```

`PRISM_BUILD_WINELIBS` builds the Wine-side payload in addition to the ordinary build performed on that Unix host. A Windows application still requires a Windows Prism library of the appropriate architecture. The Wine bridge payload and the Windows Prism library SHOULD come from the same Prism release; applications and distributors MUST NOT assume that bridge components from different Prism releases are interchangeable, and the APIs of the bridges is intentionally unspecified.

The installed bridge payload is placed under the install tree's `wine` directory. Distributors SHOULD preserve that payload as a unit rather than renaming individual files or combining files from different builds. The Wine-visible portions of the payload MUST be discoverable by Wine when the application starts. How a distributor arranges Wine's module search path is outside Prism's API contract and may vary between a system Wine installation, a private prefix, and Proton. Common deployment scenarios are discussed in the following sections.

## Deploying the bridge

A Wine bridge consists of two files which serve different purposes and MUST be deployed together. The file whose name ends in `.dll` is the Windows-visible placeholder. Wine must be able to find this file through the ordinary Windows DLL search path. The corresponding file whose name ends in `.dll.so` is the host-side Wine module. Wine must be able to find this file through `WINEDLLPATH`.

The installed Prism payload places the host-side `.dll.so` files in the `wine` directory and the corresponding placeholder `.dll` files in `wine/placeholders`. Only bridges which were successfully built will be present. The files MUST NOT be renamed. A placeholder and its corresponding `.dll.so` MUST come from the same Prism build and MUST have an architecture compatible with the Windows Prism library and application using them.

The placeholder is not a replacement for the `.dll.so`, nor is the `.dll.so` a replacement for the placeholder. Copying only the placeholder allows Wine to locate the Windows-visible module name but does not provide the bridge itself. Making only the `.dll.so` available through `WINEDLLPATH` is likewise insufficient if Wine cannot locate the corresponding `.dll` through its Windows DLL search path.

### Application-local deployment

For an application distributed as a self-contained directory, the RECOMMENDED arrangement is to copy the placeholder DLLs from Prism's `wine/placeholders` directory into the directory containing the application's executable. The matching `.dll.so` files SHOULD be kept in a separate Unix-visible directory, such as a `wine` subdirectory of the application installation.

For example, an application using all three bridged backends would place `prism_orca_bridge.dll`, `prism_speech_dispatcher_bridge.dll`, and `prism_spiel_bridge.dll` beside `application.exe` and `prism.dll`, or if the application statically links Prism, just next to `application.exe`. The corresponding `prism_orca_bridge.dll.so`, `prism_speech_dispatcher_bridge.dll.so`, and `prism_spiel_bridge.dll.so` files could be placed in a `wine` subdirectory.

The application is then launched with the directory containing the `.dll.so` files included in `WINEDLLPATH`. A launcher script might resemble the following:

```sh
#!/bin/sh

APPDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export WINEDLLPATH="$APPDIR/wine${WINEDLLPATH:+:$WINEDLLPATH}"
exec wine "$APPDIR/application.exe" "$@"
```

If the application uses a particular Wine prefix, the launcher MAY set `WINEPREFIX` in the ordinary manner as well:

```sh
#!/bin/sh

APPDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export WINEPREFIX="$APPDIR/prefix"
export WINEDLLPATH="$APPDIR/wine${WINEDLLPATH:+:$WINEDLLPATH}"
exec wine "$APPDIR/application.exe" "$@"
```

An existing `WINEDLLPATH` SHOULD be preserved as shown above rather than replaced outright, as it may contain other libraries the application requires. The bridge directory may be located anywhere on the Unix filesystem; it does not need to reside inside the Wine prefix.

Application-local deployment is generally preferable because it keeps the Prism version, bridge payload, and application together. It also permits two applications requiring different Prism releases to coexist without replacing one another's bridge files.

### Deployment into a private Wine prefix

An application which owns an entire Wine prefix MAY instead install the placeholder files into the prefix's Windows system directories. The `.dll.so` files remain Unix files and SHOULD be stored in a separate directory which is added to `WINEDLLPATH`.

For a 64-bit application in a 64-bit Wine prefix, the 64-bit placeholders may be placed in `$WINEPREFIX/drive_c/windows/system32`. For a 32-bit application in a 64-bit Wine prefix, the 32-bit placeholders belong in `$WINEPREFIX/drive_c/windows/syswow64`. In a purely 32-bit prefix, the 32-bit placeholders are placed in `system32`.

The corresponding host-side `.dll.so` files MAY, for example, be stored in a `prism-wine` directory beneath the prefix or in another package-owned directory on the Unix filesystem. Whichever directory is chosen must then be included in `WINEDLLPATH` when the application is launched:

```sh
WINEPREFIX="/path/to/prefix" \
WINEDLLPATH="/path/to/prefix/prism-wine${WINEDLLPATH:+:$WINEDLLPATH}" \
wine application.exe
```

Placing the placeholders in the prefix system directories makes them available to applications throughout that prefix. A distributor SHOULD therefore use this arrangement only when it controls the prefix or deliberately wishes to make one Prism bridge payload shared by every application in it. Application-local deployment is safer when unrelated programs share a prefix.

The `.dll.so` files SHOULD NOT be copied into `drive_c`, `system32`, or `syswow64`, as they are host-side modules and are located through `WINEDLLPATH`, not through the Windows filesystem search path.

### Wine launchers and desktop applications

A desktop launcher, package wrapper, shell script, or other program which starts the Windows application MAY set `WINEDLLPATH` instead of requiring the user to configure it globally. This is generally preferable to placing Prism's bridge modules into Wine's own installation directories.

For example:

```sh
export WINEDLLPATH="/opt/example/lib/prism/wine${WINEDLLPATH:+:$WINEDLLPATH}"
exec wine "/opt/example/share/example/application.exe"
```

A system package might therefore install the host-side bridge modules beneath a package-owned directory such as `/usr/lib/example/prism-wine`, while placing the placeholders in the application's directory or private Wine prefix. Its launcher would add the host-side directory to `WINEDLLPATH`.

Distributors SHOULD prefer a package-owned bridge directory over copying the `.dll.so` files into Wine's own library directories. Modifying Wine's installation couples the Prism package to the layout and package-management policy of a particular Wine distribution and makes coexistence of different Prism versions substantially more difficult.

### Proton and game-local deployment

The application-local arrangement is also suitable for Proton. The placeholder DLLs SHOULD normally be placed beside the Windows executable, or otherwise in a location visible through that application's Windows DLL search path. The `.dll.so` files remain on the Unix side and the directory containing them must be present in `WINEDLLPATH` when Proton starts the application.

For launch systems which use `%command%` to represent the application's normal Proton command, an appropriate launch option can generally be written as:

```text
WINEDLLPATH="/absolute/path/to/prism-wine:$WINEDLLPATH" %command%
```

The precise location of a Proton prefix is insignificant to Prism. A distributor SHOULD NOT require the bridge `.dll.so` files to be installed into a particular Steam or Proton prefix directory when an application-local placeholder and an explicit `WINEDLLPATH` can be used instead. Proton versions and launchers may arrange their prefixes differently, whereas the bridge requirement remains simply that Wine can locate both halves of the payload.

## Using a bridged backend

No bridge-specific initialization function exists. An application uses the same registry and backend functions it uses everywhere else. In particular, it SHOULD query runtime support before initialization when selecting a specific bridged backend:

```c
PrismBackend *backend = prism_registry_create(ctx, PRISM_BACKEND_SPIEL);
if (backend != NULL) {
    const uint64_t features = prism_backend_get_features(backend);
    if ((features & PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME) != 0) {
        PrismError err = prism_backend_initialize(backend);
        if (err == PRISM_OK && err != PRISM_ERROR_ALREADY_INITIALIZED) {
            /* Use the backend through the ordinary Prism API. */
        }
    }
    prism_backend_free(backend);
}
```

An application that has no reason to prefer one of the bridged backends SHOULD use the ordinary best-backend selection functions instead of adding Wine-specific selection policy. The registry's priority ordering and initialization fallback rules apply normally. Code SHOULD NOT infer that it is running under Wine solely because one of these backend identifiers is present in the registry.

## Deployment requirements

The architecture of the Windows Prism library, the Windows application, the Wine environment, and the bridge payload MUST be compatible. Bridge generation is supported only on x86-family hosts. A 32-bit Windows application requires a bridge payload suitable for its Wine architecture, and a 64-bit Windows application requires the corresponding 64-bit payload.

The host services used by Orca and Spiel are normally associated with the logged-in graphical session, while Speech Dispatcher is normally associated with the user's speech session. A Wine application launched outside that session MAY therefore be unable to use a bridge that works when the same application is launched from the desktop. Service startup, D-Bus session configuration, Wine prefix policy, and Proton sandbox policy are deployment concerns and are not modified by Prism, and as such are outside the scope of this manual.

Applications do not need to ship Linux speech engines inside the Windows application. They do, however, need the matching Prism bridge payload to be available to Wine, and the host system needs the service required by the selected backend. If these requirements are not met, the backend remains usable as a registry entry but reports itself as not runtime-supported or fails initialization with an ordinary Prism error.
