# SPDX-License-Identifier: MPL-2.0

import atexit
import logging
import threading
from enum import IntEnum
from typing import Final

from .lib import ffi, lib


class LogLevel(IntEnum):
    TRACE = lib.PRISM_LOG_LEVEL_TRACE
    DEBUG = lib.PRISM_LOG_LEVEL_DEBUG
    INFO = lib.PRISM_LOG_LEVEL_INFO
    WARN = lib.PRISM_LOG_LEVEL_WARN
    ERROR = lib.PRISM_LOG_LEVEL_ERROR
    NONE = lib.PRISM_LOG_LEVEL_NONE


_TRACE_PY: Final[int] = 5
logging.addLevelName(_TRACE_PY, "TRACE")

_TO_PY: Final[dict[int, int]] = {
    int(LogLevel.TRACE): _TRACE_PY,
    int(LogLevel.DEBUG): logging.DEBUG,
    int(LogLevel.INFO): logging.INFO,
    int(LogLevel.WARN): logging.WARNING,
    int(LogLevel.ERROR): logging.ERROR,
}

_lock: Final[threading.Lock] = threading.Lock()
_installed: bool = False
_cb: ffi.CData | None = None
_prev: ffi.CData | None = None


def install_logging(logger_name: str = "prism") -> None:
    global _installed, _cb, _prev
    with _lock:
        if _installed:
            return

        @ffi.callback("void(void*, PrismLogLevel, const char*, const char*)")
        def forward(
            _userdata: ffi.CData,
            level: int,
            source: ffi.CData,
            message: ffi.CData,
        ) -> None:
            src: str = (
                ffi.string(source).decode("utf-8", "replace")
                if source != ffi.NULL
                else "prism"
            )
            msg: str = (
                ffi.string(message).decode("utf-8", "replace")
                if message != ffi.NULL
                else ""
            )
            logging.getLogger(f"{logger_name}.{src}").log(
                _TO_PY.get(level, logging.INFO), msg
            )

        handler: ffi.CData = ffi.new(
            "PrismLogHandler*", {"fn": forward, "userdata": ffi.NULL}
        )
        _prev = lib.prism_set_log_handler(handler[0])
        _cb = forward
        _installed = True
        atexit.register(_shutdown)


def set_level(level: LogLevel) -> LogLevel:
    return LogLevel(lib.prism_set_log_level(int(level)))


def uninstall() -> None:
    global _installed, _cb, _prev
    with _lock:
        if not _installed:
            return
        if _prev is not None:
            lib.prism_set_log_handler(_prev)
        _cb = None
        _prev = None
        _installed = False


def _shutdown() -> None:
    lib.prism_log_flush()
    lib.prism_log_shutdown()
