## Backend-Specific Notes

This chapter documents preconditions that go beyond the requirement that a backend's underlying speech or accessibility service be running. Backends not listed here have no preconditions beyond that one. Membership in a registry indicates only that a backend was compiled into the library or added at context initialization time, not that its preconditions are met. Applications SHOULD inspect `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` via `prism_backend_get_features` before invoking `prism_backend_initialize`. The flag reflects only the state at the moment of the call; `prism_backend_initialize` MAY still return `PRISM_ERROR_BACKEND_NOT_AVAILABLE` after a positive probe. Backend-specific error codes returned by `prism_backend_initialize` are documented in the relevant section below.

Some backends are designated legacy. Legacy backends are compiled into the official Prism release packages by default. They are NOT compiled into ordinary source builds; a source build compiles them when the `PRISM_ENABLE_LEGACY_BACKENDS` build option is enabled. Each MAY then be excluded through its own backend option, such as `PRISM_ENABLE_SYSTEM_ACCESS_BACKEND`, which cannot enable a legacy backend without `PRISM_ENABLE_LEGACY_BACKENDS`.

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

The backend has two delivery mechanisms for announcements: a legacy automation path that sends Apple events to VoiceOver, and a standard accessibility-announcement path that posts an announcement notification on a window owned by the host process. The legacy path requires that the host process be packaged as an application bundle whose `Info.plist` declares a non-empty value for `NSAppleEventsUsageDescription`, and that the user has approved automation access to VoiceOver. The string supplied for `NSAppleEventsUsageDescription` is shown verbatim in the Transparency, Consent, and Control prompt.

During initialization, if the host process is eligible for the legacy path, the backend requests automation access in the background. A host process is eligible if it is not sandboxed and is either not notarized or holds the `com.apple.security.automation.apple-events` entitlement. Initialization neither waits for the user's response nor fails because of it. A speech request uses the legacy path only when all of the following hold:

* VoiceOver is running;
* The most recent permission check, which runs in the background after each speech request and never prompts, found that automation access has been granted; and
* The host process is eligible.

Otherwise, the speech request uses the accessibility-announcement path. Because the permission check runs in the background, a change in automation access takes effect from the speech request after the one that observes it, without reinitializing the backend.

Apple events are sent from an internal background queue, in the order in which they were requested, and never block the main thread. If VoiceOver does not reply within 2 seconds, or if the event fails, the text is delivered through the accessibility-announcement path instead. A speech request whose event was merely slow MAY therefore be heard twice. After a reply times out, the backend does not use the legacy path for 30 seconds, and speech requests in that interval use the accessibility-announcement path.

The standard macOS accessibility-announcement API does not provide Prism with a completion notification or a way to cancel an announcement after it has been posted. Because the backend may fall back to that path on any speech request, the macOS backend does not advertise `PRISM_BACKEND_SUPPORTS_IS_SPEAKING` or `PRISM_BACKEND_SUPPORTS_STOP`; calls to the corresponding operations return `PRISM_ERROR_NOT_IMPLEMENTED`.

#### iOS, iPadOS, MacCatalyst, tvOS, and visionOS

The host process MUST be a fully initialized application of the platform's standard application kind, with an active main run loop. Any process that uses `UIApplicationMain` or the SwiftUI `@main` entry point satisfies this requirement.

#### WatchOS

On watchOS, the VoiceOver backend requires watchOS 10.0 or later and is available only while VoiceOver is running. Initialization returns PRISM_ERROR_BACKEND_NOT_AVAILABLE if either requirement is not satisfied.

The watchOS VoiceOver backend supports the speak and output operations. No other operations are supported at this time.

### UIA

The UIA backend delivers announcements through Microsoft UI Automation notification events. Current versions of NVDA, JAWS, and Windows Narrator all observe these notifications.

The backend reports `PRISM_BACKEND_REQUIRES_EXPLICIT_SELECTION`, and is therefore never selected by `prism_registry_create_best` or `prism_registry_acquire_best`. Its window requirements, described below, cannot be met by an arbitrary caller of those functions. NVDA and JAWS have dedicated backends that best selection reaches, but Windows Narrator does not: under best selection, a user of Narrator receives a speech engine backend, which speaks independently of Narrator. An application that wishes to reach Narrator MUST request the UIA backend by identifier or name.

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

The backend captures the root owner of the qualifying window at initialization and does not rebind to a different window thereafter. Because no window qualifies until it is visible and not minimized, an application SHOULD initialize the backend after its window has been shown. If the captured window is destroyed during the lifetime of the backend instance, subsequent operations return `PRISM_ERROR_NOT_INITIALIZED`, and applications SHOULD destroy and re-create the backend.

### NVDA

The NVDA backend communicates with NVDA through a local RPC endpoint whose name is keyed by the user's logon session identifier and the calling thread's desktop name. NVDA MUST therefore be running in the same logon session and on the same desktop as the calling thread. Cross-session and cross-desktop announcements are not supported.

The endpoint binding established at initialization is fixed for the lifetime of the backend instance and is never retried. If NVDA terminates after a successful initialization, subsequent operations return `PRISM_ERROR_BACKEND_NOT_AVAILABLE`. The backend does not automatically reconnect when NVDA is restarted; applications SHOULD destroy and re-create the backend.

### JAWS

The JAWS backend communicates with JAWS through a COM interface registered by the JAWS installer. The host process MUST be running on Windows, and the COM interface MUST be registered on the system; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### ZoomText

The ZoomText backend communicates with ZoomText through a COM interface registered by the ZoomText installer. The host process MUST be running on Windows, and the COM interface MUST be registered on the system; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### SenseReader

The Sense Reader backend communicates with Sense Reader through a COM interface registered by the Sense Reader installer. The host process MUST be running on Windows, and the COM interface MUST be registered on the system; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### PCTalker

The PC-Talker backend communicates with PC-Talker through a client library distributed with PC-Talker. The host process MUST be running on Windows, and the library MUST be resolvable through the host process's library search order; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### ZDSR

The ZDSR backend communicates with ZDSR through ZDSRAPI. The host process MUST be running on Windows, and the API library MUST be loadable; otherwise, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

The runtime-supported probe requires both `ZDSRMain.exe` and `ZDSRDaemon.exe` to be running. It only checks for those processes and does not load ZDSRAPI. The ZDSR service processes are not used as a proxy for the reader, because they can remain running when the reader is not.

The parameters in `ZDSRAPI.ini` take precedence over the arguments Prism passes to `InitTTS`. If `type=1` selects an independent channel, that channel may still be starting when initialization queries the speech state. In that transient state, ZDSR reports status 9 (independent channel not ready); Prism treats it as not speaking, so `prism_backend_initialize` succeeds and `prism_backend_is_speaking` returns `false`. A `prism_backend_speak` call made before the channel is ready returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`; applications MAY retry after the channel becomes ready. Reinitialization is not required.

Some ZDSR API versions do not export the `Braille` function. After initialization, the ZDSR backend advertises `PRISM_BACKEND_SUPPORTS_BRAILLE` only if the loaded API provides it. Otherwise `prism_backend_braille` returns `PRISM_ERROR_NOT_IMPLEMENTED`, and `prism_backend_output` outputs speech only. Before initialization, the ZDSR backend does not advertise Braille support.

### BoyPCReader

The Boy PC Reader backend communicates with Boy PC Reader through a client library distributed with Boy PC Reader. The host process MUST be running on Windows, and the library MUST be resolvable through the host process's library search order; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`. As with the ZDSR backend, the runtime-supported probe reflects only the presence of a running Boy PC Reader process and does not load the library.

### SystemAccess

The System Access backend is a legacy backend. No other requirements are imposed.

### WindowEyes

The Window Eyes backend is a legacy backend. The backend communicates with WindowEyes through a COM interface registered by the WindowEyes installer. The host process MUST be running on Windows, and the COM interface MUST be registered on the system; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### SAPI

The SAPI backend uses SAPI 5 for synthesis only. Synthesized audio is played through XAudio2; SAPI's own audio output is not used. The host process MUST be running on Windows, and the SAPI voice COM class MUST be registered; if it is not, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`. An audio output device that XAudio2 can open MUST be present when the backend is initialized; if none is present, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

The backend creates and exclusively uses its SAPI voice on an internal worker thread in the multithreaded apartment. Accordingly, the thread from which the backend is called MAY belong to any apartment. `prism_backend_speak_to_memory`, `prism_backend_set_voice`, and `prism_backend_refresh_voices` are executed on the worker thread and return when the worker has completed them. No other function waits for synthesis.

`prism_backend_speak` returns once the text has been queued. Each utterance is synthesized in its entirety before playback of it begins. From the return of `prism_backend_speak` until playback of the utterance ends, `prism_backend_is_speaking` reports `true`, unless speech is paused or stopped in the interim. Synthesis of an utterance that is stopped before its synthesis completes is abandoned. An utterance that cannot be synthesized is discarded, and the failure is reported through the logging facility.

The voice list comprises the voices registered under the SAPI voice token category and, where present, the voices of the Microsoft Speech Platform 11 runtime. If neither category contains a voice, `prism_backend_initialize` returns `PRISM_ERROR_NO_VOICES`. Each voice is synthesized in the audio format that SAPI reports for it. Where an implementation of SAPI reports no format, 16-bit mono PCM at 22050 Hz is used.

The rate, pitch, and voice in effect when `prism_backend_speak` is called apply to that utterance; subsequent changes affect only utterances queued thereafter. Pitch is applied through SAPI XML markup. Text passed to `prism_backend_speak` is escaped and SHALL NOT be interpreted as markup. Volume is applied during playback, and a change of volume therefore also affects the utterance being played.

When the default audio output device changes, playback moves to the new device at the next utterance that begins while no speech is playing. If the device in use is removed, speech in progress is discarded, and subsequent utterances are played on the current default device.

### OneCore

The OneCore backend synthesizes speech through the `Windows.Media.SpeechSynthesis` API and plays it through the `Windows.Media.Audio` audio graph API. Both APIs MUST be present for the backend to report `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`; if either is absent, `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

`prism_backend_speak` returns once the text has been queued. Each utterance is synthesized in its entirety on an internal worker thread before playback of it begins. From the return of `prism_backend_speak` until the audio graph has consumed the final audio frame of the speech, `prism_backend_is_speaking` reports `true`, unless speech is paused or stopped in the interim. `prism_backend_stop` silences the utterance being played immediately and discards all queued utterances.

The rate, pitch, and voice in effect when `prism_backend_speak` is called apply to that utterance; subsequent changes affect only utterances queued thereafter. Volume is applied during playback, and a change of volume therefore also affects the utterance being played.

The audio graph renders to the default audio output device. When the default device changes, the audio graph is recreated on the new device at the next utterance that begins while no speech is playing. If the audio graph fails, including when its device is removed, speech in progress is discarded, and the audio graph is recreated at the next utterance.

### GDK Speech

The GDK speech backend synthesizes speech through the speech synthesis API of the Microsoft Game Development Kit (GDK) and plays it through XAudio2. The backend is compiled only when the GDK headers are available at build time. The build locates the headers through the `PRISM_GDK_ROOT` build option if it is set, and otherwise through the environment variables defined by the GDK installer.

#### Windows

On Windows, Prism does not link against the GDK. The backend loads `xgameruntime.thunks.dll` at run time. The library MUST be resolvable through the host process's library search order or be present in the directory containing the Prism library; if it is not, the backend does not report `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`, and `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`. Prism does not distribute the library, the redistribution of which is governed by the GDK license. An application that uses the backend MUST deploy the library itself. A build of Prism that locates the library within the GDK copies it to the build directory beside the Prism library; the library is not installed.

The library depends on the Gaming Runtime, which is provided by Microsoft Gaming Services. Gaming Services MUST be installed. Each backend instance holds a reference to the Gaming Runtime from its creation until it is freed. If the Gaming Runtime cannot be initialized, the backend does not report `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`, and `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

#### Xbox

On Xbox, Prism links against the GDK `xgameruntime` library. When Prism is built as a static library, it shares the title's Gaming Runtime, and the title MUST call `XGameRuntimeInitialize` before creating any backend instance. When Prism is built as a shared library, each backend instance holds its own reference to the Gaming Runtime, as on Windows.

#### All platforms

The runtime-supported probe creates and destroys a speech synthesizer and an XAudio2 engine. `prism_backend_initialize` enumerates the installed voices and returns `PRISM_ERROR_NO_VOICES` if the Gaming Runtime reports none.

`prism_backend_speak` returns once the text has been queued. Each utterance is synthesized in its entirety on an internal worker thread before playback of it begins; the delay before an utterance becomes audible therefore increases with its length. From the return of `prism_backend_speak` until playback of the utterance ends, `prism_backend_is_speaking` reports `true`, unless speech is paused or stopped in the interim. An utterance that cannot be synthesized is discarded, and the failure is reported through the logging facility. Changes of the default audio output device, and removal of the device in use, are handled as described for the SAPI backend.

Until `prism_backend_set_voice` is called, the backend uses the system default voice, and `prism_backend_get_voice` returns `PRISM_ERROR_VOICE_NOT_FOUND`. If the selected voice is no longer installed when `prism_backend_refresh_voices` is called, the backend reverts to the system default voice.

Rate and pitch are applied through SSML prosody markup. Text passed to `prism_backend_speak` is escaped and SHALL NOT be interpreted as SSML. The rate and pitch in effect when `prism_backend_speak` is called apply to that utterance; subsequent changes affect only utterances queued thereafter. Volume is applied during playback, and a change of volume therefore also affects the utterance being played.

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

Note: the native Spiel backend is not currently enabled in the Python wheels because Spiel is not in the distribution package repositories used to build those artifacts at this time. The release packages and the Godot add-on for Linux include the backend, and load `libspiel-1.0.so.1` at run time when it is present. If you wish to use the native backend from Python, you will need to build Prism and Spiel from source by hand or using a tool such as [vcpkg](https://github.com/microsoft/vcpkg). Wine bridge availability is determined separately by whether the bridge supplied with the build contains Spiel and whether a compatible provider is reachable at run time.

### Android Text to Speech

The Prism Android library's application context MUST be initialized in the host process before any operation is invoked. When Prism is consumed as a standard Android library archive (AAR) through Gradle or another mechanism that respects the merged manifest, this initialization is performed automatically by a `ContentProvider` registered in the library's manifest, whose `onCreate` method runs before the application's is executed. Consumers that repackage the library in a way that drops the merged manifest entries MUST initialize the application context manually before using any Android backend.

The calling thread MUST be attached to the Java VM. This is automatic for threads created by the platform runtime. Native threads created outside the runtime MUST attach themselves through `JavaVM::AttachCurrentThread` before invoking Prism. Initialization performs the platform's text-to-speech engine handshake, which is asynchronous, and waits up to 10 seconds for the engine to report readiness. Because an engine that is already running accepts a new client almost immediately, Prism connects to the engine when its application context is initialized and keeps that connection open for the life of the process. An engine that fails to report readiness within the deadline causes `prism_backend_initialize` to return `PRISM_ERROR_BACKEND_NOT_AVAILABLE`.

### Android Screen Reader

The application-context and Java VM attachment requirements of the Android Text to Speech backend also apply to the Android Screen Reader backend. Accessibility services MUST be enabled on the device, and at least one accessibility service capable of spoken feedback MUST be enabled in the user's accessibility settings.

### Web Speech

In browsing contexts that enforce an autoplay policy, the first call to an instance of the web speech backend to produce speech MUST originate from a user-initiated event handler such as a `click` or `keydown` listener. This is a property of the browsing context, not of Prism. The set of available voices is populated asynchronously by the browser. The voice list reported by the backend MAY be briefly empty after initialization on first page load. Applications SHOULD invoke `prism_backend_refresh_voices` if the voice list is unexpectedly empty, or arrange to do so in response to the browser's `voiceschanged` event.

### Clipboard

The Clipboard backend places text on the system clipboard instead of presenting it to the user. It is available on Windows, macOS, iOS, iPadOS, Mac Catalyst, visionOS, Linux, BSD, Android, and the web, and is not built for tvOS, watchOS, or Xbox. As described in the chapter on screen readers and speech engines, it is neither a screen reader nor a speech engine. It reports `PRISM_BACKEND_REQUIRES_EXPLICIT_SELECTION`, and applications MUST therefore request it by identifier or name.

`prism_backend_speak` and `prism_backend_output` replace the contents of the clipboard with the text supplied, and ignore `interrupt`. No other operation is implemented. A call that cannot place the text on the clipboard returns `PRISM_ERROR_SPEAK_FAILURE`. Except on Linux and BSD, the text remains on the clipboard after the backend instance is freed.

#### Windows

The backend places the text on the clipboard as `CF_UNICODETEXT`, from which Windows synthesizes the other text formats. Each call opens the clipboard through a message-only window that it creates and destroys, so the backend places no requirements on the host process's windows or message loop. If another process holds the clipboard open, the call retries 10 times at 10-millisecond intervals before it fails. The backend always reports `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`.

#### Apple platforms

On macOS, the backend writes the text to the general pasteboard as `public.utf8-plain-text`. On iOS, iPadOS, Mac Catalyst, and visionOS, it sets the string of the general pasteboard; these platforms report no failure, and calls always succeed. The pasteboard is accessed on the calling thread. The backend always reports `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`.

On macOS, the host process MUST be running in a user's graphical session; if it is not, calls fail.

#### Linux and BSD

The contents of the clipboard on these systems belong to the client that set them, and are available only while that client remains connected to serve them. Each initialized backend instance therefore opens its own connection to the display server and serves the clipboard from an internal thread. `libwayland-client.so.0` and `libxcb.so.1` are loaded at run time, and are not required for Prism to load. The backend uses the first of the following that can be established at initialization:

1. A Wayland connection, if `WAYLAND_DISPLAY` is set and non-empty and the compositor implements `ext-data-control-v1` or `zwlr-data-control-unstable-v1`. `ext-data-control-v1` is preferred.
2. An X11 connection, if `DISPLAY` is set and non-empty.

Compositors that implement neither data control protocol, such as GNOME Shell, do not allow an application without a focused window to set the clipboard. Under such a compositor, the backend uses Xwayland if `DISPLAY` is set, and the compositor relays the clipboard to its Wayland clients. If neither connection can be established, the backend does not report `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`, and `prism_backend_initialize` returns `PRISM_ERROR_BACKEND_NOT_AVAILABLE`. Text consoles and other sessions without a display server do not satisfy this requirement.

A display server that does not complete the connection within 2 seconds is treated as unavailable. `prism_backend_initialize` and the runtime-supported probe therefore return within approximately 4 seconds even if neither display server responds. The thread that made a timed-out attempt is not joined, and closes its connection if the display server responds later; until then, the backend does not attempt another connection to that kind of display server.

Under Wayland, the text is offered as `text/plain;charset=utf-8`, `text/plain`, `UTF8_STRING`, `TEXT`, and `STRING`. Under X11, the backend owns the `CLIPBOARD` selection and offers `UTF8_STRING`, `TEXT`, `text/plain;charset=utf-8`, and `text/plain`. Text larger than 256 KiB is transferred with the `INCR` protocol. The `PRIMARY` selection is not set.

`prism_backend_speak` returns once the text has been handed to the internal thread. A successful return therefore indicates that the text was submitted, not that the clipboard was changed, and when several texts are submitted before the thread runs, only the last is placed on the clipboard. After the connection to the display server is lost, calls return `PRISM_ERROR_BACKEND_NOT_AVAILABLE`, and applications SHOULD destroy and re-create the backend.

The text is available only while the backend instance serves it, and is lost when the instance is freed or the process exits, unless another program preserves it. Under X11, if a clipboard manager is running when the instance is freed, the backend asks the manager to keep the text through the `SAVE_TARGETS` protocol, and waits up to 1 second for it to do so. Wayland has no equivalent protocol, and whether the text survives depends on the compositor.

Under Weston, and therefore under WSLg, a Wayland client that reads text larger than 256 KiB MAY receive it truncated, or MAY not receive it at all, because Weston does not reliably relay `INCR` transfers from Xwayland. X11 clients receive the complete text.

#### Android

The backend sets the primary clip of the system clipboard to a plain-text clip. It has the application-context and Java VM attachment requirements of the Android Text to Speech backend, and reports `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` when the clipboard service can be obtained. On Android 13 and later, the system displays a confirmation to the user whenever an application places content on the clipboard. A clip larger than the binder transaction limit is rejected, and the call fails.

#### Web

The backend writes the text through `navigator.clipboard.writeText`, which is available only in a secure context and on the browser's main thread. Calls made on other threads are proxied to the main thread and wait for it. The backend reports `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` only when the API is present. A call made while the document does not have focus fails.

The write completes asynchronously, so a successful return indicates only that the write was requested. As with the Web Speech backend, browsers MAY refuse a write that does not originate from a user-initiated event handler. Such a refusal is reported through the logging facility at the warning level, not to the caller.

### No Speech

The No Speech backend produces no output. It is available on every platform, and is intended for testing and for applications that need a backend that presents nothing. Like the Clipboard backend, it is neither a screen reader nor a speech engine, and applications MUST request it by identifier or name. It always reports `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME`, and supports every operation except those for SSML, each of which behaves as described in the chapter on backend functions.

The backend models the duration of each utterance without creating a thread, and `prism_backend_is_speaking`, `prism_backend_pause`, `prism_backend_resume`, and `prism_backend_stop` act on that model. An utterance of `n` Unicode code points lasts `60 × n ÷ s` milliseconds, where `s` is the speed for the rate in effect when the utterance is submitted. The speed rises linearly from 0.5 at a rate of 0.0 to 1 at the default rate of 0.5, and from 1 to 3 between rates of 0.5 and 1.0. Speech submitted without interruption follows the speech already queued, and speech submitted while paused is heard after `prism_backend_resume`.

`prism_backend_speak_to_memory` delivers silence for the same duration, as a single channel at 22050 Hz, in chunks of at most 4096 frames. The callback is not invoked for empty text. `prism_backend_get_bit_depth` reports 16.

The backend has one voice, named `No Speech`, whose language is `und`. Volume, rate, and pitch default to 0.5 and retain the value most recently set. Volume and pitch have no effect. `prism_backend_braille` discards its text.
