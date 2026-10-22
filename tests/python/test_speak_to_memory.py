# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from typing import TYPE_CHECKING, ClassVar

from prism import Context, CustomBackend, RegistryBuilder

if TYPE_CHECKING:
    from prism.core import AudioCallback


class _EchoBackend(CustomBackend):
    received: ClassVar[list[str]] = []

    def speak_to_memory(self, text: str, emit: AudioCallback) -> None:
        _EchoBackend.received.append(text)
        emit([0.0, 0.5, -0.5], 1, 16000)


def test_speak_to_memory_passes_text_and_delivers_audio() -> None:
    _EchoBackend.received.clear()
    chunks: list[tuple[list[float], int, int]] = []
    with RegistryBuilder() as builder:
        backend_id = builder.add_backend("Echo", _EchoBackend)
        registry = builder.freeze()
    with registry:
        ctx = Context(registry=registry)
        backend = ctx.create(backend_id)
        backend.speak_to_memory(
            "Ñandú über café",
            lambda pcm, channels, rate: chunks.append((pcm, channels, rate)),
        )
    assert _EchoBackend.received == ["Ñandú über café"]
    assert chunks == [([0.0, 0.5, -0.5], 1, 16000)]
