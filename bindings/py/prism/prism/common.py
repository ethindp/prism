from __future__ import annotations

from dataclasses import dataclass, field, fields
from enum import IntEnum
from typing import Self

from .lib import ffi, lib


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
    BOY_PC_READER = 0x285ABA1C16F3300F
    PC_TALKER = 0x344B951962E3B835
    SENSE_READER = 0xED4760890B55C2F2
    SYSTEM_ACCESS = 0x8380F2A37B2C3EB6
    WINDOW_EYES = 0x9120D89908785C13
    SPIEL = 0x478B44F14AD3D89C

    @classmethod
    def _missing_(cls, value: object) -> Self | None:
        if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
            return None
        member = int.__new__(cls, value)
        member._name_ = f"CUSTOM_{value:#018x}"
        member._value_ = value
        return cls._value2member_map_.setdefault(value, member)


class PrismError(Exception):
    """Base class for all Prism-related errors."""

    def __init__(self, code: int, message: str | None = None) -> None:
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


class PrismInternalBackendLimitExceededError(PrismError, RuntimeError):
    """PRISM_ERROR_INTERNAL_BACKEND_LIMIT_EXCEEDED"""


class PrismBackendEnteredUndefinedStateError(PrismError, RuntimeError):
    """PRISM_ERROR_BACKEND_ENTERED_UNDEFINED_STATE"""


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
    lib.PRISM_ERROR_INTERNAL_BACKEND_LIMIT_EXCEEDED: PrismInternalBackendLimitExceededError,
    lib.PRISM_ERROR_BACKEND_ENTERED_UNDEFINED_STATE: PrismBackendEnteredUndefinedStateError,
}


def _check_error(error_code: int) -> None:
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
    def from_bits(cls, bits: int) -> BackendFeatures:
        return cls(
            **{f.name: bool(bits & (1 << f.metadata["bit"])) for f in fields(cls)},
        )

    def to_bits(self) -> int:
        bits: int = 0
        for f in fields(self):
            if getattr(self, f.name):
                bits |= 1 << f.metadata["bit"]
        return bits
