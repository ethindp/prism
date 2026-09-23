## Backend-Specific Notes

This chapter documents preconditions that go beyond the requirement that a backend's underlying speech or accessibility service be running. Backends not listed here have no preconditions beyond that one. Membership in a registry indicates only that a backend was compiled into the library or added at context initialization time, not that its preconditions are met. Applications SHOULD inspect `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` via `prism_backend_get_features` before invoking `prism_backend_initialize`. The flag reflects only the state at the moment of the call; `prism_backend_initialize` MAY still return `PRISM_ERROR_BACKEND_NOT_AVAILABLE` after a positive probe. Backend-specific error codes returned by `prism_backend_initialize` are documented in the relevant section below.

Some backends are designated legacy. Legacy backends are compiled into the official Prism release packages by default. They are NOT compiled into ordinary source builds; enabling a legacy backend in a source build requires the `PRISM_ENABLE_LEGACY_BACKENDS` build option together with the per-backend option named in the affected section.

Note: if an application wishes to be notified asynchronously of a backends availability as it changes, it should utilize the backend availability enumeration system described previously in this manual.

### AVSpeech

All `PRISM_BACKEND_AV_SPEECH` operations MUST execute in a context that can dispatch work to the host process's main thread. When the calling thread is the main thread, the backend executes work inline. When the calling thread is a worker thread, the backend dispatches work to the main queue and blocks until that work completes. Applications using a worker thread MUST therefore ensure that the main thread is running an AppKit, UIKit, or Foundation run loop, and MUST NOT block the main thread while a Prism call from a worker thread is in flight. Programs entered through `NSApplicationMain`, `UIApplicationMain`, or the SwiftUI `@main` attribute satisfy the run-loop requirement. A single-threaded program that issues all Prism calls from its entry-point thread also satisfies it. A headless multithreaded program that has no application object MUST run the main run loop itself.

On macOS 14, iOS 17, and corresponding or later releases of other Apple platforms, initialization requests Personal Voice authorization. The request is asynchronous and waits up to 120 seconds for the user's response, during which `prism_backend_initialize` blocks. The outcome does not affect whether initialization succeeds: on denial or timeout the backend operates with system-installed voices, and Personal Voice voices are absent from the voice list. Consumers that wish to control the timing of the prompt SHOULD invoke `requestPersonalVoiceAuthorization` before initializing the backend.

On systems supporting in-memory rendering (macOS 10.15 or later, iOS 13 or later, and corresponding releases of other Apple platforms), `prism_backend_speak_to_memory` is bounded by an internal 5-minute timeout. If synthesis does not complete within that interval, Prism stops the memory synthesizer and returns `PRISM_ERROR_INTERNAL`; partial audio is not delivered to the callback.

### VoiceOver

#### macOS

The host process MUST own at least one `NSWindow`. The backend selects a window from the application's window list using the following preference order:

* The application's key window, if it is visible.
* The application's main window, if it is visible.
* The first window in the application's ordered window list that is visible, not miniaturized, and at `NSNormalWindowLevel`.
* The first window in the application's window list that has a content view.
* The first window in the application's window list, irrespective of state.

In the case of the final two selections listed above, the chosen window need not be visible. An offscreen `NSWindow` of any size suffices. An `NSApplication` instance MUST exist and the main run loop MUST be active. `NSApplicationMain` and the SwiftUI `@main` attribute on a `SwiftUI.App` declaration satisfy these requirements. A console-style program that does not bring up AppKit does not, regardless of whether it is wrapped in an application bundle.

The backend has two delivery mechanisms for announcements: a legacy automation path that sends Apple events to VoiceOver, and a standard accessibility-announcement path that posts an announcement notification on a window owned by the host process. The legacy path requires that the host process be packaged as an application bundle whose `Info.plist` declares a non-empty value for `NSAppleEventsUsageDescription`, and that the user has approved automation access to VoiceOver. The string supplied for `NSAppleEventsUsageDescription` is shown verbatim in the Transparency, Consent, and Control prompt. The backend does not verify these conditions in advance; it discovers them when it first attempts to send an Apple event. The legacy path is disabled for the lifetime of the backend instance under either of the following conditions:

* The helper script does not compile during initialization; or
* The first Apple event, sent at the first speech request, is rejected because automation has not been permitted. This is the case both when the user denies the prompt and when the host process cannot request automation access at all, such as when it is not packaged as an application bundle or supplies no `NSAppleEventsUsageDescription`.

The standard macOS accessibility-announcement API does not provide Prism with a completion notification or a way to cancel an announcement after it has been posted. Because the backend may fall back to that path on any speech request, the macOS backend does not advertise `PRISM_BACKEND_FEATURE_SUPPORTS_IS_SPEAKING` or `PRISM_BACKEND_FEATURE_SUPPORTS_STOP`; calls to the corresponding operations return `PRISM_ERROR_NOT_IMPLEMENTED`.

#### iOS, iPadOS, MacCatalyst, tvOS, and visionOS

The host process MUST be a fully initialized application of the platform's standard application kind, with an active main run loop. Any process that uses `UIApplicationMain` or the SwiftUI `@main` entry point satisfies this requirement.

#### WatchOS

On watchOS, the VoiceOver backend requires watchOS 10.0 or later and is available only while VoiceOver is running. Initialization returns PRISM_ERROR_BACKEND_NOT_AVAILABLE if either requirement is not satisfied.

The watchOS VoiceOver backend supports the speak and output operations. No other operations are supported at this time.

### UIA

The UIA backend delivers announcements through Microsoft UI Automation notification events. Current versions of NVDA, JAWS, and Windows Narrator all observe these notifications.

A notification event produces speech only when an assistive technology consumes it. The backend therefore reports `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` only when both of the following hold at the moment of the query:

* The system screen reader flag, as retrieved through `SystemParametersInfo` with `SPI_GETSCREENREADER`, is set.
* `UiaClientsAreListening` returns `TRUE`, indicating that at least one UI Automation client is listening for events.

If `UiaClientsAreListening` cannot be resolved from `uiautomationcore.dll` at runtime, the bit is never reported. Neither check activates a COM object, and neither requires COM to be initialized on the calling thread.

The bit remains advisory, as for every other backend. Neither condition establishes that the listening client consumes notification events, or that the software which set the system screen reader flag is still running; the bit MAY therefore be reported when no announcement will be heard. Conversely, an assistive technology that consumes notification events without setting the system screen reader flag is not detected.

In order for UIA to function, the host process MUST own at least one top-level window that, at the moment of initialization, satisfies all of the following:

* `IsWindow` returns `TRUE`.
* `IsWindowVisible` returns `TRUE`.
* `IsIconic` returns `FALSE`.
* The window is owned by the host process.
* The window is not styled with the extended styles `WS_EX_TOOLWINDOW` or `WS_EX_TOPMOST`.
* The window is not a console host window, including the window kind used by modern terminal hosts such as Windows Terminal.

Among the windows that satisfy the conditions above, the backend selects the first available of:

* The foreground window, if it belongs to the host process;
* The host process's active window; and
* The first qualifying window in the host process's window order.

The root owner of the selected window MUST also belong to the host process; if it does not, `prism_backend_initialize` returns `PRISM_ERROR_INVALID_PARAM`.

The host process MUST also be running on an interactive desktop reachable through `GetThreadDesktop`. Services running in non-interactive sessions cannot use this backend even if they synthesize their own window. If no qualifying window can be found, `prism_backend_initialize` returns `PRISM_ERROR_INVALID_PARAM`. If a qualifying window is found but the backend's internal worker fails to initialize within a 5-second deadline, `prism_backend_initialize` returns `PRISM_ERROR_INTERNAL`.

The backend captures the qualifying window at initialization and does not rebind to a different window thereafter. If the captured window is destroyed during the lifetime of the backend instance, subsequent operations return `PRISM_ERROR_NOT_INITIALIZED`, and applications SHOULD destroy and re-create the backend.

### NVDA

The NVDA backend communicates with NVDA through a local RPC endpoint whose name is keyed by the user's logon session identifier and the calling thread's desktop name. NVDA MUST therefore be running in the same logon session and on the same desktop as the calling thread. Cross-session and cross-desktop announcements are not supported.

The endpoint binding established at initialization is fixed for the lifetime of the backend instance and is never retried. If NVDA terminates after a successful initialization, subsequent operations return `PRISM_ERROR_INTERNAL`. The backend does not automatically reconnect when NVDA is restarted; applications SHOULD destroy and re-create the backend.

### JAWS

The JAWS backend communicates with JAWS through a COM interface registered by the JAWS installer. The host process MUST be running on Windows, and the COM interface MUST be registered on the system; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### ZoomText

The ZoomText backend communicates with ZoomText through a COM interface registered by the ZoomText installer. The host process MUST be running on Windows, and the COM interface MUST be registered on the system; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### SenseReader

The Sense Reader backend communicates with Sense Reader through a COM interface registered by the Sense Reader installer. The host process MUST be running on Windows, and the COM interface MUST be registered on the system; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### PCTalker

The PC-Talker backend communicates with PC-Talker through a client library distributed with PC-Talker. The host process MUST be running on Windows, and the library MUST be resolvable through the host process's library search order; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### ZDSR

The ZDSR backend communicates with ZDSR through a client library distributed with ZDSR. The host process MUST be running on Windows, and the library MUST be resolvable through the host process's library search order; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`. The runtime-supported probe reflects only the presence of a running ZDSR process and does not load the library.

### BoyPCReader

The Boy PC Reader backend communicates with Boy PC Reader through a client library distributed with Boy PC Reader. The host process MUST be running on Windows, and the library MUST be resolvable through the host process's library search order; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`. As with the ZDSR backend, the runtime-supported probe reflects only the presence of a running Boy PC Reader process and does not load the library.

### SystemAccess

The System Access backend is a legacy backend. No other requirements are imposed.

### WindowEyes

The Window Eyes backend is a legacy backend. The backend communicates with WindowEyes through a COM interface registered by the WindowEyes installer. The host process MUST be running on Windows, and the COM interface MUST be registered on the system; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### Speech Dispatcher

The Speech Dispatcher backend is available natively on Linux and BSD and through a Wine bridge to Windows applications running under Wine or Proton. The native and bridged variants use the same backend identifier and expose the same public Prism API. For more information about using the wine bridges, see the Wine Bridges chapter for build, deployment, and selection requirements.

The native variant's runtime-supported probe is a non-blocking connection attempt to the configured Speech Dispatcher socket address. The probe does not perform an SSIP handshake. The connection address is taken from the `SPEECHD_ADDRESS` environment variable if set, and from the platform default address otherwise. Most current Linux distributions configure their service manager to spawn Speech Dispatcher on first client connection. On systems without automatic spawning, the daemon MUST be started before the backend is initialized.

For the Wine variant, Speech Dispatcher MUST be reachable from the Unix session in which Wine is running. A Wine or Proton environment that cannot reach the user's Speech Dispatcher service does not satisfy this requirement, even if the service is running elsewhere on the host.

### Orca

The Orca backend is available natively on Linux and BSD and through a Wine bridge to Windows applications running under Wine or Proton. The native and bridged variants use the same backend identifier and expose the same public Prism API. For more information about using the wine bridges, see the Wine Bridges chapter for build, deployment, and selection requirements.

The native variant's runtime-supported probe issues a `NameHasOwner` query against the session bus for the well-known names `org.gnome.Orca1.Service` and `org.gnome.Orca.Service`, in that order. For the first name that is owned, the probe issues a further method call on the matching speech-control interface to confirm that the remote-control interface is actually present. A process that owns one of the well-known names but does not answer on the speech-control interface is not reported as runtime-supported, and `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE` against it. The probe requires that the session bus be available to the host process. Headless environments and minimal SSH sessions that do not provide a session bus do not satisfy this requirement.

The backend supports two variants of the Orca remote-control interface: the current one, available in Orca 50 onwards, and a legacy one, which Orca used when the remote control feature was first introduced. The backend will automatically select whichever version the running Orca instance exposes. Backend initialization SHALL fail on an Orca release which does not implement the interface according to the semantics of D-bus. The Wine variant has the same requirement that the Orca service and its speech-control interface be reachable from the Unix session in which Wine is running.

### Spiel

The Spiel backend is available natively on Linux and BSD and through a Wine bridge to Windows applications running under Wine or Proton. In either form, Spiel communicates with speech providers associated with the user's session rather than synthesizing audio itself. The native and bridged versions of the backend support the same API. For more information about using the wine bridges, see the Wine Bridges chapter for build, deployment, and selection requirements.

The runtime-supported probe requires a session D-Bus connection and at least one currently owned or activatable D-Bus service whose name ends in `.Speech.Provider`. Initialization will require that any speech providers be genuinely reachable. Headless environments and minimal SSH sessions that do not provide a session bus do not satisfy this requirement. For the Wine variant, the same session services MUST be reachable from the Unix session in which Wine is running.

A single Spiel voice that declares support for multiple languages appears in the backend's voice list as one entry per language, sharing the same name but with distinct language strings. The voice index selected through `prism_backend_set_voice` therefore identifies a `(voice, language)` pair, not a voice alone.

Note: the native Spiel backend is not currently enabled in release packages or the Python wheels because Spiel is not in the distribution package repositories used to build those artifacts at this time. If you wish to use the native backend on a system where it is not packaged, you will need to build Prism and Spiel from source by hand or using a tool such as [vcpkg](https://github.com/microsoft/vcpkg). Wine bridge availability is determined separately by whether the bridge supplied with the build contains Spiel and whether a compatible provider is reachable at run time.

### Android Text to Speech

The Prism Android library's application context MUST be initialized in the host process before any operation is invoked. When Prism is consumed as a standard Android library archive (AAR) through Gradle or another mechanism that respects the merged manifest, this initialization is performed automatically by a `ContentProvider` registered in the library's manifest, whose `onCreate` method runs before the application's is executed. Consumers that repackage the library in a way that drops the merged manifest entries MUST initialize the application context manually before using any Android backend.

The calling thread MUST be attached to the Java VM. This is automatic for threads created by the platform runtime. Native threads created outside the runtime MUST attach themselves through `JavaVM::AttachCurrentThread` before invoking Prism. Initialization performs the platform's text-to-speech engine handshake, which is asynchronous, and waits up to 10 seconds for the engine to report readiness. An engine that fails to report readiness within the deadline causes `prism_backend_initialize` to return `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### Android Screen Reader

The application-context and Java VM attachment requirements of the Android Text to Speech backend also apply to the Android Screen Reader backend. Accessibility services MUST be enabled on the device, and at least one accessibility service capable of spoken feedback MUST be enabled in the user's accessibility settings.

### Web Speech

In browsing contexts that enforce an autoplay policy, the first call to an instance of the web speech backend to produce speech MUST originate from a user-initiated event handler such as a `click` or `keydown` listener. This is a property of the browsing context, not of Prism. The set of available voices is populated asynchronously by the browser. The voice list reported by the backend MAY be briefly empty after initialization on first page load. Applications SHOULD invoke `prism_backend_refresh_voices` if the voice list is unexpectedly empty, or arrange to do so in response to the browser's `voiceschanged` event.