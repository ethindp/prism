from dataclasses import fields

import pytest
from hypothesis import given, settings
from hypothesis import strategies as st
from prism import BackendFeatures, BackendId, Context


@given(st.integers(min_value=0, max_value=(1 << 64) - 1))
@settings(max_examples=1000, deadline=None)
def test_backend_features_from_bits(bits: int):
    bf = BackendFeatures.from_bits(bits)
    for f in fields(BackendFeatures):
        pos = f.metadata["bit"]
        assert getattr(bf, f.name) == bool(bits & (1 << pos))


def test_registry_count_stable(ctx: Context):
    assert ctx.backends_count == ctx.backends_count


def test_registry_exists_invalid_false(ctx: Context):
    assert ctx.exists(BackendId.INVALID) is False


def test_registry_priority_invalid_minus_one(ctx: Context):
    assert ctx.priority_of(BackendId.INVALID) == -1


def test_registry_name_invalid_raises(ctx: Context):
    with pytest.raises(ValueError):
        ctx.name_of(BackendId.INVALID)
