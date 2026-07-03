# SPDX-License-Identifier: MPL-2.0
# Note: this code is (NOT) production-quality! If you use it in your applications, you have been warned!

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
import sounddevice
from piper import PiperVoice
from prism.core import Context
from prism.custom import CustomBackend, RegistryBuilder

MODEL_PATH: str = ""

_VOICE_CACHE: dict[str, object] = {}


def _load_voice(path: str) -> object:
    if path not in _VOICE_CACHE:
        _VOICE_CACHE[path] = PiperVoice.load(path)
    return _VOICE_CACHE[path]


def _synthesize(voice: object, text: str):
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
    def __init__(self) -> None:
        self.voice: object = None

    def initialize(self) -> None:
        self.voice = _load_voice(MODEL_PATH)

    def is_supported(self) -> bool:
        return bool(MODEL_PATH) and Path(MODEL_PATH).exists()

    def speak(self, text: str, interrupt: bool) -> None:
        if interrupt:
            sounddevice.stop()
        chunks = []
        rate = 22050
        for arr, rate in _synthesize(self.voice, text):
            chunks.append(arr)
        if not chunks:
            return
        sounddevice.play(np.concatenate(chunks), samplerate=rate, blocking=True)

    def stop(self) -> None:
        sounddevice.stop()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True, help="path to a Piper .onnx voice")
    parser.add_argument(
        "--text",
        default="Hello. This speech is produced by a Prism custom backend "
        "running a neural text to speech model in Python.",
    )
    args = parser.parse_args()
    global MODEL_PATH
    MODEL_PATH = args.model
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
