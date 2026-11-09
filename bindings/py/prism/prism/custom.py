# SPDX-License-Identifier: MPL-2.0

"""Custom backends and registries.

A :class:`RegistryBuilder` produces a :class:`Registry` containing the
compiled-in backends, backends implemented as subclasses of
:class:`CustomBackend`, and backends loaded from shared libraries. A registry
is passed to :class:`prism.Context`.
"""

from __future__ import annotations

import os
import threading
import traceback
from typing import TYPE_CHECKING, Final

from ._prism_cffi import ffi, lib
from .common import PrismError, PrismInvalidParamError
from .core import AudioCallback, BackendFeatures, _check_error

if TYPE_CHECKING:
    from collections.abc import Callable
    from typing import Self

from functools import reduce
from operator import or_

_FEATURE_BIT: Final[dict[str, int]] = {
    "speak": 1 << 2,
    "speak_to_memory": 1 << 3,
    "braille": 1 << 4,
    "output": 1 << 5,
    "is_speaking": 1 << 6,
    "stop": 1 << 7,
    "pause": 1 << 8,
    "resume": 1 << 9,
    "set_volume": 1 << 10,
    "get_volume": 1 << 11,
    "set_rate": 1 << 12,
    "get_rate": 1 << 13,
    "set_pitch": 1 << 14,
    "get_pitch": 1 << 15,
    "refresh_voices": 1 << 16,
    "count_voices": 1 << 17,
    "get_voice_name": 1 << 18,
    "get_voice_language": 1 << 19,
    "get_voice": 1 << 20,
    "set_voice": 1 << 21,
    "get_channels": 1 << 22,
    "get_sample_rate": 1 << 23,
    "get_bit_depth": 1 << 24,
}

_MAP: Final = object()
_METHOD_BITS: Final[int] = reduce(or_, _FEATURE_BIT.values(), 0)


class CustomBackend:
    """Base class of backends implemented in Python.

    A subclass is registered with :meth:`RegistryBuilder.add_backend` and is
    then used like any other backend. Prism instantiates the subclass with no
    arguments for each backend instance it creates, so the subclass MUST be
    constructible without arguments, and state shared between instances MUST be
    synchronized.

    A subclass overrides the operations it supports. The feature bit of an
    operation is set exactly when the subclass overrides the method of the same
    name; operations that are not overridden raise
    :class:`prism.common.PrismNotImplementedError` without calling Python.
    Methods are named after the members of ``PrismBackendVTable``:
    :attr:`prism.Backend.speaking` is implemented by :meth:`is_speaking`, and
    :attr:`prism.Backend.volume` by :meth:`get_volume` and :meth:`set_volume`.

    Each method MUST behave as described for the corresponding member of
    :class:`prism.Backend`. Prism validates arguments first, so a method never
    receives text that is not valid UTF-8 or a voice parameter outside
    [0.0, 1.0]. A method MAY receive empty text from callers of the C API.

    An exception raised by a method is printed to standard error and reported
    as an error: a :class:`prism.PrismError` as its own code,
    ``NotImplementedError`` as ``PRISM_ERROR_NOT_IMPLEMENTED``, ``UnicodeError``
    as ``PRISM_ERROR_INVALID_UTF8``, ``ValueError`` as
    ``PRISM_ERROR_INVALID_PARAM``, and any other exception as
    ``PRISM_ERROR_INTERNAL``. An exception raised by :meth:`is_supported` is
    reported as ``False``.

    The methods of one instance are never called concurrently, but MAY be
    called on any thread. Methods of different instances MAY be called
    concurrently.

    Attributes:
        EXTRA_FEATURES: Feature bits that no method determines: the
            silence-trimming bits, the classification bits, and
            ``requires_explicit_selection``. A subclass that presents
            messages through a screen reader SHOULD set ``is_screen_reader``;
            one that synthesizes speech SHOULD set ``is_speech_engine``. A
            subclass that sets ``requires_explicit_selection`` is never
            returned by :meth:`prism.Context.create_best` or
            :meth:`prism.Context.acquire_best`. Setting a bit that corresponds to a method
            raises ``ValueError`` at registration, and setting both
            classification bits raises
            :class:`prism.common.PrismInvalidParamError`.

    Example:
        A backend that writes speech requests to standard output::

            import prism

            class Printer(prism.CustomBackend):
                def speak(self, text, interrupt):
                    print(text)

            with prism.RegistryBuilder() as builder:
                printer_id = builder.add_backend("Printer", Printer, priority=10)
                registry = builder.freeze()
            context = prism.Context(registry)
            backend = context.create(prism.BackendId(printer_id))
            backend.speak("Hello from a custom backend")
    """

    EXTRA_FEATURES: BackendFeatures = BackendFeatures()

    def initialize(self) -> None:
        """Prepares the instance for use.

        Called once, after construction and before any operation other than
        :meth:`is_supported`. The default does nothing. An implementation whose
        screen reader or speech engine is unavailable SHOULD raise
        :class:`prism.common.PrismBackendNotAvailableError`.
        """

    def is_supported(self) -> bool:
        """Reports whether the backend is available.

        The result is reported as
        :attr:`prism.BackendFeatures.is_supported_at_runtime`, and is sampled by
        availability monitoring. This method MAY be called before
        :meth:`initialize`, and MUST NOT assume the instance is initialized. It is
        called on the sampling thread of contexts that monitor availability, and
        SHOULD return quickly.

        Returns:
            Whether the backend is available. The default is ``True``.
        """
        return True

    def speak(self, text: str, interrupt: bool) -> None:
        """Speaks text.

        Implements :meth:`prism.Backend.speak`.

        Args:
            text: The text to speak.
            interrupt: Whether to interrupt speech in progress.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def speak_to_memory(self, text: str, emit: AudioCallback) -> None:
        """Synthesizes text and passes the audio to ``emit``.

        Implements :meth:`prism.Backend.speak_to_memory`. The method MUST NOT return
        until all audio has been passed to ``emit``. Prism clamps samples to
        [-1.0, 1.0] and replaces non-finite values with 0.0 before delivering them.
        The channel count and sample rate passed to ``emit`` SHOULD match
        :meth:`get_channels` and :meth:`get_sample_rate`.

        Args:
            text: The text to synthesize.
            emit: A function that takes the next portion of audio, as described
                under :data:`prism.AudioCallback`.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def braille(self, text: str) -> None:
        """Shows text on a braille display.

        Implements :meth:`prism.Backend.braille`.

        Args:
            text: The text to show.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def output(self, text: str, interrupt: bool) -> None:
        """Presents text through speech and braille.

        Implements :meth:`prism.Backend.output`. Prism does not derive this operation from :meth:`speak` and :meth:`braille`; a backend that supports output MUST override it.

        Args:
            text: The text to present.
            interrupt: Whether to interrupt output in progress.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def stop(self) -> None:
        """Stops speech and discards queued speech.

        Implements :meth:`prism.Backend.stop`.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def pause(self) -> None:
        """Pauses speech.

        Implements :meth:`prism.Backend.pause`.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def resume(self) -> None:
        """Resumes paused speech.

        Implements :meth:`prism.Backend.resume`.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def is_speaking(self) -> bool:
        """Reports whether speech is in progress.

        Implements :attr:`prism.Backend.speaking`.

        Returns:
            Whether speech is in progress.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def set_volume(self, volume: float) -> None:
        """Sets the volume.

        Implements :attr:`prism.Backend.volume`.

        Args:
            volume: The volume, from 0.0 to 1.0.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def get_volume(self) -> float:
        """Returns the volume.

        Implements :attr:`prism.Backend.volume`.

        Returns:
            The volume, from 0.0 to 1.0.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def set_rate(self, rate: float) -> None:
        """Sets the rate.

        Implements :attr:`prism.Backend.rate`.

        Args:
            rate: The rate, from 0.0 to 1.0.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def get_rate(self) -> float:
        """Returns the rate.

        Implements :attr:`prism.Backend.rate`.

        Returns:
            The rate, from 0.0 to 1.0.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def set_pitch(self, pitch: float) -> None:
        """Sets the pitch.

        Implements :attr:`prism.Backend.pitch`.

        Args:
            pitch: The pitch, from 0.0 to 1.0.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def get_pitch(self) -> float:
        """Returns the pitch.

        Implements :attr:`prism.Backend.pitch`.

        Returns:
            The pitch, from 0.0 to 1.0.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def refresh_voices(self) -> None:
        """Reloads the list of voices.

        Implements :meth:`prism.Backend.refresh_voices`.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def count_voices(self) -> int:
        """Returns the number of voices.

        Implements :attr:`prism.Backend.voices_count`.

        Returns:
            The number of voices.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def get_voice_name(self, voice_id: int) -> str:
        """Returns the name of a voice.

        Implements :meth:`prism.Backend.get_voice_name`.

        Args:
            voice_id: The index of the voice.

        Returns:
            The name of the voice. It MUST NOT contain a NUL character.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def get_voice_language(self, voice_id: int) -> str:
        """Returns the language of a voice.

        Implements :meth:`prism.Backend.get_voice_language`.

        Args:
            voice_id: The index of the voice.

        Returns:
            The language of the voice, preferably a BCP 47 tag. It MUST NOT contain a NUL character.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def set_voice(self, voice_id: int) -> None:
        """Selects a voice.

        Implements :attr:`prism.Backend.voice`.

        Args:
            voice_id: The index of the voice.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def get_voice(self) -> int:
        """Returns the index of the selected voice.

        Implements :attr:`prism.Backend.voice`.

        Returns:
            The index of the selected voice.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def get_channels(self) -> int:
        """Returns the number of audio channels.

        Implements :attr:`prism.Backend.channels`.

        Returns:
            The number of channels.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def get_sample_rate(self) -> int:
        """Returns the audio sample rate.

        Implements :attr:`prism.Backend.sample_rate`.

        Returns:
            The sample rate, in hertz.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError

    def get_bit_depth(self) -> int:
        """Returns the native audio bit depth.

        Implements :attr:`prism.Backend.bit_depth`.

        Returns:
            The bit depth.

        Raises:
            NotImplementedError: Unless overridden.
        """
        raise NotImplementedError


def _error_for(exc: BaseException) -> int:
    if isinstance(exc, PrismError):
        return exc.code
    if isinstance(exc, NotImplementedError):
        return lib.PRISM_ERROR_NOT_IMPLEMENTED
    if isinstance(exc, UnicodeError):
        return lib.PRISM_ERROR_INVALID_UTF8
    if isinstance(exc, ValueError):
        return lib.PRISM_ERROR_INVALID_PARAM
    return lib.PRISM_ERROR_INTERNAL


def _text(ptr: ffi.CData) -> str:
    return ffi.string(ptr).decode("utf-8")


def _key(handle: ffi.CData) -> int:
    return int(ffi.cast("uintptr_t", handle))


class _InstanceState:
    __slots__ = ("obj", "str_anchor")

    def __init__(self, obj: CustomBackend) -> None:
        self.obj = obj
        self.str_anchor: ffi.CData | None = None


def _action(method: str, *readers: Callable[[object], object]) -> Callable[..., int]:
    def fn(instance_ptr: ffi.CData, *c_args: object) -> int:
        obj = ffi.from_handle(instance_ptr).obj
        getattr(obj, method)(
            *(read(a) for read, a in zip(readers, c_args, strict=True)),
        )
        return lib.PRISM_OK

    return fn


def _value_out(method: str, convert: Callable[[object], object]) -> Callable[..., int]:
    def fn(instance_ptr: ffi.CData, out_ptr: ffi.CData) -> int:
        obj = ffi.from_handle(instance_ptr).obj
        out_ptr[0] = convert(getattr(obj, method)())
        return lib.PRISM_OK

    return fn


def _string_out(method: str) -> Callable[..., int]:
    def fn(instance_ptr: ffi.CData, voice_id: int, out_ptr: ffi.CData) -> int:
        state = ffi.from_handle(instance_ptr)
        buf = ffi.new(
            "char[]",
            getattr(state.obj, method)(int(voice_id)).encode("utf-8"),
        )
        state.str_anchor = buf
        out_ptr[0] = buf
        return lib.PRISM_OK

    return fn


def _speak_to_memory(
    instance_ptr: ffi.CData,
    text_ptr: ffi.CData,
    c_callback: ffi.CData,
    c_userdata: ffi.CData,
) -> int:
    obj = ffi.from_handle(instance_ptr).obj

    def emit(samples: list[float], channels: int, sample_rate: int) -> None:
        buf = ffi.new("float[]", list(samples))
        c_callback(c_userdata, buf, len(buf), int(channels), int(sample_rate))

    obj.speak_to_memory(_text(text_ptr), emit)
    return lib.PRISM_OK


_SLOTS: Final = {
    "initialize": (_action("initialize"), None),
    "speak": (_action("speak", _text, bool), _FEATURE_BIT["speak"]),
    "speak_to_memory": (_speak_to_memory, _FEATURE_BIT["speak_to_memory"]),
    "braille": (_action("braille", _text), _FEATURE_BIT["braille"]),
    "output": (_action("output", _text, bool), _FEATURE_BIT["output"]),
    "stop": (_action("stop"), _FEATURE_BIT["stop"]),
    "pause": (_action("pause"), _FEATURE_BIT["pause"]),
    "resume": (_action("resume"), _FEATURE_BIT["resume"]),
    "is_speaking": (_value_out("is_speaking", bool), _FEATURE_BIT["is_speaking"]),
    "set_volume": (_action("set_volume", float), _FEATURE_BIT["set_volume"]),
    "get_volume": (_value_out("get_volume", float), _FEATURE_BIT["get_volume"]),
    "set_rate": (_action("set_rate", float), _FEATURE_BIT["set_rate"]),
    "get_rate": (_value_out("get_rate", float), _FEATURE_BIT["get_rate"]),
    "set_pitch": (_action("set_pitch", float), _FEATURE_BIT["set_pitch"]),
    "get_pitch": (_value_out("get_pitch", float), _FEATURE_BIT["get_pitch"]),
    "refresh_voices": (_action("refresh_voices"), _FEATURE_BIT["refresh_voices"]),
    "count_voices": (_value_out("count_voices", int), _FEATURE_BIT["count_voices"]),
    "get_voice_name": (_string_out("get_voice_name"), _FEATURE_BIT["get_voice_name"]),
    "get_voice_language": (
        _string_out("get_voice_language"),
        _FEATURE_BIT["get_voice_language"],
    ),
    "set_voice": (_action("set_voice", int), _FEATURE_BIT["set_voice"]),
    "get_voice": (_value_out("get_voice", int), _FEATURE_BIT["get_voice"]),
    "get_channels": (_value_out("get_channels", int), _FEATURE_BIT["get_channels"]),
    "get_sample_rate": (
        _value_out("get_sample_rate", int),
        _FEATURE_BIT["get_sample_rate"],
    ),
    "get_bit_depth": (_value_out("get_bit_depth", int), _FEATURE_BIT["get_bit_depth"]),
}

_LIVE: set[_Registration] = set()
_LIVE_LOCK: Final = threading.Lock()


class _Registration:
    def __init__(self, backend_cls: type[CustomBackend]) -> None:
        self._cls = backend_cls
        self._callbacks: list[ffi.CData] = []
        self._instances: dict[int, ffi.CData] = {}
        self._lock = threading.Lock()
        self.features = 0
        self.vtable = ffi.new("PrismBackendVTable*")
        self.vtable.size = ffi.sizeof("PrismBackendVTable")
        self.userdata = ffi.new_handle(self)
        self.userdata_free: ffi.CData | None = None
        self._build()

    def _make_cb(
        self,
        ctype: ffi.CType,
        fn: Callable[..., object],
        *,
        on_error: object,
    ) -> ffi.CData:
        def guarded(*args: object) -> object:
            try:
                return fn(*args)
            except BaseException as exc:  # noqa: BLE001
                traceback.print_exc()
                return _error_for(exc) if on_error is _MAP else on_error

        cb = ffi.callback(ctype, guarded)
        self._callbacks.append(cb)
        return cb

    def _install(
        self,
        field: str,
        fn: Callable[..., object],
        *,
        on_error: object,
    ) -> None:
        setattr(
            self.vtable,
            field,
            self._make_cb(
                ffi.typeof(getattr(self.vtable, field)),
                fn,
                on_error=on_error,
            ),
        )

    def _build(self) -> None:
        def create(_userdata: ffi.CData) -> ffi.CData:
            state = _InstanceState(self._cls())
            handle = ffi.new_handle(state)
            with self._lock:
                self._instances[_key(handle)] = handle
            return handle

        def destroy(instance_ptr: ffi.CData) -> None:
            with self._lock:
                self._instances.pop(_key(instance_ptr), None)

        def userdata_free(_userdata: ffi.CData) -> None:
            with _LIVE_LOCK:
                _LIVE.discard(self)

        def is_supported(instance_ptr: ffi.CData) -> bool:
            return ffi.from_handle(instance_ptr).obj.is_supported()

        self._install("create", create, on_error=ffi.NULL)
        self._install("destroy", destroy, on_error=None)
        self._install("is_supported", is_supported, on_error=False)

        for field, (trampoline, bit) in _SLOTS.items():
            if getattr(self._cls, field) is getattr(CustomBackend, field):
                continue
            self._install(field, trampoline, on_error=_MAP)
            if bit is not None:
                self.features |= bit
        extra_bits: int = self._cls.EXTRA_FEATURES.to_bits()
        offenders: int = extra_bits & _METHOD_BITS
        if offenders:
            raise ValueError(
                f"EXTRA_FEATURES declared method-derived capability bits ({offenders:#x}), which is not allowed"
            )
        self.features |= extra_bits
        self.userdata_free = self._make_cb("void(void *)", userdata_free, on_error=None)


class RegistryBuilder:
    """Builds a registry.

    This class wraps ``prism_registry_builder_new`` and
    ``prism_registry_builder_free``, and MAY be used as a context manager. A new
    builder contains the compiled-in backends, whose names and identifiers
    therefore cannot be used by other backends. Adding a backend creates no
    instance of it.

    :meth:`freeze` moves the builder's backends to a new registry and spends the
    builder; a spent builder can only be closed. The builder is closed by
    :meth:`close`, when the object is destroyed, or at the end of a ``with``
    statement.

    A builder is not thread-safe.
    """

    def __init__(self) -> None:
        """Creates a builder containing the compiled-in backends.

        Raises:
            MemoryError: If the builder cannot be created.
        """
        self._ptr = lib.prism_registry_builder_new()
        if self._ptr == ffi.NULL:
            msg = "prism_registry_builder_new failed"
            raise MemoryError(msg)
        self._frozen = False

    def add_backend(
        self,
        name: str,
        backend: type[CustomBackend],
        *,
        priority: int = 100,
    ) -> int:
        """Adds a backend implemented in Python.

        This method wraps ``prism_registry_builder_add_backend``. The backend's
        features are derived from ``backend`` as described under
        :class:`CustomBackend`. ``backend`` is kept alive for as long as the
        registry or any instance created from it.

        Args:
            name: The name of the backend. It MUST be non-empty and unique in the
                builder. It determines the identifier.
            backend: The subclass of :class:`CustomBackend` that implements the
                backend.
            priority: The priority of the backend. It MUST NOT be negative. The
                priorities of built-in backends are available from
                :meth:`prism.Context.priority_of`.

        Returns:
            The identifier of the backend, which :class:`prism.BackendId` converts.

        Raises:
            TypeError: If ``backend`` is not a subclass of :class:`CustomBackend`.
            RuntimeError: If the builder is spent or closed.
            ValueError: If ``EXTRA_FEATURES`` sets a bit that corresponds to a
                method.
            prism.common.PrismInvalidParamError: If ``name`` is empty, ``priority`` is
                negative, or both classification bits are set.
            prism.common.PrismInvalidOperationError: If the builder already contains a
                backend with the same name or identifier.
        """
        if not (isinstance(backend, type) and issubclass(backend, CustomBackend)):
            msg = "backend must be a CustomBackend subclass"
            raise TypeError(msg)
        if self._frozen or self._ptr == ffi.NULL:
            msg = "builder is spent"
            raise RuntimeError(msg)

        registration = _Registration(backend)
        with _LIVE_LOCK:
            _LIVE.add(registration)
        out_id = ffi.new("PrismBackendId*")
        err = lib.prism_registry_builder_add_backend(
            self._ptr,
            name.encode("utf-8"),
            priority,
            registration.features,
            registration.vtable,
            registration.userdata,
            registration.userdata_free,
            out_id,
        )
        _check_error(err)
        return int(out_id[0])

    def add_library(
        self,
        path: str | os.PathLike[str],
        *,
        priority_override: int | None = None,
    ) -> int:
        """Adds the backends of a shared library plugin.

        This method wraps ``prism_registry_builder_add_library``. If any error
        occurs, no backend is added and the library is unloaded. Otherwise one
        backend is added for each backend the plugin describes, in the plugin's
        order. The library remains loaded while the registry or any instance created
        from it exists.

        Loading a library runs its code with the privileges of the application,
        before Prism checks that it is a plugin. Applications MUST load only trusted
        libraries, from locations that less privileged users cannot write.

        The identifiers of the added backends are determined by their names, and
        are available from :meth:`prism.Context.id_of` once the registry is frozen.

        Args:
            path: The path of the library.
            priority_override: A non-negative priority for every backend in the
                library, or ``None`` to use the priorities the library declares.

        Returns:
            The number of backends added.

        Raises:
            RuntimeError: If the builder is spent or closed.
            TypeError: If ``path`` is not a ``str`` or a path-like object yielding
                one.
            prism.common.PrismInvalidParamError: If ``path`` is empty or contains a
                NUL character, or the library describes an inconsistent backend.
            ValueError: If ``priority_override`` is negative.
            prism.common.PrismNotImplementedError: If the platform does not support
                shared library backends.
            prism.common.PrismLibraryLoadFailedError: If the library cannot be
                opened.
            prism.common.PrismLibraryInvalidError: If the library is not a Prism
                plugin.
            prism.common.PrismIncompatibleAbiError: If the plugin is incompatible
                with this version of Prism.
            prism.common.PrismInvalidOperationError: If the builder already contains a
                backend with the name or identifier of one in the library.
        """
        if self._frozen or self._ptr == ffi.NULL:
            msg = "builder is spent"
            raise RuntimeError(msg)
        raw_path = os.fspath(path)
        if not isinstance(raw_path, str):
            msg = "path must be a str, or an os.PathLike yielding str"
            raise TypeError(msg)
        if len(raw_path) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Path MUST NOT be empty",
            )
        if "\x00" in raw_path:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Path MUST NOT contain embedded NULLs",
            )
        if priority_override is None:
            override = -1
        elif priority_override < 0:
            msg = (
                "priority_override MUST be non-negative; "
                "pass None to honor the priorities the plugin declares"
            )
            raise ValueError(msg)
        else:
            override = priority_override
        out_count = ffi.new("size_t*")
        _check_error(
            lib.prism_registry_builder_add_library(
                self._ptr,
                raw_path.encode("utf-8"),
                override,
                out_count,
            ),
        )
        return int(out_count[0])

    def freeze(self) -> Registry:
        """Creates a registry from the builder.

        This method wraps ``prism_registry_freeze``. The builder's backends are
        moved to the registry, and the builder is spent.

        Returns:
            The registry.

        Raises:
            RuntimeError: If the builder is spent or closed, or the registry cannot
                be created.
        """
        if self._frozen or self._ptr == ffi.NULL:
            msg = "builder is spent"
            raise RuntimeError(msg)
        ptr = lib.prism_registry_freeze(self._ptr)
        self._frozen = True
        if ptr == ffi.NULL:
            msg = "prism_registry_freeze failed"
            raise RuntimeError(msg)
        return Registry(ptr)

    def close(self) -> None:
        """Releases the builder.

        This method wraps ``prism_registry_builder_free``. Backends in a builder
        closed without being frozen are discarded. Closing a closed builder does
        nothing.
        """
        if getattr(self, "_ptr", ffi.NULL) != ffi.NULL:
            lib.prism_registry_builder_free(self._ptr)
            self._ptr = ffi.NULL

    __del__ = close

    def __enter__(self) -> Self:
        """Returns the builder.

        Returns:
            This builder.
        """
        return self

    def __exit__(self, *_exc: object) -> None:
        """Closes the builder."""
        self.close()


class Registry:
    """A registry produced by :meth:`RegistryBuilder.freeze`.

    A registry is passed to :class:`prism.Context`, and cannot be changed. The
    object holds a reference to the registry, released through
    ``prism_registry_release`` by :meth:`close`, when the object is destroyed, or
    at the end of a ``with`` statement. Each context holds a reference of its
    own, so the object MAY be closed once the registry is in use by a context.
    """

    def __init__(self, ptr: ffi.CData) -> None:
        """Takes ownership of a registry reference.

        Used by :meth:`RegistryBuilder.freeze`.

        Args:
            ptr: A ``PrismRegistry *`` whose reference the object owns.
        """
        self._ptr = ptr

    def close(self) -> None:
        """Releases the object's reference to the registry.

        Closing a closed registry does nothing.
        """
        if getattr(self, "_ptr", ffi.NULL) != ffi.NULL:
            lib.prism_registry_release(self._ptr)
            self._ptr = ffi.NULL

    __del__ = close

    def __enter__(self) -> Self:
        """Returns the registry.

        Returns:
            This registry.
        """
        return self

    def __exit__(self, *_exc: object) -> None:
        """Closes the registry."""
        self.close()
