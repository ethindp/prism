## Predefined Backend Identifiers

Prism defines constants for all known backend identifiers. These constants are 64-bit unsigned integers computed at compile time using a deterministic hash function. Applications MAY use these constants to request specific backends by ID rather than by name, which avoids the overhead of a string lookup.

| Constant | Backend |
| --- | --- |
| `PRISM_BACKEND_INVALID` | Invalid/sentinel value (always 0) |
| `PRISM_BACKEND_SAPI` | Microsoft SAPI (Windows) |
| `PRISM_BACKEND_AV_SPEECH` | AVSpeechSynthesizer (macOS/iOS) |
| `PRISM_BACKEND_VOICE_OVER` | VoiceOver screen reader (macOS/iOS) |
| `PRISM_BACKEND_SPEECH_DISPATCHER` | Speech Dispatcher (Linux/BSD) |
| `PRISM_BACKEND_NVDA` | NVDA screen reader (Windows) |
| `PRISM_BACKEND_JAWS` | JAWS screen reader (Windows) |
| `PRISM_BACKEND_ONE_CORE` | Windows OneCore speech API (Windows 10+) |
| `PRISM_BACKEND_ORCA` | Orca screen reader (Linux/BSD) |
| `PRISM_BACKEND_ANDROID_TTS` | Android TTS engine (Android) |
| `PRISM_BACKEND_ANDROID_SCREEN_READER` | Android screen readers (Android) |
| `PRISM_BACKEND_WEB_SPEECH` | Web SpeechSynthesis API (web) |
| `PRISM_BACKEND_UIA` | UIAutomation backend (Windows only) |
| `PRISM_BACKEND_ZDSR` | Zhengdu Screen Reader (Windows) |
| `PRISM_BACKEND_ZOOM_TEXT` | ZoomText (Windows) |

The availability of any given backend depends on the platform Prism is running on and how Prism was compiled. The presence of a constant does not guarantee that the corresponding backend is available at runtime. Use `prism_registry_exists` to check availability.
