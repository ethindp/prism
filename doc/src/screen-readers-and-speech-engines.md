# Screen Readers and Speech Engines

Prism's backends fall into two categories, which behave differently and which serve different users. This chapter describes the distinction and the consequences it has for an application. The requirements of each individual backend are given in the chapter on backend-specific notes.

## Definitions

A screen reader is an assistive technology which presents the contents of the screen to a user who cannot see it, generally through synthesized speech, braille, or both. A user of a screen reader has chosen the voice, speaking rate, verbosity, and braille configuration the screen reader employs, and relies on those choices for all of the software on the system. Examples include NVDA, JAWS, VoiceOver, Orca, and the Android screen readers.

A speech engine synthesizes audio from text on behalf of whichever program requests it. It has no knowledge of what is on the screen and does not make a system accessible on its own. Examples include SAPI, OneCore, AVSpeech, Speech Dispatcher, and the Web Speech API.

A backend reports which of the two it is through the `PRISM_BACKEND_IS_SCREEN_READER` and `PRISM_BACKEND_IS_SPEECH_ENGINE` bits of `prism_backend_get_features`. Every built-in backend sets exactly one of them. A custom backend MAY set neither, and MUST NOT set both.

An application that communicates with a screen reader asks that screen reader to present a message. The message is then subject to the screen reader's own queue and configuration. An application that communicates with a speech engine instead requests that specific audio be produced, independently of any screen reader that may be running. The two can therefore speak over one another if an application uses a speech engine while the user's screen reader is active.

Applications that present their interface through a platform's accessibility API, such as UI Automation on Windows, NSAccessibility on Apple platforms, or AT-SPI on Linux, are read by the screen reader without using Prism at all. Prism is intended for those circumstances in which that is insufficient or impossible, such as announcements of events which do not coincide with a change of focus, and applications such as games that draw their own interface and expose nothing to the accessibility API.

## Backend selection

Each backend in a registry has a priority, which can be retrieved through `prism_registry_priority`. Screen reader backends generally have a higher priority than speech engine backends. `prism_registry_create_best` and `prism_registry_acquire_best` attempt the backends in descending priority order and return the first which initializes successfully. A screen reader backend is unable to initialize when its screen reader is not running. Consequently, a program which uses these functions communicates with the user's screen reader if there is one, and otherwise falls back on a speech engine. This is the appropriate behavior for most applications, and applications SHOULD NOT replace it without a specific reason.

An application MAY present a choice of backend to its users. The application SHOULD enumerate the registry for this purpose rather than assuming that a particular backend is present, since the set of backends varies according to the platform and the options with which Prism was built. Membership in a registry does not imply that a backend can be used; applications SHOULD inspect `PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME` before offering a backend, and MUST be prepared for its initialization to fail regardless.

The selected backend is not updated if the circumstances change. An application which obtained a speech engine backend continues to use it if a screen reader is later started, and a backend whose screen reader has exited begins to return errors. A long-running application that wishes to respond to such changes SHOULD use the facilities described in the chapter on background availability enumeration.

## Speech, braille, and combined output

`prism_backend_speak` produces speech, and `prism_backend_braille` presents text on a braille display. `prism_backend_output` uses every modality that the backend supports. Since a user of a screen reader may be relying on braille alone, applications SHOULD use `prism_backend_output` for messages meant for the user unless they have a specific reason to confine a message to one modality. On a built-in backend without braille support, `prism_backend_output` is equivalent to `prism_backend_speak`. A custom backend implements `output` itself, and need not implement it at all.

## Respecting the user's configuration

Applications SHOULD NOT alter the rate, pitch, volume, or voice of a screen reader backend on their own initiative, even where the backend permits it. Experienced users commonly listen at rates which seem unintelligibly fast to others, and a value which an application considers comfortable may be unusable to them. Such changes SHOULD be made only in response to an explicit request by the user. The considerations are different for a speech engine backend, for which the application has itself assumed responsibility for these settings and SHOULD offer a means of adjusting them.

The `interrupt` parameter of `prism_backend_speak` and `prism_backend_output` likewise affects the user directly. Interrupting discards whatever the user was listening to, which may be something other than a message from the application. Applications SHOULD reserve it for messages which supersede the preceding speech, such as a change of selection in a menu, and SHOULD NOT interrupt in order to deliver messages which the user would otherwise hear in sequence.

## Differences in capability

Backends differ considerably in the functions they implement. Most screen readers offer no means of reporting when an announcement has finished, for example, and the standard announcement facility on macOS cannot be cancelled. An application MUST NOT assume that an operation is implemented because it is implemented on another backend. It SHOULD inspect the bits returned by `prism_backend_get_features` where its behavior depends on a capability, and MUST handle `PRISM_ERROR_NOT_IMPLEMENTED`.

Applications whose logic depends on the end of an utterance, such as those which advance a sequence once a line of dialogue has been spoken, therefore cannot rely upon every backend to provide this information, and SHOULD provide an alternative means of proceeding, such as a key press.
