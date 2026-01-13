from .lib import ffi, lib
from enum import IntEnum
import sys
from typing import Union

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
    ZDSR = 0xAF63B44C8601A843
    ZOOM_TEXT = 0xAE439D62DC7B1479


class PrismException(Exception):
    """Base class for all Prism-related errors."""

    def __init__(self, code, message=None):
        self.code = code
        self.message = message
        super().__init__(message or f"Prism Error Code: {code}")


class PrismNotInitializedError(PrismException, RuntimeError):
    """PRISM_ERROR_NOT_INITIALIZED"""


class PrismAlreadyInitializedError(PrismException, RuntimeError):
    """PRISM_ERROR_ALREADY_INITIALIZED"""


class PrismInvalidOperationError(PrismException, RuntimeError):
    """PRISM_ERROR_INVALID_OPERATION"""


class PrismInternalError(PrismException, RuntimeError):
    """PRISM_ERROR_INTERNAL"""


class PrismBackendNotAvailableError(PrismException, RuntimeError):
    """PRISM_ERROR_BACKEND_NOT_AVAILABLE"""


class PrismNotImplementedError(PrismException, NotImplementedError):
    """PRISM_ERROR_NOT_IMPLEMENTED"""


class PrismInvalidParamError(PrismException, ValueError):
    """PRISM_ERROR_INVALID_PARAM"""


class PrismRangeError(PrismException, IndexError):
    """PRISM_ERROR_RANGE_OUT_OF_BOUNDS"""


class PrismInvalidUtf8Error(PrismException, UnicodeError):
    """PRISM_ERROR_INVALID_UTF8"""


class PrismNotSpeakingError(PrismException):
    """PRISM_ERROR_NOT_SPEAKING"""


class PrismNotPausedError(PrismException):
    """PRISM_ERROR_NOT_PAUSED"""


class PrismAlreadyPausedError(PrismException):
    """PRISM_ERROR_ALREADY_PAUSED"""


class PrismSpeakError(PrismException, IOError):
    """PRISM_ERROR_SPEAK_FAILURE"""


class PrismNoVoicesError(PrismException):
    """PRISM_ERROR_NO_VOICES"""


class PrismVoiceNotFoundError(PrismException, LookupError):
    """PRISM_ERROR_VOICE_NOT_FOUND"""


class PrismMemoryError(PrismException, MemoryError):
    """PRISM_ERROR_MEMORY_FAILURE"""


class PrismUnknownError(PrismException):
    """PRISM_ERROR_UNKNOWN"""


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
}


def _check_error(error_code: int):
    """
    Checks the error code. If it is OK, returns None.
    Otherwise, raises the appropriate PrismException.
    """
    if error_code == 0:
        return
    exc_class = _ERROR_MAP.get(error_code, PrismUnknownError)
    msg_ptr = lib.prism_error_string(error_code)
    msg = ffi.string(msg_ptr).decode("utf-8")
    raise exc_class(error_code, msg)


class Backend:
    _raw = None

    def __init__(self, raw_ptr) -> None:
        if raw_ptr == ffi.NULL:
            raise RuntimeError("Backend raw pointer MUST NOT be NULL!")
        self._raw = raw_ptr
        res = lib.prism_backend_initialize(self._raw)
        if res != lib.PRISM_OK and res != lib.PRISM_ERROR_ALREADY_INITIALIZED:
            return _check_error(res)

    def __del__(self):
        lib.prism_backend_free(self._raw)
        self._raw = None
        return None

    @property
    def name(self) -> str:
        return ffi.string(lib.prism_backend_name(self._raw)).decode("utf-8")

    def speak(self, text: str, interrupt: bool = False):
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Text MUST NOT be empty",
            )
        return _check_error(
            lib.prism_backend_speak(self._raw, text.encode("utf-8"), interrupt),
        )

    def speak_to_memory(self, text: str, on_audio_data):
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Text MUST NOT be empty",
            )

        @ffi.callback("void(void *, const float *, size_t, size_t, size_t)")
        def audio_callback_shim(_userdata, samples_ptr, count, channels, rate):
            pcm_data = ffi.unpack(samples_ptr, count * channels)
            on_audio_data(pcm_data, channels, rate)

        self._active_callback = audio_callback_shim
        return self._check_error(
            lib.prism_backend_speak_to_memory(
                self._raw,
                text.encode("utf-8"),
                audio_callback_shim,
                ffi.NULL,
            ),
        )

    def braille(self, text: str):
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Text MUST NOT be empty",
            )
        return _check_error(lib.prism_backend_braille(self._raw, text.encode("utf-8")))

    def output(self, text: str, interrupt: bool = False):
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Text MUST NOT be empty",
            )
        return _check_error(
            lib.prism_backend_output(self._raw, text.encode("utf-8"), interrupt),
        )

    def stop(self):
        return _check_error(lib.prism_backend_stop(self._raw))

    def pause(self):
        return _check_error(lib.prism_backend_pause(self._raw))

    def resume(self):
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
    def volume(self, volume: float):
        return _check_error(lib.prism_backend_set_volume(self._raw, volume))

    @property
    def rate(self) -> float:
        p_rate = ffi.new("float*")
        _check_error(lib.prism_backend_get_rate(self._raw, p_rate))
        return p_rate[0]

    @rate.setter
    def rate(self, rate: float):
        return _check_error(lib.prism_backend_set_rate(self._raw, rate))

    @property
    def pitch(self) -> float:
        p_pitch = ffi.new("float*")
        _check_error(lib.prism_backend_get_pitch(self._raw, p_pitch))
        return p_pitch[0]

    @pitch.setter
    def pitch(self, pitch: float):
        return _check_error(lib.prism_backend_set_pitch(self._raw, pitch))

    def refresh_voices(self):
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
    def voice(self, idx: int):
        return _check_error(lib.prism_backend_set_voice(self._raw, idx))

    @property
    def channels(self):
        out_channels = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_channels(self._raw, out_channels))
        return out_channels[0]

    @property
    def sample_rate(self):
        out_sample_rate = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_sample_rate(self._raw, out_sample_rate))
        return out_sample_rate[0]

    @property
    def bit_depth(self):
        out_bit_depth = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_bit_depth(self._raw, out_bit_depth))
        return out_bit_depth[0]


class Context:
    _ctx = None

    def __init__(self, hwnd: Union[HWND, int, None] = None):
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

    def id_of(self, index_or_name: Union[int, str]) -> BackendId:
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
