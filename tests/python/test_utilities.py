# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

import gc
import logging

import prism
import pytest
from prism import Context, CustomBackend, RegistryBuilder
from prism.common import PrismInvalidParamError


class _SilentBackend(CustomBackend):
    def speak(self, text: str, interrupt: bool) -> None:
        pass


def test_version_matches_string_and_dunder() -> None:
    major, minor, patch = (int(part) for part in prism.version_string().split("."))
    assert prism.version() == (major << 16) | (minor << 8) | patch
    assert prism.__version__ == prism.version_string()


def test_get_returns_cached_backend_only_while_alive() -> None:
    with RegistryBuilder() as builder:
        backend_id = builder.add_backend("Silent", _SilentBackend)
        registry = builder.freeze()
    with registry:
        ctx = Context(registry=registry)
        assert ctx.get(backend_id) is None
        acquired = ctx.acquire(backend_id)
        cached = ctx.get(backend_id)
        assert cached is not None
        assert cached.name == acquired.name
        del acquired, cached
        gc.collect()
        assert ctx.get(backend_id) is None


def test_write_log_is_delivered_through_logging(
    caplog: pytest.LogCaptureFixture,
) -> None:
    prism.install_logging()
    previous = prism.set_log_level(prism.LogLevel.INFO)
    try:
        with caplog.at_level(logging.INFO, logger="prism"):
            prism.write_log(prism.LogLevel.WARN, "test", "hello")
            prism.flush_log()
    finally:
        prism.set_log_level(previous)
        prism.uninstall_logging()
    assert ("prism.test", logging.WARNING, "hello") in caplog.record_tuples


@pytest.mark.parametrize(
    ("source", "message"),
    [("", "text"), ("source", ""), ("so\x00urce", "text"), ("source", "te\x00xt")],
)
def test_write_log_rejects_unrepresentable_text(source: str, message: str) -> None:
    with pytest.raises(PrismInvalidParamError):
        prism.write_log(prism.LogLevel.INFO, source, message)
