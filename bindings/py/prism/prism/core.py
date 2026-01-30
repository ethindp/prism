import sys
from dataclasses import dataclass, field, fields
from enum import IntEnum

from .lib import ffi, lib

if sys.platform == "win32":
    try:
        from win32more.Windows.Win32.Foundation import HWND
    except ImportError:
        HWND = int
else:
    HWND = int


class BackendId(IntEnum):
    INVALID = 0
    SAPI = 0x1D6DF72422CEEE66
    AV_SPEECH = 0x28E3429577805C24
    VOICE_OVER = 0xCB4897961A754BCB
    SPEECH_DISPATCHER = 0xE3D6F895D949EBFE
    NVDA = 0x89CC19C5C4AC1A56
    JAWS = 0xAC3D60E9BD84B53E
    ONE_CORE = 0x6797D32F0D994CB4
    ORCA = 0x10AA1FC05A17F96C
    ANDROID_SCREEN_READER = 0xD199C175AEEC494B
    ANDROID_TTS = 0xBC175831BFE4E5CC
    WEB_SPEECH = 0x3572538D44D44A8F
    UIA = 0x6238F019DB678F8E
    ZDSR = 0x3D93C56C9E7F2A2E
    ZOOM_TEXT = 0xAE439D62DC7B1479


class PrismError(Exception):
    """Base class for all Prism-related errors."""

    def __init__(self, code: lib.PrismError, message: str | None = None) -> None:
        self.code = code
        self.message = message
        super().__init__(message or f"Prism Error Code: {code}")


class PrismNotInitializedError(PrismError, RuntimeError):
    """PRISM_ERROR_NOT_INITIALIZED"""


class PrismAlreadyInitializedError(PrismError, RuntimeError):
    """PRISM_ERROR_ALREADY_INITIALIZED"""


class PrismInvalidOperationError(PrismError, RuntimeError):
    """PRISM_ERROR_INVALID_OPERATION"""


class PrismInternalError(PrismError, RuntimeError):
    """PRISM_ERROR_INTERNAL"""


class PrismBackendNotAvailableError(PrismError, RuntimeError):
    """PRISM_ERROR_BACKEND_NOT_AVAILABLE"""


class PrismNotImplementedError(PrismError, NotImplementedError):
    """PRISM_ERROR_NOT_IMPLEMENTED"""


class PrismInvalidParamError(PrismError, ValueError):
    """PRISM_ERROR_INVALID_PARAM"""


class PrismRangeError(PrismError, IndexError):
    """PRISM_ERROR_RANGE_OUT_OF_BOUNDS"""


class PrismInvalidUtf8Error(PrismError, UnicodeError):
    """PRISM_ERROR_INVALID_UTF8"""


class PrismNotSpeakingError(PrismError):
    """PRISM_ERROR_NOT_SPEAKING"""


class PrismNotPausedError(PrismError):
    """PRISM_ERROR_NOT_PAUSED"""


class PrismAlreadyPausedError(PrismError):
    """PRISM_ERROR_ALREADY_PAUSED"""


class PrismSpeakError(PrismError, IOError):
    """PRISM_ERROR_SPEAK_FAILURE"""


class PrismNoVoicesError(PrismError):
    """PRISM_ERROR_NO_VOICES"""


class PrismVoiceNotFoundError(PrismError, LookupError):
    """PRISM_ERROR_VOICE_NOT_FOUND"""


class PrismMemoryError(PrismError, MemoryError):
    """PRISM_ERROR_MEMORY_FAILURE"""


class PrismUnknownError(PrismError):
    """PRISM_ERROR_UNKNOWN"""


class PrismInvalidAudioFormatError(PrismError, RuntimeError):
    """PRISM_ERROR_INVALID_AUDIO_FORMAT"""


_ERROR_MAP = {
    lib.PRISM_ERROR_NOT_INITIALIZED: PrismNotInitializedError,
    lib.PRISM_ERROR_INVALID_PARAM: PrismInvalidParamError,
    lib.PRISM_ERROR_NOT_IMPLEMENTED: PrismNotImplementedError,
    lib.PRISM_ERROR_NO_VOICES: PrismNoVoicesError,
    lib.PRISM_ERROR_VOICE_NOT_FOUND: PrismVoiceNotFoundError,
    lib.PRISM_ERROR_SPEAK_FAILURE: PrismSpeakError,
    lib.PRISM_ERROR_MEMORY_FAILURE: PrismMemoryError,
    lib.PRISM_ERROR_RANGE_OUT_OF_BOUNDS: PrismRangeError,
    lib.PRISM_ERROR_INTERNAL: PrismInternalError,
    lib.PRISM_ERROR_NOT_SPEAKING: PrismNotSpeakingError,
    lib.PRISM_ERROR_NOT_PAUSED: PrismNotPausedError,
    lib.PRISM_ERROR_ALREADY_PAUSED: PrismAlreadyPausedError,
    lib.PRISM_ERROR_INVALID_UTF8: PrismInvalidUtf8Error,
    lib.PRISM_ERROR_INVALID_OPERATION: PrismInvalidOperationError,
    lib.PRISM_ERROR_ALREADY_INITIALIZED: PrismAlreadyInitializedError,
    lib.PRISM_ERROR_BACKEND_NOT_AVAILABLE: PrismBackendNotAvailableError,
    lib.PRISM_ERROR_UNKNOWN: PrismUnknownError,
    lib.PRISM_ERROR_INVALID_AUDIO_FORMAT: PrismInvalidAudioFormatError,
}


def _check_error(error_code: int) -> None:
    """
    Checks the error code. If it is OK, returns None.
    Otherwise, raises the appropriate PrismError.
    """
    if error_code == 0:
        return
    exc_class = _ERROR_MAP.get(error_code, PrismUnknownError)
    msg_ptr = lib.prism_error_string(error_code)
    msg = ffi.string(msg_ptr).decode("utf-8")
    raise exc_class(error_code, msg)


def _bit(position: int) -> field:
    return field(default=False, metadata={"bit": position})


@dataclass(frozen=True, slots=True)
class BackendFeatures:
    is_supported_at_runtime: bool = _bit(0)
    supports_speak: bool = _bit(2)
    supports_speak_to_memory: bool = _bit(3)
    supports_braille: bool = _bit(4)
    supports_output: bool = _bit(5)
    supports_is_speaking: bool = _bit(6)
    supports_stop: bool = _bit(7)
    supports_pause: bool = _bit(8)
    supports_resume: bool = _bit(9)
    supports_set_volume: bool = _bit(10)
    supports_get_volume: bool = _bit(11)
    supports_set_rate: bool = _bit(12)
    supports_get_rate: bool = _bit(13)
    supports_set_pitch: bool = _bit(14)
    supports_get_pitch: bool = _bit(15)
    supports_refresh_voices: bool = _bit(16)
    supports_count_voices: bool = _bit(17)
    supports_get_voice_name: bool = _bit(18)
    supports_get_voice_language: bool = _bit(19)
    supports_get_voice: bool = _bit(20)
    supports_set_voice: bool = _bit(21)
    supports_get_channels: bool = _bit(22)
    supports_get_sample_rate: bool = _bit(23)
    supports_get_bit_depth: bool = _bit(24)
    performs_silence_trimming_on_speak: bool = _bit(25)
    performs_silence_trimming_on_speak_to_memory: bool = _bit(26)
    supports_speak_ssml: bool = _bit(27)
    supports_speak_to_memory_ssml: bool = _bit(28)

    @classmethod
    def from_bits(cls, bits: int) -> "BackendFeatures":
        return cls(
            **{f.name: bool(bits & (1 << f.metadata["bit"])) for f in fields(cls)},
        )


class Backend:
    _raw = None

    def __init__(self, raw_ptr: lib.PrismBackend) -> None:
        if raw_ptr == ffi.NULL:
            raise RuntimeError("Backend raw pointer MUST NOT be NULL!")
        self._raw = raw_ptr
        res = lib.prism_backend_initialize(self._raw)
        if res not in {lib.PRISM_OK, lib.PRISM_ERROR_ALREADY_INITIALIZED}:
            _check_error(res)

    def __del__(self) -> None:
        lib.prism_backend_free(self._raw)
        self._raw = None

    @property
    def name(self) -> str:
        return ffi.string(lib.prism_backend_name(self._raw)).decode("utf-8")

    def speak(self, text: str, interrupt: bool = False) -> None:
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Text MUST NOT be empty",
            )
        return _check_error(
            lib.prism_backend_speak(self._raw, text.encode("utf-8"), interrupt),
        )

    def speak_to_memory(self, text: str, on_audio_data) -> None:
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Text MUST NOT be empty",
            )

        @ffi.callback("void(void *, const float *, size_t, size_t, size_t)")
        def audio_callback_shim(
            _userdata,
            samples_ptr: int,
            count,
            channels,
            rate,
        ) -> None:
            pcm_data = ffi.unpack(samples_ptr, count * channels)
            on_audio_data(pcm_data, channels, rate)

        self._active_callback = audio_callback_shim
        return _check_error(
            lib.prism_backend_speak_to_memory(
                self._raw,
                text.encode("utf-8"),
                audio_callback_shim,
                ffi.NULL,
            ),
        )

    def braille(self, text: str) -> None:
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Text MUST NOT be empty",
            )
        return _check_error(lib.prism_backend_braille(self._raw, text.encode("utf-8")))

    def output(self, text: str, interrupt: bool = False) -> None:
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Text MUST NOT be empty",
            )
        return _check_error(
            lib.prism_backend_output(self._raw, text.encode("utf-8"), interrupt),
        )

    def stop(self) -> None:
        return _check_error(lib.prism_backend_stop(self._raw))

    def pause(self) -> None:
        return _check_error(lib.prism_backend_pause(self._raw))

    def resume(self) -> None:
        return _check_error(lib.prism_backend_resume(self._raw))

    @property
    def speaking(self) -> bool:
        p_speaking = ffi.new("bool*")
        _check_error(lib.prism_backend_is_speaking(self._raw, p_speaking))
        return p_speaking[0]

    @property
    def volume(self) -> float:
        p_volume = ffi.new("float*")
        _check_error(lib.prism_backend_get_volume(self._raw, p_volume))
        return p_volume[0]

    @volume.setter
    def volume(self, volume: float) -> None:
        return _check_error(lib.prism_backend_set_volume(self._raw, volume))

    @property
    def rate(self) -> float:
        p_rate = ffi.new("float*")
        _check_error(lib.prism_backend_get_rate(self._raw, p_rate))
        return p_rate[0]

    @rate.setter
    def rate(self, rate: float) -> None:
        return _check_error(lib.prism_backend_set_rate(self._raw, rate))

    @property
    def pitch(self) -> float:
        p_pitch = ffi.new("float*")
        _check_error(lib.prism_backend_get_pitch(self._raw, p_pitch))
        return p_pitch[0]

    @pitch.setter
    def pitch(self, pitch: float) -> None:
        return _check_error(lib.prism_backend_set_pitch(self._raw, pitch))

    def refresh_voices(self) -> None:
        return _check_error(lib.prism_backend_refresh_voices(self._raw))

    @property
    def voices_count(self) -> int:
        out_count = ffi.new("size_t*")
        _check_error(lib.prism_backend_count_voices(self._raw, out_count))
        return out_count[0]

    def get_voice_name(self, idx: int) -> str:
        pp_name = ffi.new("char **")
        _check_error(lib.prism_backend_get_voice_name(self._raw, idx, pp_name))
        return ffi.string(pp_name[0]).decode("utf-8")

    def get_voice_language(self, idx: int) -> str:
        pp_lang = ffi.new("char **")
        _check_error(lib.prism_backend_get_voice_language(self._raw, idx, pp_lang))
        return ffi.string(pp_lang[0]).decode("utf-8")

    @property
    def voice(self) -> int:
        out_voice_id = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_voice(self._raw, out_voice_id))
        return out_voice_id[0]

    @voice.setter
    def voice(self, idx: int) -> None:
        return _check_error(lib.prism_backend_set_voice(self._raw, idx))

    @property
    def channels(self) -> int:
        out_channels = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_channels(self._raw, out_channels))
        return out_channels[0]

    @property
    def sample_rate(self) -> int:
        out_sample_rate = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_sample_rate(self._raw, out_sample_rate))
        return out_sample_rate[0]

    @property
    def bit_depth(self) -> int:
        out_bit_depth = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_bit_depth(self._raw, out_bit_depth))
        return out_bit_depth[0]

    @property
    def features(self) -> BackendFeatures:
        return BackendFeatures.from_bits(lib.prism_backend_get_features(self._raw))


class Context:
    _ctx: lib.PrismContext = None

    def __init__(self, hwnd: HWND | int | None = None) -> None:
        config = lib.prism_config_init()
        if hwnd is not None:
            config.platform_data = ffi.cast("void*", int(hwnd))
        self._ctx = lib.prism_init(ffi.new("PrismConfig *", config))
        if self._ctx == ffi.NULL:
            raise RuntimeError("Prism could not be initialized")

    def __del__(self):
        if hasattr(self, "_ctx") and self._ctx:
            lib.prism_shutdown(self._ctx)
            self._ctx = None

    @property
    def backends_count(self) -> int:
        return lib.prism_registry_count(self._ctx)

    def id_of(self, index_or_name: int | str) -> BackendId:
        if isinstance(index_or_name, int):
            res = lib.prism_registry_id_at(self._ctx, index_or_name)
        elif isinstance(index_or_name, str):
            res = lib.prism_registry_id(self._ctx, index_or_name.encode("utf-8"))
        else:
            raise TypeError("Expected int or string")
        try:
            return BackendId(res)
        except ValueError as e:
            raise ValueError(f"Prism returned unknown backend ID: {res:#x}") from e

    def name_of(self, backend_id: BackendId) -> str:
        c_ptr = lib.prism_registry_name(self._ctx, backend_id)
        if c_ptr == ffi.NULL:
            raise ValueError("Backend ID not found")
        return ffi.string(c_ptr).decode("utf-8")

    def priority_of(self, backend_id: BackendId) -> int:
        return lib.prism_registry_priority(self._ctx, backend_id)

    def exists(self, backend_id: BackendId) -> bool:
        return bool(lib.prism_registry_exists(self._ctx, backend_id))

    def create(self, backend_id: BackendId) -> Backend:
        res = lib.prism_registry_create(self._ctx, int(backend_id))
        if res == ffi.NULL:
            raise ValueError("Invalid or unsupported backend")
        return Backend(res)

    def create_best(self) -> Backend:
        res = lib.prism_registry_create_best(self._ctx)
        if res == ffi.NULL:
            raise ValueError("Invalid or unsupported backend")
        return Backend(res)

    def acquire(self, backend_id: BackendId) -> Backend:
        res = lib.prism_registry_acquire(self._ctx, int(backend_id))
        if res == ffi.NULL:
            raise ValueError("Invalid or unsupported backend")
        return Backend(res)

    def acquire_best(self) -> Backend:
        res = lib.prism_registry_acquire_best(self._ctx)
        if res == ffi.NULL:
            raise ValueError("Invalid or unsupported backend")
        return Backend(res)
