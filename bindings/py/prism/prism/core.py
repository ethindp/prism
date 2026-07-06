from __future__ import annotations

import logging
import sys
from collections.abc import Callable
from typing import TYPE_CHECKING

from ._dispatch import _Dispatcher
from .common import (
    BackendFeatures,
    BackendId,
    PrismInvalidParamError,
    _check_error,
)
from .lib import ffi, lib

if TYPE_CHECKING:
    from ._dispatch import AvailabilityCallback
    from .custom import Registry

AudioCallback = Callable[[list[float], int, int], None]


class Backend:
    _raw: ffi.CData = None

    def __init__(self, raw_ptr: ffi.CData) -> None:
        if raw_ptr == ffi.NULL:
            raise RuntimeError("Backend raw pointer MUST NOT be NULL!")
        self._raw = raw_ptr
        res = lib.prism_backend_initialize(self._raw)
        if res not in {lib.PRISM_OK, lib.PRISM_ERROR_ALREADY_INITIALIZED}:
            _check_error(res)

    def __del__(self) -> None:
        if sys.is_finalizing():
            return
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

    def speak_to_memory(self, text: str, on_audio_data: AudioCallback) -> None:
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                "Text MUST NOT be empty",
            )

        @ffi.callback("void(void *, const float *, size_t, size_t, size_t)")
        def audio_callback_shim(
            _userdata: ffi.CData,
            samples_ptr: ffi.CData,
            count: int,
            channels: int,
            rate: int,
        ) -> None:
            pcm_data = ffi.unpack(samples_ptr, count)
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
    _ctx: ffi.CData = None
    _registry: Registry | None
    _dispatcher: _Dispatcher | None
    _availability_cb: ffi.CData | None
    _on_availability: AvailabilityCallback | None

    def __init__(
        self,
        registry: Registry | None = None,
        *,
        on_availability: AvailabilityCallback | None = None,
        poll_interval_ms: int = 0,
        debounce_samples: int = 0,
        backoff_max_ms: int = 0,
        auto_power_manage: bool = True,
    ) -> None:
        self._ctx = None
        self._registry = registry
        self._dispatcher = None
        self._availability_cb = None
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
        return lib.prism_registry_count(self._ctx)

    def id_of(self, index_or_name: int | str) -> BackendId:
        if isinstance(index_or_name, int):
            res = lib.prism_registry_id_at(self._ctx, index_or_name)
        elif isinstance(index_or_name, str):
            res = lib.prism_registry_id(self._ctx, index_or_name.encode("utf-8"))
        else:
            raise TypeError("Expected int or string")
        if res == 0:
            raise KeyError(f"No backend matching {index_or_name!r}")
        return BackendId(res)

    def name_of(self, backend_id: BackendId) -> str:
        c_ptr = lib.prism_registry_name(self._ctx, backend_id)
        if c_ptr == ffi.NULL:
            raise KeyError("Backend ID not found")
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

    def pause_availability_polling(self) -> None:
        lib.prism_availability_poll_pause(self._ctx)

    def resume_availability_polling(self) -> None:
        lib.prism_availability_poll_resume(self._ctx)

    @staticmethod
    def auto_power_supported() -> bool:
        return bool(lib.prism_availability_auto_power_supported())
