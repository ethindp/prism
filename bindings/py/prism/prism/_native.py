from __future__ import annotations

import os
import sys
from pathlib import Path

_NATIVE_SUBDIR = "_native"


def _find_native_dir() -> Path | None:
    local = Path(__file__).parent / _NATIVE_SUBDIR
    if local.is_dir():
        return local
    relative = Path("prism") / _NATIVE_SUBDIR
    for entry in sys.path:
        candidate = Path(entry) / relative
        if candidate.is_dir():
            return candidate
    return None


_native_dir = _find_native_dir()
if _native_dir is not None:
    _package = sys.modules.get(__package__ or "prism")
    if _package is not None:
        _package.__path__.append(str(_native_dir))
    if sys.platform == "win32":
        os.add_dll_directory(str(_native_dir))
