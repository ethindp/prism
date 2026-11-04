"""Contexts and backends.

A :class:`Context` gives access to a registry of backends. A :class:`Backend`
is a screen reader or speech engine obtained from a context.
"""

from __future__ import annotations

import logging
import sys
from collections.abc import Callable
from typing import TYPE_CHECKING

from ._dispatch import _Dispatcher
from ._prism_cffi import ffi, lib
from .common import (
    BackendFeatures,
    BackendId,
    PrismInvalidParamError,
    _check_error,
)

if TYPE_CHECKING:
    from ._dispatch import AvailabilityCallback, BaselineCallback
    from .custom import Registry

AudioCallback = Callable[[list[float], int, int], None]
"""The type of a function that receives audio from :meth:`Backend.speak_to_memory`.

The function is called with a ``list`` of samples, the number of channels,
and the sample rate in hertz. Samples are ``float`` values from -1.0 to 1.0,
whatever the backend's native format. Channels are interleaved. The list MAY
be empty.

The ``emit`` function of :meth:`prism.CustomBackend.speak_to_memory` has the
same type.
"""


def _require_text(text: str, what: str = "Text") -> bytes:
    if len(text) == 0:
        raise PrismInvalidParamError(
            lib.PRISM_ERROR_INVALID_PARAM,
            f"{what} MUST NOT be empty",
        )
    if "\x00" in text:
        raise PrismInvalidParamError(
            lib.PRISM_ERROR_INVALID_PARAM,
            f"{what} MUST NOT contain embedded NULLs",
        )
    return text.encode("utf-8")


class Backend:
    """A backend instance.

    Backends are obtained from :class:`Context`, and are initialized before they
    are returned. This class SHOULD NOT be instantiated directly.

    The backend is released when the object is destroyed. A backend obtained
    through :meth:`Context.create` or :meth:`Context.create_best` is an
    independent instance. A backend obtained through :meth:`Context.acquire`,
    :meth:`Context.acquire_best`, or :meth:`Context.get` shares one instance
    with every object obtained in that way for the same backend: settings
    changed through one object are visible through the others, and the instance
    is released with the last of them. Releasing a backend MAY discard speech
    that is queued or playing, so a reference MUST be kept for as long as speech
    is expected to continue.

    A backend is not thread-safe. It MUST NOT be used from more than one thread
    at a time, and this applies to all objects sharing an instance. Distinct
    instances MAY be used from different threads.

    An operation the backend does not support raises
    :class:`prism.common.PrismNotImplementedError`; :attr:`features` reports
    which operations are supported. Other failures raise the subclass of
    :class:`prism.PrismError` for the error Prism reports. Each method lists the
    exceptions specific to it.

    Text arguments MUST be non-empty, MUST NOT contain a NUL character, and MUST
    be encodable as UTF-8.
    """

    _raw: ffi.CData = None

    def __init__(self, raw_ptr: ffi.CData) -> None:
        """Wraps and initializes a native backend.

        Used by :class:`Context`. This constructor wraps
        ``prism_backend_initialize``, and takes ownership of ``raw_ptr``. A backend
        that is already initialized is not initialized again.

        Args:
            raw_ptr: A ``PrismBackend *`` returned by a registry function.

        Raises:
            RuntimeError: If ``raw_ptr`` is ``NULL``.
            PrismBackendNotAvailableError: If a component the backend requires is
                missing or not running.
            PrismError: If initialization fails for another reason.
        """
        if raw_ptr == ffi.NULL:
            raise RuntimeError("Backend raw pointer MUST NOT be NULL!")
        self._raw = raw_ptr
        res = lib.prism_backend_initialize(self._raw)
        if res not in {lib.PRISM_OK, lib.PRISM_ERROR_ALREADY_INITIALIZED}:
            _check_error(res)

    def __del__(self) -> None:
        """Releases the backend.

        Nothing is released while the interpreter is finalizing.
        """
        if sys.is_finalizing():
            return
        lib.prism_backend_free(self._raw)
        self._raw = None

    @property
    def name(self) -> str:
        """The name of the backend, such as ``"NVDA"``.

        Wraps ``prism_backend_name``. The name is the one :meth:`Context.name_of`
        returns for the backend's identifier.
        """
        return ffi.string(lib.prism_backend_name(self._raw)).decode("utf-8")

    def speak(self, text: str, interrupt: bool = False) -> None:
        """Speaks text.

        This method wraps ``prism_backend_speak``. The text is spoken with the
        current voice, rate, pitch, and volume. Depending on the backend, the method
        returns when speech starts or when it ends; :attr:`speaking` reports
        whether speech is still in progress, where supported.

        A screen reader backend passes the text to the screen reader, which speaks it
        according to the user's settings. Some backends limit the length of text;
        long text SHOULD be divided into shorter segments.

        Args:
            text: The text to speak.
            interrupt: If ``True``, speech in progress is stopped and queued speech
                is discarded before the text is spoken. If ``False``, the text is
                queued after speech in progress on most backends; some mix the two,
                and screen readers MAY apply their own setting.

        Raises:
            PrismInvalidParamError: If ``text`` is empty or contains a NUL
                character.
            UnicodeEncodeError: If ``text`` cannot be encoded as UTF-8.
            PrismNotImplementedError: If the backend does not support speech.
            PrismSpeakError: If synthesis fails.
        """
        encoded = _require_text(text)
        _check_error(lib.prism_backend_speak(self._raw, encoded, interrupt))

    def speak_to_memory(self, text: str, on_audio_data: AudioCallback) -> None:
        """Synthesizes text and passes the audio to a function.

        This method wraps ``prism_backend_speak_to_memory``. It returns when
        synthesis is complete and all audio has been passed to ``on_audio_data``,
        on every backend.

        ``on_audio_data`` is called one or more times, each time with the next
        portion of the audio, as described under :data:`AudioCallback`. It MAY be
        called on a thread other than the calling thread, and MUST NOT use this
        backend. If it raises an exception, it is not called again for this
        synthesis, and the exception is re-raised by this method when synthesis
        ends.

        The format of the audio is reported in advance by :attr:`channels` and
        :attr:`sample_rate`. Calling :meth:`stop` during synthesis has undefined
        behavior.

        Args:
            text: The text to synthesize.
            on_audio_data: The function that receives the audio.

        Raises:
            PrismInvalidParamError: If ``text`` is empty or contains a NUL
                character.
            UnicodeEncodeError: If ``text`` cannot be encoded as UTF-8.
            PrismNotImplementedError: If the backend does not support synthesis to
                memory. No screen reader backend supports it.
            PrismInvalidAudioFormatError: If the backend produces audio Prism cannot
                convert.
            PrismSpeakError: If synthesis fails.
        """
        encoded = _require_text(text)
        captured: BaseException | None = None

        @ffi.callback("void(void *, const float *, size_t, size_t, size_t)")
        def audio_callback_shim(
            _userdata: ffi.CData,
            samples_ptr: ffi.CData,
            count: int,
            channels: int,
            rate: int,
        ) -> None:
            nonlocal captured
            if captured is not None:
                return
            try:
                pcm_data = ffi.unpack(samples_ptr, count) if count > 0 else []
                on_audio_data(pcm_data, channels, rate)
            except BaseException as exc:  # noqa: BLE001
                captured = exc

        self._active_callback = audio_callback_shim
        res = lib.prism_backend_speak_to_memory(
            self._raw,
            encoded,
            audio_callback_shim,
            ffi.NULL,
        )
        if captured is not None:
            raise captured
        _check_error(res)

    def braille(self, text: str) -> None:
        """Shows text on a braille display.

        This method wraps ``prism_backend_braille``. The text is shown according to
        the screen reader's braille settings. Braille output is independent of
        speech. Some backends succeed when no display is connected and others raise
        an exception, so the outcome does not indicate whether a display is present.

        Args:
            text: The text to show.

        Raises:
            PrismInvalidParamError: If ``text`` is empty or contains a NUL
                character.
            UnicodeEncodeError: If ``text`` cannot be encoded as UTF-8.
            PrismNotImplementedError: If the backend does not support braille. No
                speech engine backend supports it.
        """
        encoded = _require_text(text)
        _check_error(lib.prism_backend_braille(self._raw, encoded))

    def output(self, text: str, interrupt: bool = False) -> None:
        """Presents text through speech and braille.

        This method wraps ``prism_backend_output``. A screen reader backend speaks
        the text and shows it on the braille display; a speech engine backend
        speaks it, as :meth:`speak` does. Messages meant for the user SHOULD be
        presented with this method, since a screen reader user may rely on braille
        alone. The method MAY succeed when one of the two fails.

        Args:
            text: The text to present.
            interrupt: As for :meth:`speak`.

        Raises:
            PrismInvalidParamError: If ``text`` is empty or contains a NUL
                character.
            UnicodeEncodeError: If ``text`` cannot be encoded as UTF-8.
            PrismNotImplementedError: If the backend does not support output.
            PrismSpeakError: If output fails.
        """
        encoded = _require_text(text)
        _check_error(lib.prism_backend_output(self._raw, encoded, interrupt))

    def stop(self) -> None:
        """Stops speech and discards queued speech.

        This method wraps ``prism_backend_stop``. After it returns,
        :attr:`speaking` is ``False``. A screen reader backend stops only speech it
        produced, not speech the screen reader produces on its own.

        Calling this method during :meth:`speak_to_memory` has undefined behavior.

        Raises:
            PrismNotImplementedError: If the backend does not support stopping.
            PrismNotSpeakingError: If no speech is in progress, on some backends.
                Most backends succeed in that case.
        """
        _check_error(lib.prism_backend_stop(self._raw))

    def pause(self) -> None:
        """Pauses speech.

        This method wraps ``prism_backend_pause``. Queued speech is kept, and
        :meth:`resume` continues from the paused position. While paused,
        :attr:`speaking` is ``False``. Speaking with ``interrupt=True`` discards the
        paused speech; speaking with ``interrupt=False`` queues after it.

        Raises:
            PrismNotImplementedError: If the backend does not support pausing.
            PrismNotSpeakingError: If no speech is in progress.
            PrismAlreadyPausedError: If speech is already paused.
        """
        _check_error(lib.prism_backend_pause(self._raw))

    def resume(self) -> None:
        """Resumes speech paused by :meth:`pause`.

        This method wraps ``prism_backend_resume``.

        Raises:
            PrismNotImplementedError: If the backend does not support resuming.
            PrismNotPausedError: If speech is not paused, including after
                :meth:`stop`.
        """
        _check_error(lib.prism_backend_resume(self._raw))

    @property
    def speaking(self) -> bool:
        """Whether speech is in progress.

        Wraps ``prism_backend_is_speaking``. Paused speech is not in progress. The
        value MAY remain ``True`` briefly after the last audio is heard. Most screen
        reader backends do not support this property and raise
        :class:`PrismNotImplementedError`.
        """
        p_speaking = ffi.new("bool*")
        _check_error(lib.prism_backend_is_speaking(self._raw, p_speaking))
        return p_speaking[0]

    @property
    def volume(self) -> float:
        """The speech volume, from 0.0 (silent) to 1.0 (loudest).

        Wraps ``prism_backend_get_volume`` and ``prism_backend_set_volume``. The
        volume is relative to the backend and independent of the system volume. 0.5
        is the backend's default level. A new value applies to subsequent speech.

        Screen reader backends do not support this property and raise
        :class:`PrismNotImplementedError`. Setting a value outside [0.0, 1.0], NaN,
        or infinity raises :class:`PrismRangeError`.
        """
        p_volume = ffi.new("float*")
        _check_error(lib.prism_backend_get_volume(self._raw, p_volume))
        return p_volume[0]

    @volume.setter
    def volume(self, volume: float) -> None:
        _check_error(lib.prism_backend_set_volume(self._raw, volume))

    @property
    def rate(self) -> float:
        """The speech rate, from 0.0 (slowest) to 1.0 (fastest).

        Wraps ``prism_backend_get_rate`` and ``prism_backend_set_rate``. 0.5 is
        always the backend's default rate. The rate changes tempo but not pitch. A
        new value applies to subsequent speech.

        Screen reader backends do not support this property and raise
        :class:`PrismNotImplementedError`. Setting a value outside [0.0, 1.0] raises
        :class:`PrismRangeError`.
        """
        p_rate = ffi.new("float*")
        _check_error(lib.prism_backend_get_rate(self._raw, p_rate))
        return p_rate[0]

    @rate.setter
    def rate(self, rate: float) -> None:
        _check_error(lib.prism_backend_set_rate(self._raw, rate))

    @property
    def pitch(self) -> float:
        """The speech pitch, from 0.0 (lowest) to 1.0 (highest).

        Wraps ``prism_backend_get_pitch`` and ``prism_backend_set_pitch``. 0.5 is
        the backend's default pitch. A new value applies to subsequent speech.

        Screen reader backends, and speech engines whose voices have a fixed pitch,
        do not support this property and raise :class:`PrismNotImplementedError`.
        Setting a value outside [0.0, 1.0] raises :class:`PrismRangeError`.
        """
        p_pitch = ffi.new("float*")
        _check_error(lib.prism_backend_get_pitch(self._raw, p_pitch))
        return p_pitch[0]

    @pitch.setter
    def pitch(self, pitch: float) -> None:
        _check_error(lib.prism_backend_set_pitch(self._raw, pitch))

    def refresh_voices(self) -> None:
        """Reloads the list of voices from the system.

        This method wraps ``prism_backend_refresh_voices``. Voices installed after
        the backend was initialized appear only after a refresh. A refresh MAY change
        voice indices, so a stored choice of voice SHOULD be identified by name.
        Backends that read their voices only during initialization succeed without
        changing the list.

        Raises:
            PrismNotImplementedError: If the backend does not support voices. No
                screen reader backend supports them.
        """
        _check_error(lib.prism_backend_refresh_voices(self._raw))

    @property
    def voices_count(self) -> int:
        """The number of voices.

        Wraps ``prism_backend_count_voices``. Voice indices run from 0 to one less
        than this value. The value changes only after :meth:`refresh_voices`.
        Backends without voice support raise :class:`PrismNotImplementedError`.
        """
        out_count = ffi.new("size_t*")
        _check_error(lib.prism_backend_count_voices(self._raw, out_count))
        return out_count[0]

    def get_voice_name(self, idx: int) -> str:
        """Returns the name of a voice.

        This method wraps ``prism_backend_get_voice_name``. Names come from the
        speech engine and are often localized.

        Args:
            idx: The index of the voice.

        Returns:
            The name of the voice, such as ``"Microsoft David"``.

        Raises:
            PrismRangeError: If ``idx`` is out of range.
            PrismNotImplementedError: If the backend does not support voices.
        """
        pp_name = ffi.new("char **")
        _check_error(lib.prism_backend_get_voice_name(self._raw, idx, pp_name))
        return ffi.string(pp_name[0]).decode("utf-8")

    def get_voice_language(self, idx: int) -> str:
        """Returns the language of a voice.

        This method wraps ``prism_backend_get_voice_language``.

        Args:
            idx: The index of the voice.

        Returns:
            The language of the voice, usually a BCP 47 tag such as ``"en-US"``.

        Raises:
            PrismRangeError: If ``idx`` is out of range.
            PrismNotImplementedError: If the backend does not support voices.
        """
        pp_lang = ffi.new("char **")
        _check_error(lib.prism_backend_get_voice_language(self._raw, idx, pp_lang))
        return ffi.string(pp_lang[0]).decode("utf-8")

    @property
    def voice(self) -> int:
        """The index of the voice used for speech.

        Wraps ``prism_backend_get_voice`` and ``prism_backend_set_voice``. Until a
        voice is set, the value is the index of the backend's default voice. A new
        voice applies to subsequent speech, and MAY change :attr:`channels`,
        :attr:`sample_rate`, and :attr:`bit_depth`. After :meth:`refresh_voices`,
        the value is the voice's new index; if the voice no longer exists, the
        behavior is undefined.

        Setting an index not less than :attr:`voices_count` raises
        :class:`PrismRangeError` or :class:`PrismVoiceNotFoundError`. If setting the
        voice leaves the backend unusable, the setter raises
        :class:`PrismBackendEnteredUndefinedStateError`. Backends without voice
        support raise :class:`PrismNotImplementedError`.
        """
        out_voice_id = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_voice(self._raw, out_voice_id))
        return out_voice_id[0]

    @voice.setter
    def voice(self, idx: int) -> None:
        _check_error(lib.prism_backend_set_voice(self._raw, idx))

    @property
    def channels(self) -> int:
        """The number of channels in the audio of :meth:`speak_to_memory`.

        Wraps ``prism_backend_get_channels``. Usually 1. The value depends on the
        voice. Backends without synthesis to memory raise
        :class:`PrismNotImplementedError`.
        """
        out_channels = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_channels(self._raw, out_channels))
        return out_channels[0]

    @property
    def sample_rate(self) -> int:
        """The sample rate, in hertz, of the audio of :meth:`speak_to_memory`.

        Wraps ``prism_backend_get_sample_rate``. Common values are 16000, 22050,
        44100, and 48000. The value depends on the voice. Backends without synthesis
        to memory raise :class:`PrismNotImplementedError`.
        """
        out_sample_rate = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_sample_rate(self._raw, out_sample_rate))
        return out_sample_rate[0]

    @property
    def bit_depth(self) -> int:
        """The bit depth of the speech engine's native audio.

        Wraps ``prism_backend_get_bit_depth``. Common values are 8 and 16. Audio
        from :meth:`speak_to_memory` is always delivered as ``float`` samples, so the
        value is informational. Backends without synthesis to memory raise
        :class:`PrismNotImplementedError`.
        """
        out_bit_depth = ffi.new("size_t*")
        _check_error(lib.prism_backend_get_bit_depth(self._raw, out_bit_depth))
        return out_bit_depth[0]

    @property
    def features(self) -> BackendFeatures:
        """Feature information for the backend.

        Wraps ``prism_backend_get_features``. The backend is queried on each access,
        so :attr:`BackendFeatures.is_supported_at_runtime` is current. Determining
        it MAY require probing for a running process or service, so the result
        SHOULD be kept rather than read repeatedly when only the other fields are
        needed.
        """
        return BackendFeatures.from_bits(lib.prism_backend_get_features(self._raw))


class Context:
    """A Prism context.

    This class wraps ``prism_init`` and ``prism_shutdown``. A context is bound to
    a registry when it is created: the global registry, which contains the
    backends compiled into Prism, or a :class:`prism.Registry`, which MAY also
    contain custom backends and backends from shared libraries. Backends are
    ordered by priority, highest first; screen readers have higher priority than
    speech engines.

    :meth:`create` and :meth:`create_best` return independent instances.
    :meth:`acquire`, :meth:`acquire_best`, and :meth:`get` return a shared
    instance, as described under :class:`Backend`.

    The methods of a context MAY be called from any thread. The context is shut
    down when the object is destroyed. Backends obtained from it remain usable.

    Availability monitoring:
        A context created with ``on_availability`` samples the availability of
        every backend on a background thread, and calls ``on_availability`` when
        a backend becomes available or unavailable. A context created without it
        starts no thread.

        The first sample of each backend sets a baseline and is not reported. A
        change is reported once it has been observed on ``debounce_samples``
        consecutive samples. If ``backoff_max_ms`` is greater than
        ``poll_interval_ms``, the interval between samples doubles while nothing
        changes, up to ``backoff_max_ms``, and returns to ``poll_interval_ms``
        when a change is observed.

        The baseline is set after the constructor returns. A change that occurs
        between the application's own check of a backend and the baseline sample
        of that backend is part of the baseline and is not reported.
        ``on_baseline`` is called once the baseline is set, before any call to
        ``on_availability``. An application that checks availability before
        then, and keeps the result, SHOULD check again when ``on_baseline`` is
        called. A later notification MAY report a change it has already seen.

        Callbacks are not called on Prism's sampling thread. Each notification
        is passed to a thread of the binding that runs an ``asyncio`` event loop,
        so a callback MAY block. A callback MAY be a coroutine function, in which
        case its coroutine runs on that loop. At most one coroutine runs at a
        time for each backend; a newer notification for the backend replaces one
        that is waiting to run. Exceptions from callbacks are logged to the
        ``prism.dispatch`` logger. Coroutines still running when the context is
        destroyed are cancelled. Callbacks MUST synchronize access to shared
        state, including backends used on other threads.
    """

    _ctx: ffi.CData = None
    _registry: Registry | None
    _dispatcher: _Dispatcher | None
    _availability_cb: ffi.CData | None
    _baseline_cb: ffi.CData | None
    _on_availability: AvailabilityCallback | None

    def __init__(
        self,
        registry: Registry | None = None,
        *,
        on_availability: AvailabilityCallback | None = None,
        on_baseline: BaselineCallback | None = None,
        poll_interval_ms: int = 0,
        debounce_samples: int = 0,
        backoff_max_ms: int = 0,
        auto_power_manage: bool = True,
    ) -> None:
        """Creates a context.

        Args:
            registry: The registry to use, or ``None`` for the global registry. The
                context keeps its own reference, so ``registry`` MAY be closed
                afterwards.
            on_availability: A function called with a :class:`BackendId`, the
                backend's name, and ``True`` if the backend became available or
                ``False`` if it became unavailable; or ``None`` to disable
                availability monitoring. It MAY be a coroutine function. The
                remaining arguments are ignored when it is ``None``.
            on_baseline: A function called with no arguments when the baseline is
                set, or ``None``. It MAY be a coroutine function.
            poll_interval_ms: The base interval between samples, in milliseconds. 0
                selects 1000.
            debounce_samples: The number of consecutive samples that MUST agree
                before a change is reported. 0 selects 2; 1 reports changes
                immediately.
            backoff_max_ms: The longest interval between samples, in milliseconds.
                0, or a value not greater than ``poll_interval_ms``, keeps the
                interval constant.
            auto_power_manage: Whether to stop sampling while the system is
                suspended. It has an effect only where :meth:`auto_power_supported`
                returns ``True``.

        Raises:
            RuntimeError: If the context cannot be created.
        """
        self._ctx = None
        self._registry = registry
        self._dispatcher = None
        self._availability_cb = None
        self._baseline_cb = None
        self._on_availability = on_availability
        config = lib.prism_config_init()
        cfg = ffi.new("PrismConfig *", config)
        if registry is not None:
            cfg.registry = registry._ptr  # noqa: SLF001
        if on_availability is not None:
            self._dispatcher = _Dispatcher()
            dispatcher: _Dispatcher = self._dispatcher
            user_cb: AvailabilityCallback = on_availability

            @ffi.callback("void(void*, PrismBackendId, const char*, bool)")
            def _availability_trampoline(
                _userdata: ffi.CData,
                backend: int,
                name_ptr: ffi.CData,
                available: bool,
            ) -> None:
                try:
                    name: str = (
                        ffi.string(name_ptr).decode("utf-8", "replace")
                        if name_ptr != ffi.NULL
                        else ""
                    )
                    dispatcher.submit(
                        user_cb, BackendId(backend), name, bool(available)
                    )
                except BaseException:
                    logging.getLogger("prism.dispatch").exception(
                        "availability trampoline failed"
                    )

            self._availability_cb = _availability_trampoline
            cfg.availability_callback = _availability_trampoline
            if on_baseline is not None:
                baseline_user_cb: BaselineCallback = on_baseline

                @ffi.callback("void(void*)")
                def _baseline_trampoline(_userdata: ffi.CData) -> None:
                    try:
                        dispatcher.submit_baseline(baseline_user_cb)
                    except BaseException:
                        logging.getLogger("prism.dispatch").exception(
                            "baseline trampoline failed"
                        )

                self._baseline_cb = _baseline_trampoline
                cfg.availability_baseline_callback = _baseline_trampoline
            cfg.availability_userdata = ffi.NULL
            cfg.availability_poll_interval_ms = poll_interval_ms
            cfg.availability_debounce_samples = debounce_samples
            cfg.availability_backoff_max_ms = backoff_max_ms
            cfg.availability_auto_power_manage = auto_power_manage
        self._ctx = lib.prism_init(cfg)
        if self._ctx == ffi.NULL:
            if self._dispatcher is not None:
                self._dispatcher.close()
                self._dispatcher = None
            raise RuntimeError("Prism could not be initialized")

    def __del__(self) -> None:
        """Shuts down the context.

        Nothing is released while the interpreter is finalizing.
        """
        if sys.is_finalizing():
            return
        if getattr(self, "_ctx", None):
            lib.prism_shutdown(self._ctx)
            self._ctx = None
        if getattr(self, "_dispatcher", None) is not None:
            self._dispatcher.close()
            self._dispatcher = None

    @property
    def backends_count(self) -> int:
        """The number of backends in the registry.

        Wraps ``prism_registry_count``. The value includes backends that cannot be
        used on the current system, and does not change for the life of the
        registry. 0 means Prism was built without backends for the platform.
        """
        return lib.prism_registry_count(self._ctx)

    def id_of(self, index_or_name: int | str) -> BackendId:
        """Returns the identifier of a backend, given its position or its name.

        This method wraps ``prism_registry_id_at`` and ``prism_registry_id``.
        Positions run from 0, the highest-priority backend, to one less than
        :attr:`backends_count`. Names are compared exactly, including case.

        Args:
            index_or_name: The position of the backend in the registry, or its
                name.

        Returns:
            The identifier of the backend.

        Raises:
            KeyError: If no backend is at the position or has the name.
            TypeError: If ``index_or_name`` is neither an ``int`` nor a ``str``. A
                ``bool`` is not accepted.
        """
        if isinstance(index_or_name, int) and not isinstance(index_or_name, bool):
            res = lib.prism_registry_id_at(self._ctx, index_or_name)
        elif isinstance(index_or_name, str):
            res = lib.prism_registry_id(self._ctx, index_or_name.encode("utf-8"))
        else:
            raise TypeError("Expected int or string")
        if res == 0:
            raise KeyError(f"No backend matching {index_or_name!r}")
        return BackendId(res)

    def name_of(self, backend_id: BackendId) -> str:
        """Returns the name of a backend.

        This method wraps ``prism_registry_name``.

        Args:
            backend_id: The identifier of the backend.

        Returns:
            The name of the backend.

        Raises:
            KeyError: If the registry does not contain the backend.
        """
        c_ptr = lib.prism_registry_name(self._ctx, backend_id)
        if c_ptr == ffi.NULL:
            raise KeyError("Backend ID not found")
        return ffi.string(c_ptr).decode("utf-8")

    def priority_of(self, backend_id: BackendId) -> int:
        """Returns the priority of a backend.

        This method wraps ``prism_registry_priority``. Priorities are non-negative;
        higher priorities are tried first by :meth:`create_best` and
        :meth:`acquire_best`. The priorities of built-in backends MAY change between
        releases.

        Args:
            backend_id: The identifier of the backend.

        Returns:
            The priority, or -1 if the registry does not contain the backend.
        """
        return lib.prism_registry_priority(self._ctx, backend_id)

    def exists(self, backend_id: BackendId) -> bool:
        """Reports whether the registry contains a backend.

        This method wraps ``prism_registry_exists``. It does not report whether the
        backend can be used; see :attr:`BackendFeatures.is_supported_at_runtime`.

        Args:
            backend_id: The identifier of the backend.

        Returns:
            ``True`` if the registry contains the backend.
        """
        return bool(lib.prism_registry_exists(self._ctx, backend_id))

    def get(self, backend_id: BackendId) -> Backend | None:
        """Returns the shared instance of a backend, if one exists.

        This method wraps ``prism_registry_get``. It never creates an instance. It
        returns the instance obtained through :meth:`acquire` or
        :meth:`acquire_best` while any object referring to that instance remains.

        Args:
            backend_id: The identifier of the backend.

        Returns:
            The shared instance, or ``None``.

        Raises:
            PrismError: If the instance exists but cannot be initialized.
        """
        res = lib.prism_registry_get(self._ctx, int(backend_id))
        if res == ffi.NULL:
            return None
        return Backend(res)

    def create(self, backend_id: BackendId) -> Backend:
        """Creates and initializes an instance of a backend.

        This method wraps ``prism_registry_create`` and
        ``prism_backend_initialize``. Each call creates an independent instance.
        Initialization MAY take noticeable time.

        Args:
            backend_id: The identifier of the backend.

        Returns:
            The backend.

        Raises:
            ValueError: If the registry does not contain the backend, or the
                backend cannot be created.
            PrismBackendNotAvailableError: If a component the backend requires is
                missing or not running.
            PrismError: If initialization fails for another reason.
        """
        res = lib.prism_registry_create(self._ctx, int(backend_id))
        if res == ffi.NULL:
            raise ValueError("Invalid or unsupported backend")
        return Backend(res)

    def create_best(self) -> Backend:
        """Creates and initializes the highest-priority backend that can be initialized.

        This method wraps ``prism_registry_create_best``. Backends are tried in
        priority order, and the first that initializes is returned. A screen reader
        backend initializes only while its screen reader is running, so the result
        is the user's screen reader if one is running, and otherwise a speech
        engine.

        The choice is not revisited: a backend obtained while no screen reader was
        running remains a speech engine if one is started later. Availability
        monitoring, described under :class:`Context`, reports such changes.

        Returns:
            The backend.

        Raises:
            ValueError: If no backend can be initialized, as on a system with no
                speech engine. Applications MUST handle this case.
        """
        res = lib.prism_registry_create_best(self._ctx)
        if res == ffi.NULL:
            raise ValueError("Invalid or unsupported backend")
        return Backend(res)

    def acquire(self, backend_id: BackendId) -> Backend:
        """Returns the shared instance of a backend, creating it if necessary.

        This method wraps ``prism_registry_acquire`` and
        ``prism_backend_initialize``. The instance is shared as described under
        :class:`Backend`. :meth:`create` SHOULD be used unless the instance is meant
        to be shared.

        Args:
            backend_id: The identifier of the backend.

        Returns:
            The backend.

        Raises:
            ValueError: If the registry does not contain the backend, or the
                backend cannot be created.
            PrismBackendNotAvailableError: If a component the backend requires is
                missing or not running.
            PrismError: If initialization fails for another reason.
        """
        res = lib.prism_registry_acquire(self._ctx, int(backend_id))
        if res == ffi.NULL:
            raise ValueError("Invalid or unsupported backend")
        return Backend(res)

    def acquire_best(self) -> Backend:
        """Returns the shared instance of the highest-priority backend that can be initialized.

        This method wraps ``prism_registry_acquire_best``. Backends are tried in
        priority order, as for :meth:`create_best`, using existing shared instances
        where they exist. :meth:`create_best` SHOULD be used unless the instance is
        meant to be shared.

        Returns:
            The backend.

        Raises:
            ValueError: If no backend can be initialized.
        """
        res = lib.prism_registry_acquire_best(self._ctx)
        if res == ffi.NULL:
            raise ValueError("Invalid or unsupported backend")
        return Backend(res)

    def pause_availability_polling(self) -> None:
        """Pauses availability monitoring.

        This method wraps ``prism_availability_poll_pause``. While paused, the
        sampling thread uses no processor time. A sample in progress completes. The
        method does nothing if monitoring is disabled or already paused.
        """
        lib.prism_availability_poll_pause(self._ctx)

    def resume_availability_polling(self) -> None:
        """Resumes availability monitoring.

        This method wraps ``prism_availability_poll_resume``. Every backend is
        sampled immediately, and each whose availability differs from what was last
        reported is reported without debouncing. A change that occurred and reverted
        while paused is not reported. The method does nothing if monitoring is
        disabled or not paused.
        """
        lib.prism_availability_poll_resume(self._ctx)

    @staticmethod
    def auto_power_supported() -> bool:
        """Reports whether ``auto_power_manage`` has an effect.

        This method wraps ``prism_availability_auto_power_supported``. Where it
        returns ``False``, sampling continues while the system is suspended unless
        :meth:`pause_availability_polling` is called. The result depends only on the
        platform and build, and the method MAY be called at any time.

        Returns:
            ``True`` if automatic power management is supported.
        """
        return bool(lib.prism_availability_auto_power_supported())
