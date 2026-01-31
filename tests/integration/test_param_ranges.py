import pytest
from hypothesis import given, settings
from prism import PrismError
from hypothesis import strategies as st

invalid_unit = st.one_of(
    st.floats(max_value=-1e-9, allow_nan=False, allow_infinity=False),
    st.floats(min_value=1.0 + 1e-9, allow_nan=False, allow_infinity=False),
    st.just(float("nan")),
    st.just(float("inf")),
    st.just(float("-inf")),
)


@pytest.mark.integration
@settings(max_examples=1000, deadline=None)
@given(invalid_unit)
def test_volume_invalid_raises_range_if_supported(any_backend, v):
    b = any_backend
    if not b.features.supports_set_volume:
        pytest.skip("Backend does not support set volume")
    with pytest.raises(PrismError):
        b.volume = v


@pytest.mark.integration
@settings(max_examples=1000, deadline=None)
@given(invalid_unit)
def test_rate_invalid_raises_range_if_supported(any_backend, v):
    b = any_backend
    if not b.features.supports_set_rate:
        pytest.skip("Backend does not support set rate")
    with pytest.raises(PrismError):
        b.rate = v


@pytest.mark.integration
@settings(max_examples=1000, deadline=None)
@given(invalid_unit)
def test_pitch_invalid_raises_range_if_supported(any_backend, v):
    b = any_backend
    if not b.features.supports_set_pitch:
        pytest.skip("Backend does not support set pitch")
    with pytest.raises(PrismError):
        b.pitch = v
