# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

import os
import shutil
import subprocess
import sys

import pytest
from prism import BackendId, Context

pytestmark = pytest.mark.integration


def _commands() -> tuple[list[str], list[str] | None] | None:
    if sys.platform == "win32":
        # PowerShell otherwise writes in the console's OEM code page.
        return [
            "powershell",
            "-NoProfile",
            "-Command",
            "[Console]::OutputEncoding = [Text.Encoding]::UTF8; Get-Clipboard -Raw",
        ], None
    if sys.platform == "darwin":
        return ["pbpaste"], ["pbcopy"]
    if os.environ.get("WAYLAND_DISPLAY") and shutil.which("wl-paste"):
        return ["wl-paste", "--no-newline"], ["wl-copy"]
    if os.environ.get("DISPLAY") and shutil.which("xclip"):
        return (
            ["xclip", "-selection", "clipboard", "-out"],
            ["xclip", "-selection", "clipboard", "-in"],
        )
    return None


def _read(command: list[str]) -> str:
    read = subprocess.run(command, capture_output=True, timeout=10, check=True)  # noqa: S603
    return read.stdout.decode().removesuffix("\r\n")


def test_clipboard_places_text_on_the_clipboard() -> None:
    ctx = Context()
    if not ctx.exists(BackendId.CLIPBOARD):
        pytest.skip("the Clipboard backend is not in this build")
    commands = _commands()
    if commands is None:
        pytest.skip("no means of reading the clipboard back")
    read, write = commands
    backend = ctx.create(BackendId.CLIPBOARD)
    features = backend.features
    assert features.requires_explicit_selection
    assert not features.is_screen_reader
    assert not features.is_speech_engine
    if not features.is_supported_at_runtime:
        pytest.skip("no clipboard is reachable")
    large = "".join(f"line {n} ñandú 日本語\n" for n in range(40000))
    for text in ("Prism clipboard ñandú 日本語", "second\nline", large):
        backend.speak(text, interrupt=True)
        assert _read(read) == text
    if write is not None:
        subprocess.run(write, input=b"taken", timeout=10, check=True)  # noqa: S603
        assert _read(read) == "taken"
        backend.speak("regained", interrupt=True)
        assert _read(read) == "regained"
