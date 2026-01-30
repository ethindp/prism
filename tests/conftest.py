from prism import Context, BackendId
import os
import gc
import random
import pytest

_SEED = os.environ.get("PRISM_TEST_SEED")
_rng = random.Random(int(_SEED) if _SEED is not None else None)


def _registry_ids(ctx: Context):
    return [ctx.id_of(i) for i in range(ctx.backends_count)]


def _existing_ids(ctx: Context):
    return [
        bid
        for bid in [bid for bid in _registry_ids(ctx) if ctx.exists(bid)]
        if bid != BackendId.INVALID
    ]


@pytest.fixture(scope="module")
def ctx():
    c = Context()
    yield c
    del c
    gc.collect()


@pytest.fixture
def any_backend(ctx):
    candidates = _existing_ids(ctx)
    if not candidates:
        pytest.skip("No backends exist in registry")
    attempts = max(10, len(candidates) * 5)
    pool = candidates[:]
    for _ in range(attempts):
        bid = _rng.choice(pool)
        try:
            return ctx.create(bid)
        except Exception:
            continue

    pytest.skip("No backend could be created and initialized after randomized attempts")


def pytest_generate_tests(metafunc):
    if "backend_id" in metafunc.fixturenames:
        c = Context()
        ids = _registry_ids(c)
        del c
        gc.collect()
        metafunc.parametrize("backend_id", ids, ids=[str(i) for i in ids])
