# SPDX-License-Identifier: MPL-2.0
# Note: this code is (NOT) production-quality! If you use it in your applications, you have been warned!

"""Speak through a custom backend that runs a Piper neural voice in Python.

Requires the ``piper-tts``, ``numpy`` and ``sounddevice`` packages, and a
Piper voice model in ONNX format.
"""

import argparse
import sys
from collections.abc import Iterator
from pathlib import Path

import numpy as np
import sounddevice
from piper import PiperVoice
from prism import Context, CustomBackend, RegistryBuilder

_VOICE_CACHE: dict[str, object] = {}


def _load_voice(path: str) -> object:
    if path not in _VOICE_CACHE:
        _VOICE_CACHE[path] = PiperVoice.load(path)
    return _VOICE_CACHE[path]


def _synthesize(voice: object, text: str) -> Iterator[tuple[np.ndarray, int]]:
    if hasattr(voice, "synthesize"):
        for chunk in voice.synthesize(text):
            arr = getattr(chunk, "audio_float_array", None)
            if arr is None:
                arr = (
                    np.frombuffer(chunk.audio_int16_bytes, dtype=np.int16).astype(
                        np.float32
                    )
                    / 32768.0
                )
            yield arr, chunk.sample_rate
    else:
        rate = voice.config.sample_rate
        for raw in voice.synthesize_stream_raw(text):
            arr = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
            yield arr, rate


class PiperBackend(CustomBackend):
    """A speech engine backend that synthesizes with Piper and plays the result.

    Attributes:
        model_path: The path of the Piper voice model, shared by every
            instance. It is set before the backend is registered.
    """

    model_path: str = ""

    def __init__(self) -> None:
        """Create an instance with no voice loaded."""
        self.voice: object = None

    def initialize(self) -> None:
        """Load the voice model, or reuse it if another instance loaded it."""
        self.voice = _load_voice(self.model_path)

    def is_supported(self) -> bool:
        """Report whether the voice model exists.

        Returns:
            Whether ``model_path`` names an existing file.
        """
        return bool(self.model_path) and Path(self.model_path).exists()

    def speak(self, text: str, interrupt: bool) -> None:
        """Synthesize text and play it, blocking until playback finishes.

        Args:
            text: The text to speak.
            interrupt: Whether to stop audio that is already playing.
        """
        if interrupt:
            sounddevice.stop()
        chunks = []
        rate = 22050
        for arr, chunks_rate in _synthesize(self.voice, text):
            chunks.append(arr)
            rate = chunks_rate
        if not chunks:
            return
        sounddevice.play(np.concatenate(chunks), samplerate=rate, blocking=True)

    def stop(self) -> None:
        """Stop audio that is playing."""
        sounddevice.stop()


def main() -> int:
    """Register the Piper backend and speak through it.

    Returns:
        The process exit status.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True, help="path to a Piper .onnx voice")
    parser.add_argument(
        "--text",
        default="Hello. This speech is produced by a Prism custom backend "
        "running a neural text to speech model in Python.",
    )
    args = parser.parse_args()
    PiperBackend.model_path = args.model
    with RegistryBuilder() as builder:
        backend_id = builder.add_backend("Piper", PiperBackend, priority=1000)
        registry = builder.freeze()
    with registry:
        ctx = Context(registry=registry)
        backend = ctx.create(backend_id)
        print(f"Speaking through {backend.name}...")
        backend.speak(args.text, interrupt=True)
        print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
