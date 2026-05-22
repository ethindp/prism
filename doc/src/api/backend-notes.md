## Backend-Specific Notes

This chapter documents preconditions that go beyond the requirement that a backend's underlying speech or accessibility service be running. Backends not listed here have no preconditions beyond that one. Membership in the registry indicates only that a backend was compiled into the library, not that its preconditions are met. Applications SHOULD inspect `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` via `prism_backend_get_features` before invoking `prism_backend_initialize`. The flag reflects only the state at the moment of the call; `prism_backend_initialize` MAY still return `PRISM_ERROR_BACKEND_NOT_AVAILABLE` after a positive probe. Backend-specific error codes returned by `prism_backend_initialize` are documented in the relevant section below.

Some backends are designated legacy. Legacy backends are compiled into the official Prism release packages by default. They are NOT compiled into ordinary source builds; enabling a legacy backend in a source build requires the `PRISM_ENABLE_LEGACY_BACKENDS` build option together with the per-backend option named in the affected section.

### AVSpeech

All `PRISM_BACKEND_AV_SPEECH` operations MUST execute in a context that can dispatch work to the host process's main thread. When the calling thread is the main thread, the backend executes work inline. When the calling thread is a worker thread, the backend dispatches work to the main queue and blocks until that work completes. Applications using a worker thread MUST therefore ensure that the main thread is running an AppKit, UIKit, or Foundation run loop, and MUST NOT block the main thread while a Prism call from a worker thread is in flight. Programs entered through `NSApplicationMain`, `UIApplicationMain`, or the SwiftUI `@main` attribute satisfy the run-loop requirement. A single-threaded program that issues all Prism calls from its entry-point thread also satisfies it. A headless multithreaded program that has no application object MUST run the main run loop itself.

On macOS 14, iOS 17, and corresponding or later releases of other Apple platforms, initialization requests Personal Voice authorization. The request is asynchronous and waits up to 120 seconds for the user's response, during which `prism_backend_initialize` blocks. The outcome does not affect whether initialization succeeds: on denial or timeout the backend operates with system-installed voices, and Personal Voice voices are absent from the voice list. Consumers that wish to control the timing of the prompt SHOULD invoke `requestPersonalVoiceAuthorization` before initializing the backend.

On systems supporting in-memory rendering (macOS 10.15 or later, iOS 13 or later, and corresponding releases of other Apple platforms), `prism_backend_speak_to_memory` is bounded by an internal 5-minute timeout. Utterances that exceed it are truncated.

### VoiceOver

#### macOS

The host process MUST own at least one `NSWindow`. The backend selects a window from the application's window list using the following preference order:

* The application's key window, if it is visible.
* The application's main window, if it is visible.
* The first window in the application's ordered window list that is visible, not miniaturized, and at `NSNormalWindowLevel`.
* The first window in the application's window list that has a content view.
* The first window in the application's window list, irrespective of state.

In the case of the final two selections listed above, the chosen window need not be visible. An offscreen `NSWindow` of any size suffices. An `NSApplication` instance MUST exist and the main run loop MUST be active. `NSApplicationMain` and the SwiftUI `@main` attribute on a `SwiftUI.App` declaration satisfy these requirements. A console-style program that does not bring up AppKit does not, regardless of whether it is wrapped in an application bundle.

The backend has two delivery mechanisms for announcements: a legacy automation path that sends Apple events to VoiceOver, and a standard accessibility-announcement path that posts an announcement notification on a window owned by the host process. The legacy path requires that the host process be packaged as an application bundle whose `Info.plist` declares a non-empty value for `NSAppleEventsUsageDescription`, and that the user has approved automation access to VoiceOver. The string supplied for `NSAppleEventsUsageDescription` is shown verbatim in the Transparency, Consent, and Control prompt. The legacy path is disabled for the lifetime of the backend instance under any of the following conditions:

* The helper script does not compile during initialization.
* The user denies the TCC prompt at the first speech request.
* The host process is not packaged as an application bundle.

When the legacy path is disabled, subsequent speech requests are routed through the standard accessibility-announcement path without producing an error.

#### iOS, iPadOS, tvOS, and visionOS

The host process MUST be a fully initialized application of the platform's standard application kind, with an active main run loop. Any process that uses `UIApplicationMain` or the SwiftUI `@main` entry point satisfies this requirement.

### UIA

The UIA backend delivers announcements through Microsoft UI Automation notification events. Current versions of NVDA, JAWS, and Windows Narrator all observe these notifications.

The host process MUST own at least one top-level window that, at the moment of initialization, satisfies all of the following:

* `IsWindow` returns `TRUE`.
* `IsWindowVisible` returns `TRUE`.
* `IsIconic` returns `FALSE`.
* The window is owned by the host process.
* The window is not styled with the extended styles `WS_EX_TOOLWINDOW` or `WS_EX_TOPMOST`.
* The window is not a console host window.

The host process MUST also be running on an interactive desktop reachable through `GetThreadDesktop`. Services running in non-interactive sessions cannot use this backend even if they synthesize their own window. If no qualifying window can be found, `prism_backend_initialize` returns `PRISM_ERROR_INVALID_PARAM`. If a qualifying window is found but the backend's internal worker fails to initialize within a 5-second deadline, `prism_backend_initialize` returns `PRISM_ERROR_INTERNAL`.

The backend captures the qualifying window at initialization and does not rebind to a different window thereafter. If the captured window is destroyed during the lifetime of the backend instance, subsequent operations return `PRISM_ERROR_NOT_INITIALIZED`, and applications SHOULD destroy and re-create the backend.

### NVDA

The NVDA backend communicates with NVDA through a local RPC endpoint whose name is keyed by the user's logon session identifier and the calling thread's desktop name. NVDA MUST therefore be running in the same logon session and on the same desktop as the calling thread. Cross-session and cross-desktop announcements are not supported.

The endpoint binding established at initialization is fixed for the lifetime of the backend instance and is never retried. If NVDA terminates after a successful initialization, subsequent operations return `PRISM_ERROR_BACKEND_NOT_AVAILABLE`. The backend does not automatically reconnect when NVDA is restarted; applications SHOULD destroy and re-create the backend.

### SystemAccess

The System Access backend is a legacy backend. In addition to `PRISM_ENABLE_LEGACY_BACKENDS`, source builds that wish to include it MUST set `PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND`. No other requirements are imposed.

### WindowEyes

The Window Eyes backend is a legacy backend. In addition to `PRISM_ENABLE_LEGACY_BACKENDS`, source builds that wish to include it MUST set `PRISM_ENABLE_LEGACY_WINDOW_EYES_BACKEND`. The backend communicates with WindowEyes through a COM interface registered by the WindowEyes installer. The host process MUST be running on Windows, and the COM interface MUST be registered on the system; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### Speech Dispatcher

The speech dispatcher backend is registered in two variants:

* The native variant is registered on Linux and BSD builds, links against `libspeechd`, and connects directly to a local speech-dispatcher daemon over its SSIP protocol. It reports the full feature set: speech output, voice management, pause and resume, and rate, pitch, and volume controls.
* The Wine bridge variant is registered on Win32 builds with `PRISM_BUILD_WINELIBS` set, and is runtime-supported only when the host process is running under Wine. It bridges from the Win32 host through a Winelib component into a Linux-side speech-dispatcher reachable from the host process's WINE prefix. It reports only speech output and stop.

The native variant's runtime-supported probe is a non-blocking connection attempt to the configured speech-dispatcher socket address. The probe does not perform an SSIP handshake, so a daemon that accepts connections but rejects the handshake will appear runtime-supported but cause `prism_backend_initialize` to return `PRISM_ERROR_BACKEND_NOT_AVAILABLE`. The connection address is taken from the `SPEECHD_ADDRESS` environment variable if set, and from the platform default address otherwise. Most current Linux distributions configure their service manager to spawn speech-dispatcher on first client connection. On systems without automatic spawning, the daemon MUST be started before the backend is initialized.

### Orca

The Orca backend is registered in two variants:

* The native variant is registered on Linux and BSD builds, links against GIO, and connects directly to an Orca service on the session bus.
* The Wine bridge variant is registered on Win32 builds with `PRISM_BUILD_WINELIBS` set, and is runtime-supported only when the host process is running under Wine. It bridges from the Win32 host through a Winelib component into a Linux-side session bus reachable from the host process's WINE prefix.

The native variant's runtime-supported probe issues a `NameHasOwner` query against the session bus for the well-known names `org.gnome.Orca1.Service` and `org.gnome.Orca.Service`, in that order. The probe requires that the session bus be available to the host process. Headless environments and minimal SSH sessions that do not provide a session bus do not satisfy this requirement. The remote-controller D-Bus interface was introduced in Orca version 49.0. Earlier versions of Orca do not provide it, and `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE` against those versions even when the well-known name is present.

### Spiel

The Spiel backend is registered on Linux and BSD builds and communicates with Spiel speech providers through the session bus. The backend does not itself synthesize audio; synthesis is performed by whichever speech provider handles the utterance.

The runtime-supported probe requires that the session bus be available to the host process and that at least one Spiel speech provider be reachable on it. Headless environments and minimal SSH sessions that do not provide a session bus do not satisfy this requirement.

A single Spiel voice that declares support for multiple languages appears in the backend's voice list as one entry per language, sharing the same name but with distinct language strings. The voice index selected through `prism_backend_set_voice` therefore identifies a `(voice, language)` pair, not a voice alone.

Note: the Spiel backend is not currently enabled in release packages or the Python wheels because it is not in any distribution package repositories at this time. As such, enabling it would break loading of the library for apps. Once this situation is resolved, it will be re-enabled. If you wish to have access to the backend on your machine, you will need to build Prism and Spiel from source by hand or using a tool such as [vcpkg](https://github.com/microsoft/vcpkg).

### Android Text to Speech

The Prism Android library's application context MUST be initialized in the host process before any operation is invoked. When Prism is consumed as a standard Android library archive (AAR) through Gradle or another mechanism that respects the merged manifest, this initialization is performed automatically by a `ContentProvider` registered in the library's manifest, whose `onCreate` method runs before the application's is executed. Consumers that repackage the library in a way that drops the merged manifest entries MUST initialize the application context manually before using any Android backend.

The calling thread MUST be attached to the Java VM. This is automatic for threads created by the platform runtime. Native threads created outside the runtime MUST attach themselves through `JavaVM::AttachCurrentThread` before invoking Prism. Initialization performs the platform's text-to-speech engine handshake, which is asynchronous, and waits up to 10 seconds for the engine to report readiness. An engine that fails to report readiness within the deadline causes `prism_backend_initialize` to return `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### Android Screen Reader

The application-context and Java VM attachment requirements of the Android Text to Speech backend also apply to the Android Screen Reader backend. Accessibility services MUST be enabled on the device, and at least one accessibility service capable of spoken feedback MUST be enabled in the user's accessibility settings. Both conditions are rechecked on every speech request. A transition of either condition to false during the lifetime of the backend causes subsequent operations to return `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### Web Speech

In browsing contexts that enforce an autoplay policy, the first call to an instance of the web speech backend to produce speech MUST originate from a user-initiated event handler such as a `click` or `keydown` listener. This is a property of the browsing context, not of Prism. The set of available voices is populated asynchronously by the browser. The voice list reported by the backend MAY be briefly empty after initialization on first page load. Applications SHOULD invoke `prism_backend_refresh_voices` if the voice list is unexpectedly empty, or arrange to do so in response to the browser's `voiceschanged` event.