import logging

import numpy as np
import pytest
import torch
import torchaudio

logging.getLogger("torch.hub").setLevel(logging.WARNING)
model, utils = torch.hub.load("snakers4/silero-vad", "silero_vad", trust_repo=True)
(get_speech_timestamps, _, _, _, _) = utils
model.eval()


def _has_any_speech(pcm, channels, sr, threshold=0.5):
    x = np.asarray(pcm, dtype=np.float32)
    if channels > 1:
        x = x.reshape(-1, channels).mean(axis=1)
    wav = torch.from_numpy(x)
    if sr != 16000:
        wav = torchaudio.functional.resample(wav, sr, 16000)
    if hasattr(model, "reset_states"):
        model.reset_states()
    speech = get_speech_timestamps(
        wav,
        model,
        sampling_rate=16000,
        threshold=threshold,
        min_speech_duration_ms=0,
        speech_pad_ms=0,
    )
    return bool(speech)


def _ms_to_frames(ms: float, sr: int) -> int:
    return int(max(0, round(ms * sr / 1000.0)))


def _mean_square_to_db(msq: float) -> float:
    eps = 1e-16
    return float(10.0 * np.log10(msq + eps))


def _frame_db(
    interleaved: np.ndarray, start_frame: int, frame_len: int, channels: int,
) -> float:
    total_frames = interleaved.size // channels
    end_frame = min(start_frame + frame_len, total_frames)
    if end_frame <= start_frame:
        return -160.0
    p = interleaved[start_frame * channels : end_frame * channels]
    msq = float(np.mean(p.astype(np.float64) ** 2))
    return _mean_square_to_db(msq)


def _has_no_silence_at_edges(
    samples_interleaved: np.ndarray,
    channels: int,
    sr: int,
    *,
    frame_ms: float = 10.0,
    hop_ms: float = 5.0,
    silence_db: float = -70.0,
    require_silence_ms: float = 20.0,
):
    x = np.asarray(samples_interleaved, dtype=np.float32)
    if x.size == 0 or channels <= 0 or sr <= 0 or (x.size % channels) != 0:
        return True
    total_frames = x.size // channels
    frame_len = max(1, _ms_to_frames(frame_ms, sr))
    hop = max(1, _ms_to_frames(hop_ms, sr))
    need_frames = max(1, _ms_to_frames(require_silence_ms, sr) // hop)
    n = 1 if total_frames <= frame_len else 1 + (total_frames - frame_len) // hop
    db = np.empty(n, dtype=np.float32)
    for i in range(n):
        db[i] = _frame_db(x, i * hop, frame_len, channels)
    silent = db <= silence_db
    lead = 0
    for s in silent:
        if s:
            lead += 1
        else:
            break
    trail = 0
    for s in silent[::-1]:
        if s:
            trail += 1
        else:
            break
    return (lead < need_frames) and (trail < need_frames)


@pytest.mark.integration
def test_silence_trimming_if_advertised(any_backend):
    b = any_backend
    f = b.features
    if not (
        f.supports_speak_to_memory and f.performs_silence_trimming_on_speak_to_memory
    ):
        pytest.skip("Trim bit not advertised or speak_to_memory unsupported")
    buf = []
    meta = {"channels": None, "rate": None}

    def cb(pcm, channels, rate):
        if meta["channels"] is None:
            meta["channels"] = channels
            meta["rate"] = rate
        else:
            assert meta["channels"] == channels
            assert meta["rate"] == rate
        buf.extend(pcm)

    b.speak_to_memory("Hello world. This is a trim test.", cb)
    channels = meta["channels"]
    rate = meta["rate"]
    assert channels and rate
    assert len(buf) > 0
    assert _has_any_speech(buf, channels, rate, threshold=0.5)
    assert _has_no_silence_at_edges(
        np.asarray(buf, dtype=np.float32),
        channels=channels,
        sr=rate,
        silence_db=-85.0,
        require_silence_ms=20.0,
    )
