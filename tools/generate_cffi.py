# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import argparse
from io import StringIO
from pathlib import Path
from typing import TYPE_CHECKING, Protocol

from cffi import FFI
from pcpp import Action, OutputDirective, Preprocessor

if TYPE_CHECKING:
    from collections.abc import Sequence

MODULE_NAME = "prism._prism_cffi"
_PRAGMA = "pragma"


class _Token(Protocol):
    value: str


class _CdefPreprocessor(Preprocessor):
    def __init__(self) -> None:
        super().__init__()
        self.line_directive = None

    def on_include_not_found(
        self,
        is_malformed: bool,
        is_system_include: bool,
        curdir: str,
        includepath: str,
    ) -> None:
        if is_system_include:
            raise OutputDirective(Action.IgnoreAndRemove)
        super().on_include_not_found(
            is_malformed,
            is_system_include,
            curdir,
            includepath,
        )

    def on_directive_unknown(
        self,
        directive: _Token,
        toks: list[_Token],
        ifpassthru: bool,
        precedingtoks: list[_Token],
    ) -> None:
        if directive.value == _PRAGMA:
            raise OutputDirective(Action.IgnoreAndRemove)
        super().on_directive_unknown(directive, toks, ifpassthru, precedingtoks)


def preprocess_header(header: Path) -> str:
    preprocessor = _CdefPreprocessor()
    preprocessor.parse(header.read_text(encoding="utf-8"), str(header))
    sink = StringIO()
    preprocessor.write(sink)
    return sink.getvalue()


def generate(header: Path, output: Path) -> None:
    ffi = FFI()
    ffi.cdef(preprocess_header(header))
    ffi.set_source(MODULE_NAME, f'#include "{header.name}"')
    ffi.emit_c_code(str(output))


def _parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate Prism's cffi stub.")
    parser.add_argument("header", type=Path, help="path to prism.h")
    parser.add_argument("output", type=Path, help="destination C file")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> None:
    args = _parse_args(argv)
    generate(args.header, args.output)


if __name__ == "__main__":
    main()
