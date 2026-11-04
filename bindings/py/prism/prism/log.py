# SPDX-License-Identifier: MPL-2.0

"""Prism's diagnostic log.

Prism discards its log messages unless a handler is installed or the
``PRISM_LOG`` environment variable is set. :func:`install_logging` forwards
messages to the :mod:`logging` module, and :func:`set_level` sets the least
severe level delivered.

These functions are re-exported by the ``prism`` package as
``prism.install_logging``, ``prism.uninstall_logging``,
``prism.set_log_level``, ``prism.write_log``, and ``prism.flush_log``.
"""

import atexit
import logging
import threading
from enum import IntEnum
from typing import Final

from ._prism_cffi import ffi, lib
from .common import PrismInvalidParamError


class LogLevel(IntEnum):
    """The severity of a log message.

    The members correspond to the ``PrismLogLevel`` values. Each is named after
    its value without the ``PRISM_LOG_LEVEL_`` prefix and has the same value:
    ``LogLevel.WARN`` is ``PRISM_LOG_LEVEL_WARN``.

    Members are ordered from least to most severe. A message is delivered when
    its level is at least the threshold set by :func:`set_level`. ``NONE`` is
    above every severity, so as a threshold it delivers nothing. It MUST NOT be
    the level of a message.
    """

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
_shutdown_registered: bool = False
_cb: ffi.CData | None = None
_prev: ffi.CData | None = None


def install_logging(logger_name: str = "prism") -> None:
    """Forwards Prism's log messages to the :mod:`logging` module.

    This function wraps ``prism_set_log_handler``. A message from the source
    ``source`` goes to the logger named ``logger_name.source``. ``TRACE`` maps to
    a level named ``TRACE`` with the value 5; the other levels map to the
    corresponding :mod:`logging` levels.

    The handler replaces the one in effect, including the handler installed for
    ``PRISM_LOG``; :func:`uninstall` restores it. If forwarding is already
    installed, the function does nothing. Messages are delivered only once the
    threshold has been lowered with :func:`set_level`.

    Messages are delivered on Prism's logging thread. Handlers of the receiving
    loggers run on that thread, MUST NOT call the functions of this module, and
    SHOULD return quickly; messages arriving while the thread is busy MAY be
    dropped.

    The first call registers an :mod:`atexit` function that delivers pending
    messages and stops the logging thread when the interpreter exits.

    Args:
        logger_name: The prefix of the receiving loggers' names.

    Example:
        Deliver informational messages and above::

            import logging
            import prism

            logging.basicConfig(level=logging.INFO)
            prism.install_logging()
            prism.set_log_level(prism.LogLevel.INFO)
    """
    global _installed, _cb, _prev, _shutdown_registered
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
        if not _shutdown_registered:
            atexit.register(_shutdown)
            _shutdown_registered = True


def set_level(level: LogLevel) -> LogLevel:
    """Sets the least severe level of message delivered.

    This function wraps ``prism_set_log_level``. The initial threshold is
    ``LogLevel.NONE``. Messages below the threshold are discarded where they are
    produced. The threshold is independent of the handler. The function MAY be
    called from any thread at any time.

    Args:
        level: The new threshold.

    Returns:
        The previous threshold.
    """
    return LogLevel(lib.prism_set_log_level(int(level)))


def uninstall() -> None:
    """Stops forwarding log messages and restores the previous handler.

    A message already being delivered MAY still reach :mod:`logging`. The
    function does nothing if forwarding is not installed.
    """
    global _installed, _cb, _prev
    with _lock:
        if not _installed:
            return
        if _prev is not None:
            lib.prism_set_log_handler(_prev)
        _cb = None
        _prev = None
        _installed = False


def write(level: LogLevel, source: str, message: str) -> None:
    """Writes a message to Prism's log.

    This function wraps ``prism_log``. The message is subject to the same
    threshold and handler as Prism's own messages. A message below the threshold
    is discarded; otherwise it is queued, and the function returns without
    waiting for delivery. If the queue is full, the message is dropped, and the
    number of dropped messages is later reported as a warning from the source
    ``"prism"``.

    The function MAY be called from any thread, but MUST NOT be called by a
    handler of a logger that receives Prism's messages.

    Args:
        level: The level of the message. It MUST NOT be ``LogLevel.NONE``.
        source: The component the message comes from.
        message: The text of the message.

    Raises:
        PrismInvalidParamError: If ``source`` or ``message`` is empty or contains
            a NUL character.
    """
    for name, text in (("Source", source), ("Message", message)):
        if len(text) == 0:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                f"{name} MUST NOT be empty",
            )
        if "\x00" in text:
            raise PrismInvalidParamError(
                lib.PRISM_ERROR_INVALID_PARAM,
                f"{name} MUST NOT contain embedded NULLs",
            )
    lib.prism_log(int(level), source.encode("utf-8"), message.encode("utf-8"))


def flush() -> None:
    """Waits until queued log messages have been delivered.

    This function wraps ``prism_log_flush``. It returns once every message queued
    before the call has been passed to the handler, or immediately if no handler
    is installed. Dropped messages are not recovered.

    The function MUST NOT be called by a handler of a logger that receives
    Prism's messages.
    """
    lib.prism_log_flush()


def _shutdown() -> None:
    flush()
    lib.prism_log_shutdown()
