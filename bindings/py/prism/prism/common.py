"""Backend identifiers, feature information, exceptions, and version queries.

The names intended for general use are re-exported by the ``prism`` package.
The subclasses of :class:`PrismError` are available only from this module.
"""

from __future__ import annotations

from dataclasses import dataclass, field, fields
from enum import IntEnum
from typing import TYPE_CHECKING

from ._prism_cffi import ffi, lib

if TYPE_CHECKING:
    from typing import Self


class BackendId(IntEnum):
    """A backend identifier.

    The members correspond to the ``PRISM_BACKEND_*`` constants of the C API.
    Each member is named after its constant without the ``PRISM_BACKEND_``
    prefix and has the constant's value: ``BackendId.SPEECH_DISPATCHER`` is
    ``PRISM_BACKEND_SPEECH_DISPATCHER``. ``BackendId.INVALID`` is ``0``, which
    never identifies a backend.

    The identifier of a backend is a hash of its name. It does not change
    between releases or platforms, and MAY be stored. Membership of a backend in
    a particular registry is reported by :meth:`prism.Context.exists`.

    Custom backends and backends loaded from shared libraries have identifiers
    that are not members. ``BackendId(value)`` accepts any positive integer and,
    for such a value, returns a pseudo-member named ``CUSTOM_`` followed by the
    value in hexadecimal; the same value always returns the same pseudo-member.
    A negative integer, a ``bool``, or a non-integer raises ``ValueError``.
    """

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
    GDK_SPEECH = 0xD39D872349416B11
    CLIPBOARD = 0x7056908DE8F9F991
    NO_SPEECH = 0xC2C0EAB0BF97A5F0

    @classmethod
    def _missing_(cls, value: object) -> Self | None:
        if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
            return None
        member = int.__new__(cls, value)
        member._name_ = f"CUSTOM_{value:#018x}"
        member._value_ = value
        return cls._value2member_map_.setdefault(value, member)


class PrismError(Exception):
    """Base class of the exceptions that report Prism errors.

    Each ``PrismError`` value of the C API other than ``PRISM_OK`` has a
    subclass named after it; ``PRISM_ERROR_NOT_IMPLEMENTED`` is reported as
    :class:`PrismNotImplementedError`. Values unknown to this version of the
    binding are reported as :class:`PrismUnknownError`. Subclasses also derive
    from the built-in exception closest in meaning, where there is one, so that
    :class:`PrismNotImplementedError` is a ``NotImplementedError`` and
    :class:`PrismInvalidParamError` is a ``ValueError``.

    Attributes:
        code: The numeric ``PrismError`` value.
        message: The description of the error, or ``None``. It is the result of
            ``prism_error_string`` for errors reported by Prism, and a
            description of the binding's own for errors the binding detects.
    """

    def __init__(self, code: int, message: str | None = None) -> None:
        """Creates an exception for a ``PrismError`` value.

        A custom backend MAY raise these exceptions to report a specific error; see
        :class:`prism.CustomBackend`.

        Args:
            code: The numeric ``PrismError`` value.
            message: The description of the error. If ``None``, the string form of
                the exception gives the numeric value.
        """
        self.code = code
        self.message = message
        super().__init__(message or f"Prism Error Code: {code}")


class PrismNotInitializedError(PrismError, RuntimeError):
    """The backend is not initialized.

    Reports ``PRISM_ERROR_NOT_INITIALIZED``. Backends are initialized when they are obtained, so this error is not ordinarily seen.
    """


class PrismAlreadyInitializedError(PrismError, RuntimeError):
    """The backend is already initialized.

    Reports ``PRISM_ERROR_ALREADY_INITIALIZED``. The binding treats this error as success when it initializes a backend, so it is not ordinarily seen.
    """


class PrismInvalidOperationError(PrismError, RuntimeError):
    """The operation is invalid in the current state.

    Reports ``PRISM_ERROR_INVALID_OPERATION``. Raised, among other cases, when a registry builder is given a backend whose name or identifier it already contains.
    """


class PrismInternalError(PrismError, RuntimeError):
    """An internal error occurred.

    Reports ``PRISM_ERROR_INTERNAL``. Also reported for a custom backend method that raises an exception with no more specific error.
    """


class PrismBackendNotAvailableError(PrismError, RuntimeError):
    """The backend is not available on this system.

    Reports ``PRISM_ERROR_BACKEND_NOT_AVAILABLE``. A component the backend requires is missing or not running. The backend object remains valid but cannot produce output.
    """


class PrismNotImplementedError(PrismError, NotImplementedError):
    """The backend does not support the operation.

    Reports ``PRISM_ERROR_NOT_IMPLEMENTED``. Support for each operation is reported by :attr:`prism.Backend.features`.
    """


class PrismInvalidParamError(PrismError, ValueError):
    """An argument is invalid.

    Reports ``PRISM_ERROR_INVALID_PARAM``. Also raised by the binding, without calling Prism, for text that is empty or contains a NUL character.
    """


class PrismRangeError(PrismError, IndexError):
    """An argument is outside its valid range.

    Reports ``PRISM_ERROR_RANGE_OUT_OF_BOUNDS``. Raised for a voice parameter outside [0.0, 1.0] and for an out-of-range voice index.
    """


class PrismInvalidUtf8Error(PrismError, UnicodeError):
    """A string is not valid UTF-8.

    Reports ``PRISM_ERROR_INVALID_UTF8``. Text passed through the binding is encoded from ``str``, so this error arises only from text produced elsewhere, such as backend names in a shared library.
    """


class PrismNotSpeakingError(PrismError):
    """No speech is in progress.

    Reports ``PRISM_ERROR_NOT_SPEAKING``.
    """


class PrismNotPausedError(PrismError):
    """Speech is not paused.

    Reports ``PRISM_ERROR_NOT_PAUSED``.
    """


class PrismAlreadyPausedError(PrismError):
    """Speech is already paused.

    Reports ``PRISM_ERROR_ALREADY_PAUSED``.
    """


class PrismSpeakError(PrismError, IOError):
    """Speech synthesis failed.

    Reports ``PRISM_ERROR_SPEAK_FAILURE``.
    """


class PrismNoVoicesError(PrismError):
    """The backend has no voices.

    Reports ``PRISM_ERROR_NO_VOICES``.
    """


class PrismVoiceNotFoundError(PrismError, LookupError):
    """The voice does not exist.

    Reports ``PRISM_ERROR_VOICE_NOT_FOUND``. Backends report an out-of-range voice index either as this error or as :class:`PrismRangeError`.
    """


class PrismMemoryError(PrismError, MemoryError):
    """Memory allocation failed.

    Reports ``PRISM_ERROR_MEMORY_FAILURE``.
    """


class PrismUnknownError(PrismError):
    """An unclassified error occurred.

    Reports ``PRISM_ERROR_UNKNOWN``. Also reported for error values unknown to this version of the binding.
    """


class PrismInvalidAudioFormatError(PrismError, RuntimeError):
    """The audio format is not one Prism can handle.

    Reports ``PRISM_ERROR_INVALID_AUDIO_FORMAT``. The speech engine or voice uses an audio format Prism cannot convert, or reported format parameters that are not meaningful.
    """


class PrismInternalBackendLimitExceededError(PrismError, RuntimeError):
    """The backend's instance limit would be exceeded.

    Reports ``PRISM_ERROR_INTERNAL_BACKEND_LIMIT_EXCEEDED``. The backend allows only a fixed number of instances to exist at once.
    """


class PrismBackendEnteredUndefinedStateError(PrismError, RuntimeError):
    """The backend is in an undefined state.

    Reports ``PRISM_ERROR_BACKEND_ENTERED_UNDEFINED_STATE``. An operation failed in a way that left the backend unusable. The backend MUST NOT be used further.
    """


class PrismLibraryLoadFailedError(PrismError, OSError):
    """A shared library could not be opened.

    Reports ``PRISM_ERROR_LIBRARY_LOAD_FAILED``. The file does not exist, is not a loadable library, was built for another architecture, or failed to initialize.
    """


class PrismLibraryInvalidError(PrismError, ImportError):
    """A shared library is not a Prism plugin.

    Reports ``PRISM_ERROR_LIBRARY_INVALID``. The library does not export ``prism_plugin_query``.
    """


class PrismIncompatibleAbiError(PrismError, RuntimeError):
    """A plugin is incompatible with this version of Prism.

    Reports ``PRISM_ERROR_INCOMPATIBLE_ABI``. The plugin declined the host, or declared a plugin ABI generation this version does not accept.
    """


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
    lib.PRISM_ERROR_LIBRARY_LOAD_FAILED: PrismLibraryLoadFailedError,
    lib.PRISM_ERROR_LIBRARY_INVALID: PrismLibraryInvalidError,
    lib.PRISM_ERROR_INCOMPATIBLE_ABI: PrismIncompatibleAbiError,
}


def version() -> int:
    """Returns the version of the Prism library in use.

    This function wraps ``prism_version``. The version is that of the loaded
    native library, which MAY differ from the version of the ``prismatoid``
    package. It is encoded as ``(major << 16) | (minor << 8) | patch``, so
    version 1.4.7 is ``0x010407``; bits 24 through 31 are zero. Encoded versions
    compare in the same order as their major, minor, and patch components. The
    prerelease identifier and build metadata are not encoded, so ``1.0.0-rc.1``
    and ``1.0.0`` have the same encoded value; use :func:`version_string` to
    distinguish them. Prism follows Semantic Versioning, and while the major version is 0,
    minor releases MUST NOT be assumed compatible.

    Returns:
        The encoded version.
    """
    return lib.prism_version()


def version_string() -> str:
    """Returns the version of the Prism library in use, as a string.

    This function wraps ``prism_version_string``. The result is a Semantic
    Versioning 2.0.0 version string, including the prerelease identifier and
    build metadata if the version has them, such as ``"1.4.7"`` or
    ``"1.0.0-rc.1"``. It is also available as ``prism.__version__``.

    Returns:
        The version string.
    """
    return ffi.string(lib.prism_version_string()).decode("utf-8")


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
    """Feature information for a backend.

    Represents the mask returned by ``prism_backend_get_features``, with one
    ``bool`` field for each defined bit. A field is named after its
    ``PrismBackendFeature`` constant without the ``PRISM_BACKEND_`` prefix, in
    lower case: ``supports_speak`` is ``PRISM_BACKEND_SUPPORTS_SPEAK``.

    ``is_supported_at_runtime`` is ``True`` when the backend's screen reader or
    speech engine is available. It is advisory; obtaining the backend MAY still
    fail.

    A field beginning with ``supports_`` is ``True`` when the backend implements
    the operation it names, using the operation's C name:
    ``supports_count_voices`` concerns :attr:`prism.Backend.voices_count`, and
    ``supports_is_speaking`` concerns :attr:`prism.Backend.speaking`. An
    operation whose field is ``False`` raises :class:`PrismNotImplementedError`.
    ``supports_speak_ssml`` and ``supports_speak_to_memory_ssml`` are reserved.

    ``performs_silence_trimming_on_speak`` and
    ``performs_silence_trimming_on_speak_to_memory`` are ``True`` when the
    backend removes leading and trailing silence from spoken output, or from
    audio delivered by :meth:`prism.Backend.speak_to_memory`.

    ``is_screen_reader`` is ``True`` for a backend that sends messages to a
    screen reader, and ``is_speech_engine`` for one that synthesizes speech
    itself. At most one is ``True``. Every built-in backend sets exactly one,
    except :attr:`BackendId.CLIPBOARD` and :attr:`BackendId.NO_SPEECH`, which
    set neither; a custom backend MAY set neither.

    ``requires_explicit_selection`` is ``True`` for a backend that
    :meth:`prism.Context.create_best` and :meth:`prism.Context.acquire_best`
    never select. Such a backend is obtained only by requesting it by
    identifier or name.

    Instances are immutable and compare equal when all fields are equal.
    """

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
    is_screen_reader: bool = _bit(56)
    is_speech_engine: bool = _bit(57)
    requires_explicit_selection: bool = _bit(58)

    @classmethod
    def from_bits(cls, bits: int) -> BackendFeatures:
        """Creates feature information from a feature mask.

        Args:
            bits: A mask of ``PrismBackendFeature`` bits. Undefined bits are
                ignored.

        Returns:
            Feature information whose fields are ``True`` for the bits set in
            ``bits``.
        """
        return cls(
            **{f.name: bool(bits & (1 << f.metadata["bit"])) for f in fields(cls)},
        )

    def to_bits(self) -> int:
        """Returns the feature mask represented by this object.

        ``BackendFeatures.from_bits(features.to_bits())`` is equal to ``features``.

        Returns:
            A mask with the bit of each ``True`` field set.
        """
        bits: int = 0
        for f in fields(self):
            if getattr(self, f.name):
                bits |= 1 << f.metadata["bit"]
        return bits
