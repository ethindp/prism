# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import time

import pytest
from prism import BackendId, Context
from prism.common import PrismBackendNotAvailableError

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
    if (
        os.environ.get("WAYLAND_DISPLAY")
        and shutil.which("wl-paste")
        and _responds(["wl-paste", "--list-types"])
    ):
        return ["wl-paste", "--no-newline"], ["wl-copy"]
    if (
        os.environ.get("DISPLAY")
        and shutil.which("xclip")
        and _responds(["xclip", "-selection", "clipboard", "-out"])
    ):
        return (
            ["xclip", "-selection", "clipboard", "-out"],
            ["xclip", "-selection", "clipboard", "-in"],
        )
    return None


def _responds(command: list[str]) -> bool:
    # wl-paste waits indefinitely under a compositor it cannot read from, such
    # as WSLg's, and either tool does under a display server that has stopped
    # answering. An empty clipboard still answers, with a failure status.
    try:
        subprocess.run(command, capture_output=True, timeout=2, check=False)  # noqa: S603
    except subprocess.TimeoutExpired:
        return False
    return True


def _read(command: list[str]) -> str:
    read = subprocess.run(command, capture_output=True, timeout=10, check=True)  # noqa: S603
    return read.stdout.decode().removesuffix("\r\n")


def _read_once_placed(command: list[str], expected: str) -> str:
    # On Linux, speak returns once the serving thread has the text, which then
    # places it, so the clipboard can briefly still hold the previous text.
    deadline = time.monotonic() + 5
    while (text := _read(command)) != expected and time.monotonic() < deadline:
        time.sleep(0.05)
    return text


def test_clipboard_places_text_on_the_clipboard() -> None:
    ctx = Context()
    if not ctx.exists(BackendId.CLIPBOARD):
        pytest.skip("the Clipboard backend is not in this build")
    commands = _commands()
    if commands is None:
        pytest.skip("no means of reading the clipboard back")
    read, write = commands
    try:
        backend = ctx.create(BackendId.CLIPBOARD)
    except PrismBackendNotAvailableError:
        pytest.skip("no clipboard is reachable")
    features = backend.features
    assert features.requires_explicit_selection
    assert not features.is_screen_reader
    assert not features.is_speech_engine
    if not features.is_supported_at_runtime:
        pytest.skip("no clipboard is reachable")
    large = "".join(f"line {n} ñandú 日本語\n" for n in range(40000))
    for text in ("Prism clipboard ñandú 日本語", "second\nline", large):
        backend.speak(text, interrupt=True)
        assert _read_once_placed(read, text) == text
    if write is not None:
        subprocess.run(write, input=b"taken", timeout=10, check=True)  # noqa: S603
        assert _read_once_placed(read, "taken") == "taken"
        backend.speak("regained", interrupt=True)
        assert _read_once_placed(read, "regained") == "regained"
