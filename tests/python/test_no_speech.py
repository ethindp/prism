# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

import pytest
from prism import Backend, BackendId, Context
from prism.common import (
    PrismAlreadyPausedError,
    PrismNotPausedError,
    PrismNotSpeakingError,
    PrismRangeError,
)

LONG_TEXT = "a" * 100


@pytest.fixture
def backend() -> Backend:
    ctx = Context()
    if not ctx.exists(BackendId.NO_SPEECH):
        pytest.skip("the No Speech backend is not in this build")
    return ctx.create(BackendId.NO_SPEECH)


def _audio_frames(backend: Backend, text: str) -> tuple[int, set[float], set[int]]:
    frames = 0
    values: set[float] = set()
    formats: set[int] = set()

    def collect(pcm: list[float], channels: int, rate: int) -> None:
        nonlocal frames
        frames += len(pcm) // channels
        values.update(pcm)
        formats.add(channels * 1_000_000 + rate)

    backend.speak_to_memory(text, collect)
    return frames, values, formats


def test_no_speech_reports_every_operation_and_no_classification(
    backend: Backend,
) -> None:
    features = backend.features
    assert features.is_supported_at_runtime
    assert features.requires_explicit_selection
    assert not features.is_screen_reader
    assert not features.is_speech_engine
    operations = [
        name for name in features.__dataclass_fields__ if name.startswith("supports_")
    ]
    for name in operations:
        assert getattr(features, name) == (not name.endswith("_ssml")), name


def test_no_speech_is_never_selected_automatically() -> None:
    ctx = Context()
    try:
        selected = ctx.create_best()
    except ValueError:
        return
    assert selected.name != "No Speech"


def test_no_speech_pause_resume_and_stop_follow_the_contract(
    backend: Backend,
) -> None:
    assert not backend.speaking
    with pytest.raises(PrismNotSpeakingError):
        backend.pause()
    with pytest.raises(PrismNotPausedError):
        backend.resume()
    backend.speak(LONG_TEXT, interrupt=True)
    assert backend.speaking
    backend.pause()
    assert not backend.speaking
    with pytest.raises(PrismAlreadyPausedError):
        backend.pause()
    backend.resume()
    assert backend.speaking
    backend.stop()
    assert not backend.speaking
    with pytest.raises(PrismNotSpeakingError):
        backend.pause()


def test_no_speech_interrupting_speech_clears_a_pause(backend: Backend) -> None:
    backend.speak(LONG_TEXT, interrupt=True)
    backend.pause()
    backend.speak(LONG_TEXT, interrupt=True)
    assert backend.speaking
    with pytest.raises(PrismNotPausedError):
        backend.resume()


def test_no_speech_queued_speech_while_paused_waits_for_resume(
    backend: Backend,
) -> None:
    backend.speak(LONG_TEXT, interrupt=True)
    backend.pause()
    backend.speak(LONG_TEXT, interrupt=False)
    assert not backend.speaking
    backend.resume()
    assert backend.speaking


@pytest.mark.parametrize(
    ("rate", "text", "frames"),
    [
        (0.5, "abcdefghij", 13230),
        (0.5, "Ñandú", 6615),
        (1.0, "abcdefghij", 4410),
        (0.0, "abcdefghij", 26460),
    ],
)
def test_no_speech_renders_silence_whose_length_follows_the_rate(
    backend: Backend, rate: float, text: str, frames: int
) -> None:
    backend.rate = rate
    rendered, values, formats = _audio_frames(backend, text)
    assert rendered == frames
    assert values == {0.0}
    assert formats == {backend.channels * 1_000_000 + backend.sample_rate}


def test_no_speech_reports_a_fixed_format_and_one_voice(backend: Backend) -> None:
    assert (backend.channels, backend.sample_rate, backend.bit_depth) == (
        1,
        22050,
        16,
    )
    backend.refresh_voices()
    assert backend.voices_count == 1
    assert backend.voice == 0
    assert backend.get_voice_name(0) == "No Speech"
    assert backend.get_voice_language(0) == "und"
    backend.voice = 0
    with pytest.raises(PrismRangeError):
        backend.voice = 1
    with pytest.raises(PrismRangeError):
        backend.get_voice_name(1)


def test_no_speech_retains_voice_parameters(backend: Backend) -> None:
    assert (backend.volume, backend.rate, backend.pitch) == (0.5, 0.5, 0.5)
    backend.volume = 0.25
    backend.rate = 1.0
    backend.pitch = 0.0
    assert (backend.volume, backend.rate, backend.pitch) == (0.25, 1.0, 0.0)
    backend.braille("text")
    backend.output("text", interrupt=False)
    assert backend.speaking
