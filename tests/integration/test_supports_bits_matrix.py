import sys
from dataclasses import fields

import pytest
from prism import Backend, BackendId, PrismNotImplementedError


def _skip_uia_in_matrix(backend_id: BackendId):
    return sys.platform == "win32" and backend_id == BackendId.UIA


def _call_for_support_flag(backend: Backend, flag_name: str):
    text = "Hello from Prism tests"
    v = 0.5
    if flag_name == "supports_speak":
        return lambda: backend.speak(text, interrupt=True)
    if flag_name == "supports_speak_to_memory":

        def run():
            def cb(pcm, channels, rate):
                pass

            backend.speak_to_memory(text, cb)

        return run
    if flag_name == "supports_braille":
        return lambda: backend.braille(text)
    if flag_name == "supports_output":
        return lambda: backend.output(text, interrupt=True)
    if flag_name == "supports_is_speaking":
        return lambda: backend.speaking
    if flag_name == "supports_stop":
        return lambda: backend.stop()
    if flag_name == "supports_pause":
        return lambda: backend.pause()
    if flag_name == "supports_resume":
        return lambda: backend.resume()
    if flag_name == "supports_set_volume":
        return lambda: setattr(backend, "volume", v)
    if flag_name == "supports_get_volume":
        return lambda: backend.volume
    if flag_name == "supports_set_rate":
        return lambda: setattr(backend, "rate", v)
    if flag_name == "supports_get_rate":
        return lambda: backend.rate
    if flag_name == "supports_set_pitch":
        return lambda: setattr(backend, "pitch", v)
    if flag_name == "supports_get_pitch":
        return lambda: backend.pitch
    if flag_name == "supports_refresh_voices":
        return lambda: backend.refresh_voices()
    if flag_name == "supports_count_voices":
        return lambda: backend.voices_count
    if flag_name == "supports_get_voice_name":
        return lambda: backend.get_voice_name(0)
    if flag_name == "supports_get_voice_language":
        return lambda: backend.get_voice_language(0)
    if flag_name == "supports_get_voice":
        return lambda: backend.voice
    if flag_name == "supports_set_voice":
        return lambda: setattr(backend, "voice", 0)
    if flag_name == "supports_get_channels":
        return lambda: backend.channels
    if flag_name == "supports_get_sample_rate":
        return lambda: backend.sample_rate
    if flag_name == "supports_get_bit_depth":
        return lambda: backend.bit_depth
    if flag_name == "supports_speak_ssml":
        if not hasattr(backend, "speak_ssml"):
            pytest.xfail(
                "BackendFeatures advertises supports_speak_ssml but wrapper has no speak_ssml() yet",
            )
        return lambda: backend.speak_ssml("<speak>Hello</speak>", interrupt=True)
    if flag_name == "supports_speak_to_memory_ssml":
        if not hasattr(backend, "speak_to_memory_ssml"):
            pytest.xfail(
                "BackendFeatures advertises supports_speak_to_memory_ssml but wrapper has no speak_to_memory_ssml() yet",
            )

        def run():
            def cb(pcm, channels, rate):
                pass

            backend.speak_to_memory_ssml("<speak>Hello</speak>", cb)

        return run
    raise AssertionError(f"No call mapping for flag: {flag_name}")


def _assert_support_gate(supported: bool, call):
    if not supported:
        with pytest.raises(PrismNotImplementedError):
            call()
        return
    try:
        call()
    except PrismNotImplementedError:
        pytest.fail("supports_* bit true but call raised PrismNotImplementedError")
    except Exception as e:
        if e.__class__.__name__.startswith("Prism"):
            return
        raise


@pytest.mark.integration
def test_every_supports_bit_is_enforced(ctx, backend_id):
    if _skip_uia_in_matrix(backend_id):
        pytest.skip("UIA requires HWND; covered by dedicated WinUI harness test")
    try:
        backend = ctx.create(backend_id)
    except Exception as e:
        pytest.skip(
            f"Backend {backend_id} could not be created/initialized here: {type(e).__name__}: {e}",
        )
    feats = backend.features
    for f in fields(type(feats)):
        if not f.name.startswith("supports_"):
            continue
        supported = getattr(feats, f.name)
        call = _call_for_support_flag(backend, f.name)
        _assert_support_gate(supported, call)
