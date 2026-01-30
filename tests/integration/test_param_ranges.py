import pytest
from hypothesis import given, settings
from tests.strategies import invalid_unit
from prism import PrismRangeError


@pytest.mark.integration
@settings(max_examples=1000, deadline=None)
@given(invalid_unit)
def test_volume_invalid_raises_range_if_supported(any_backend, v):
    b = any_backend
    if not b.features.supports_set_volume:
        pytest.skip("Backend does not support set volume")
    with pytest.raises(PrismRangeError):
        b.volume = v


@pytest.mark.integration
@settings(max_examples=1000, deadline=None)
@given(invalid_unit)
def test_rate_invalid_raises_range_if_supported(any_backend, v):
    b = any_backend
    if not b.features.supports_set_rate:
        pytest.skip("Backend does not support set rate")
    with pytest.raises(PrismRangeError):
        b.rate = v


@pytest.mark.integration
@settings(max_examples=1000, deadline=None)
@given(invalid_unit)
def test_pitch_invalid_raises_range_if_supported(any_backend, v):
    b = any_backend
    if not b.features.supports_set_pitch:
        pytest.skip("Backend does not support set pitch")
    with pytest.raises(PrismRangeError):
        b.pitch = v
