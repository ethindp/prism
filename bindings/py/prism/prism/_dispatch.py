# SPDX-License-Identifier: MPL-2.0
import asyncio
import inspect
import logging
import threading
from collections.abc import Awaitable, Callable
from typing import TYPE_CHECKING, Final

from .common import BackendId

if TYPE_CHECKING:
    import concurrent.futures


AvailabilityCallback = Callable[[BackendId, str, bool], None | Awaitable[None]]

_log: Final[logging.Logger] = logging.getLogger("prism.dispatch")


class _Dispatcher:
    __slots__ = (
        "_closed",
        "_loop",
        "_pending",
        "_ready",
        "_running",
        "_tasks",
        "_thread",
    )

    _loop: asyncio.AbstractEventLoop
    _thread: threading.Thread
    _ready: threading.Event
    _running: set[BackendId]
    _pending: dict[BackendId, Awaitable[None]]
    _tasks: set[asyncio.Task[None]]
    _closed: bool

    def __init__(self) -> None:
        self._loop = asyncio.new_event_loop()
        self._thread = threading.Thread(
            target=self._run,
            name="prism-availability-dispatch",
            daemon=True,
        )
        self._ready = threading.Event()
        self._running = set()
        self._pending = {}
        self._tasks = set()
        self._closed = False
        self._thread.start()
        self._ready.wait()

    def _run(self) -> None:
        asyncio.set_event_loop(self._loop)
        self._loop.call_soon(self._ready.set)
        self._loop.run_forever()

    def submit(
        self,
        cb: AvailabilityCallback,
        backend: BackendId,
        name: str,
        available: bool,
    ) -> None:
        if self._closed:
            return
        self._loop.call_soon_threadsafe(self._on_event, cb, backend, name, available)

    def _on_event(
        self,
        cb: AvailabilityCallback,
        backend: BackendId,
        name: str,
        available: bool,
    ) -> None:
        try:
            result: None | Awaitable[None] = cb(backend, name, available)
        except Exception:  # noqa: BLE001
            _log.exception("availability callback raised (sync)")
            return
        if not inspect.isawaitable(result):
            return
        coro: Awaitable[None] = result
        if backend in self._running:
            superseded: Awaitable[None] | None = self._pending.get(backend)
            if superseded is not None:
                superseded.close()
            self._pending[backend] = coro
            return
        self._running.add(backend)
        self._spawn(backend, coro)

    def _spawn(self, backend: BackendId, coro: Awaitable[None]) -> None:
        task: asyncio.Task[None] = self._loop.create_task(
            self._run_backend(backend, coro)
        )
        self._tasks.add(task)
        task.add_done_callback(self._tasks.discard)

    async def _run_backend(self, backend: BackendId, first: Awaitable[None]) -> None:
        current: Awaitable[None] | None = first
        try:
            while current is not None:
                try:
                    await current
                except asyncio.CancelledError:
                    raise
                except Exception:  # noqa: BLE001
                    _log.exception("availability callback raised (async)")
                current = self._pending.pop(backend, None)
        finally:
            self._running.discard(backend)
            leftover: Awaitable[None] | None = self._pending.pop(backend, None)
            if leftover is not None:
                leftover.close()

    def close(self, timeout: float = 5.0) -> None:
        if self._closed:
            return
        self._closed = True
        try:
            fut: concurrent.futures.Future[None] | None = None
            fut = asyncio.run_coroutine_threadsafe(self._quiesce(), self._loop)
            fut.result(timeout)
        except TimeoutError:
            _log.warning("availability dispatcher shutdown timed out; forcing")
        except RuntimeError:
            pass
        self._loop.call_soon_threadsafe(self._loop.stop)
        self._thread.join()
        self._loop.close()

    async def _quiesce(self) -> None:
        live: list[asyncio.Task[None]] = list(self._tasks)
        for task in live:
            task.cancel()
        if live:
            await asyncio.gather(*live, return_exceptions=True)
        for coro in self._pending.values():
            coro.close()
        self._pending.clear()
