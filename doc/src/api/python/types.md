## Types

### `BackendId`

An enumeration of backend identifiers, corresponding to the constants described in the C API's chapter on [backend identifiers](../c/backend-identifiers.md).

#### Syntax

```python
class BackendId(IntEnum):
    INVALID = 0
    SAPI = ...
    AV_SPEECH = ...
    VOICE_OVER = ...
    SPEECH_DISPATCHER = ...
    NVDA = ...
    JAWS = ...
    ONE_CORE = ...
    ORCA = ...
    ANDROID_SCREEN_READER = ...
    ANDROID_TTS = ...
    WEB_SPEECH = ...
    UIA = ...
    ZDSR = ...
    ZOOM_TEXT = ...
    BOY_PC_READER = ...
    PC_TALKER = ...
    SENSE_READER = ...
    SYSTEM_ACCESS = ...
    WINDOW_EYES = ...
    SPIEL = ...
    GDK_SPEECH = ...
```

#### Remarks

The enumeration accepts any positive integer, so identifiers of custom and plugin backends can be represented. A value without a member of its own acquires a name of the form `CUSTOM_0x...`.

### `BackendFeatures`

An immutable representation of a backend's features, corresponding to the bits described under [`prism_backend_get_features`](../c/backend-functions.md#prism_backend_get_features).

#### Syntax

```python
@dataclass(frozen=True, slots=True)
class BackendFeatures:
    is_supported_at_runtime: bool = False
    supports_speak: bool = False
    supports_speak_to_memory: bool = False
    supports_braille: bool = False
    supports_output: bool = False
    supports_is_speaking: bool = False
    supports_stop: bool = False
    supports_pause: bool = False
    supports_resume: bool = False
    supports_set_volume: bool = False
    supports_get_volume: bool = False
    supports_set_rate: bool = False
    supports_get_rate: bool = False
    supports_set_pitch: bool = False
    supports_get_pitch: bool = False
    supports_refresh_voices: bool = False
    supports_count_voices: bool = False
    supports_get_voice_name: bool = False
    supports_get_voice_language: bool = False
    supports_get_voice: bool = False
    supports_set_voice: bool = False
    supports_get_channels: bool = False
    supports_get_sample_rate: bool = False
    supports_get_bit_depth: bool = False
    performs_silence_trimming_on_speak: bool = False
    performs_silence_trimming_on_speak_to_memory: bool = False
    supports_speak_ssml: bool = False
    supports_speak_to_memory_ssml: bool = False

    @classmethod
    def from_bits(cls, bits: int) -> BackendFeatures: ...
    def to_bits(self) -> int: ...
```

#### Members

Each field corresponds to the bit named `PRISM_BACKEND_` followed by the field's name in upper case.

`from_bits`

Returns an instance representing the given bits. Bits without a corresponding field are ignored.

`to_bits`

Returns the integer representation of the instance.

### `AudioCallback`

The type of a function which receives synthesized audio, corresponding to [`PrismAudioCallback`](../c/audio-callback.md).

#### Syntax

```python
AudioCallback = Callable[[list[float], int, int], None]
```

#### Parameters

The function receives the samples, the number of channels, and the sample rate, in that order. The samples have been copied, so the function MAY retain them. The rules concerning the thread on which the function is invoked, and the functions it MAY call, are those given in the C API's chapter on [thread safety](../c/thread-safety.md).

### `AvailabilityCallback` and `BaselineCallback`

The types of the functions passed to [`Context`](./context.md#context). They are defined in `prism._dispatch`.

#### Syntax

```python
AvailabilityCallback = Callable[[BackendId, str, bool], Awaitable[None] | None]
BaselineCallback = Callable[[], Awaitable[None] | None]
```
