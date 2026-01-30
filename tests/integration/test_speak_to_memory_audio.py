import threading

import pytest
from hypothesis import given, settings

from tests.strategies import safe_text


@pytest.mark.integration
@settings(max_examples=40, deadline=None)
@given(safe_text)
def test_speak_to_memory_audio_invariants(any_backend, text):
    b = any_backend
    if not b.features.supports_speak_to_memory:
        pytest.skip("speak_to_memory not supported")
    seen = {"calls": 0, "threads": set(), "channels": None, "rate": None, "total": 0}

    def cb(pcm, channels, rate):
        seen["calls"] += 1
        seen["threads"].add(threading.get_ident())
        seen["total"] += len(pcm)
        assert isinstance(channels, int) and channels > 0
        assert isinstance(rate, int) and rate > 0
        assert len(pcm) % channels == 0
        for x in pcm:
            assert -1.0001 <= x <= 1.0001
        if seen["channels"] is None:
            seen["channels"] = channels
            seen["rate"] = rate
        else:
            assert seen["channels"] == channels
            assert seen["rate"] == rate

    b.speak_to_memory(text, cb)
    assert seen["calls"] >= 1
    assert seen["total"] >= 1
