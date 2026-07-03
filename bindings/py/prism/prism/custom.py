# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import threading
import traceback
from typing import TYPE_CHECKING, Final

from .core import AudioCallback, _check_error
from .lib import ffi, lib

if TYPE_CHECKING:
    from collections.abc import Callable
    from typing import Self

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


class CustomBackend:
    """Base class for a Python-implemented Prism backend.

    Override only the operations you actually support.
    """

    def initialize(self) -> None: ...
    def is_supported(self) -> bool:
        return True

    def speak(self, text: str, interrupt: bool) -> None:
        raise NotImplementedError

    def speak_to_memory(self, text: str, emit: AudioCallback) -> None:
        raise NotImplementedError

    def braille(self, text: str) -> None:
        raise NotImplementedError

    def output(self, text: str, interrupt: bool) -> None:
        raise NotImplementedError

    def stop(self) -> None:
        raise NotImplementedError

    def pause(self) -> None:
        raise NotImplementedError

    def resume(self) -> None:
        raise NotImplementedError

    def is_speaking(self) -> bool:
        raise NotImplementedError

    def set_volume(self, volume: float) -> None:
        raise NotImplementedError

    def get_volume(self) -> float:
        raise NotImplementedError

    def set_rate(self, rate: float) -> None:
        raise NotImplementedError

    def get_rate(self) -> float:
        raise NotImplementedError

    def set_pitch(self, pitch: float) -> None:
        raise NotImplementedError

    def get_pitch(self) -> float:
        raise NotImplementedError

    def refresh_voices(self) -> None:
        raise NotImplementedError

    def count_voices(self) -> int:
        raise NotImplementedError

    def get_voice_name(self, voice_id: int) -> str:
        raise NotImplementedError

    def get_voice_language(self, voice_id: int) -> str:
        raise NotImplementedError

    def set_voice(self, voice_id: int) -> None:
        raise NotImplementedError

    def get_voice(self) -> int:
        raise NotImplementedError

    def get_channels(self) -> int:
        raise NotImplementedError

    def get_sample_rate(self) -> int:
        raise NotImplementedError

    def get_bit_depth(self) -> int:
        raise NotImplementedError


def _error_for(exc: BaseException) -> int:
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
    __slots__ = ("handle", "obj", "str_anchor")

    def __init__(self, obj: CustomBackend) -> None:
        self.obj = obj
        self.handle: ffi.CData | None = None
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
        self._instances: dict[int, _InstanceState] = {}
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
            state.handle = ffi.new_handle(state)
            with self._lock:
                self._instances[_key(state.handle)] = state
            return state.handle

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

        self.userdata_free = self._make_cb("void(void *)", userdata_free, on_error=None)


class RegistryBuilder:
    def __init__(self) -> None:
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

    def freeze(self) -> Registry:
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
        if getattr(self, "_ptr", ffi.NULL) != ffi.NULL:
            lib.prism_registry_builder_free(self._ptr)
            self._ptr = ffi.NULL

    __del__ = close

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()


class Registry:
    def __init__(self, ptr: ffi.CData) -> None:
        self._ptr = ptr

    def close(self) -> None:
        if getattr(self, "_ptr", ffi.NULL) != ffi.NULL:
            lib.prism_registry_release(self._ptr)
            self._ptr = ffi.NULL

    __del__ = close

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()
