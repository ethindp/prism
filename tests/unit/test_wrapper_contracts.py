import pytest
from prism import Backend, PrismError


def test_empty_text_rejected_by_wrapper(any_backend: Backend):
    with pytest.raises(PrismError):
        any_backend.speak("")
    with pytest.raises(PrismError):
        any_backend.braille("")
    with pytest.raises(PrismError):
        any_backend.output("", interrupt=False)
