# SPDX-License-Identifier: MPL-2.0

"""An mdBook preprocessor that renders the Python and Godot API references.

The references are written where each language keeps its documentation: in
the Python binding's docstrings, and in the Godot extension's class
reference XML. This preprocessor renders them into the manual when it is
built, replacing these directives in a chapter:

``{{#python-api NAME...}}``
    The named objects of the ``prism`` package, such as ``prism.Context``
    or ``prism.common.PrismError``.

``{{#python-exceptions}}``
    Every subclass of ``PrismError`` in ``prism.common``.

``{{#godot-class NAME}}``
    The class described by ``gdextension/doc_classes/NAME.xml``.

The Python binding is read without being imported, so the native library
need not be built. A directive that cannot be resolved fails the build.
"""

from __future__ import annotations

import builtins
import json
import posixpath
import re
import sys
import textwrap
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING

import griffe

if TYPE_CHECKING:
    from collections.abc import Callable, Iterator

ROOT = Path(__file__).resolve().parent.parent
PYTHON_SOURCE = ROOT / "bindings" / "py" / "prism"
GODOT_DOCS = ROOT / "gdextension" / "doc_classes"
GODOT_CLASS_URL = "https://docs.godotengine.org/en/stable/classes/class_{}.html"
PYTHON_LIBRARY_URL = "https://docs.python.org/3/library/{}.html"
PYTHON_EXCEPTION_URL = "https://docs.python.org/3/library/exceptions.html#{}"
# Parameter lists longer than this are written one parameter per line.
WRAP_PARAMETERS_AT = 60

_DIRECTIVE = re.compile(
    r"\{\{#(python-api|python-exceptions|godot-class)\s*([^}]*)\}\}"
)


class DirectiveError(Exception):
    """A directive could not be rendered."""


@dataclass
class _Index:
    # Maps a name, in each form a reference may use, to (chapter, anchor).
    targets: dict[str, tuple[str, str]] = field(default_factory=dict)
    # The classes whose members a bare name may refer to, as Sphinx resolves
    # names relative to the object being documented.
    scope: tuple[str, ...] = ()

    def add(self, chapter: str, anchor: str, *names: str) -> None:
        for name in names:
            self.targets.setdefault(name, (chapter, anchor))

    def link(self, name: str, text: str, chapter: str) -> str:
        candidates = (*(f"{owner}.{name}" for owner in self.scope), name)
        target = next((self.targets[c] for c in candidates if c in self.targets), None)
        if target is None:
            return _builtin_link(name, text)
        page, anchor = target
        if page == chapter:
            return f"[`{text}`](#{anchor})"
        rel = posixpath.relpath(page, posixpath.dirname(chapter))
        return f"[`{text}`]({rel}#{anchor})"


def _builtin_link(name: str, text: str) -> str:
    value = getattr(builtins, name, None)
    if isinstance(value, type) and issubclass(value, BaseException):
        return f"[`{text}`]({PYTHON_EXCEPTION_URL.format(name)})"
    return f"`{text}`"


# Python


def _load_package() -> griffe.Module:
    return griffe.load(
        "prism",
        search_paths=[str(PYTHON_SOURCE)],
        docstring_parser="google",
        allow_inspection=False,
    )


def _resolve(package: griffe.Module, name: str) -> tuple[str, griffe.Object]:
    try:
        obj = package[name.removeprefix("prism.")]
    except KeyError as exc:
        msg = f"python-api: no object named {name}"
        raise DirectiveError(msg) from exc
    return name, obj.final_target if obj.is_alias else obj


def _exceptions(package: griffe.Module) -> list[tuple[str, griffe.Object]]:
    found = []
    for member in package["common"].members.values():
        if member.is_alias or not member.is_class or member.name.startswith("_"):
            continue
        if member.name == "PrismError" or "PrismError" in map(str, member.bases):
            found.append((f"prism.common.{member.name}", member))
    return found


def _anchor(name: str) -> str:
    return "py-" + name.removeprefix("prism.").replace(".", "-")


def _is_property(obj: griffe.Object) -> bool:
    return obj.is_attribute and "property" in obj.labels


def _members(cls: griffe.Class) -> Iterator[griffe.Object]:
    for member in sorted(cls.members.values(), key=lambda m: m.lineno or 0):
        if member.is_alias or member.name.startswith("_"):
            continue
        if (member.is_function or _is_property(member)) and member.docstring:
            yield member


def _constructor(cls: griffe.Object) -> griffe.Function | None:
    init = cls.members.get("__init__")
    if init is not None and init.is_function and init.parent is cls:
        return init
    return None


def _index_python(
    index: _Index, chapter: str, objects: list[tuple[str, griffe.Object]]
) -> None:
    for name, obj in objects:
        short = name.rsplit(".", 1)[-1]
        index.add(chapter, _anchor(name), name, short, obj.path)
        if obj.is_class:
            for member in _members(obj):
                full = f"{name}.{member.name}"
                index.add(
                    chapter, _anchor(full), full, f"{short}.{member.name}", member.path
                )


_ROLE = re.compile(r":(?:class|meth|attr|func|data|exc|mod):`~?([^`]+)`")
_LITERAL = re.compile(r"``([^`]+)``")


def _inline(text: str, index: _Index, chapter: str) -> str:
    def role(match: re.Match[str]) -> str:
        target = match.group(1)
        if match.group(0).startswith(":mod:") and target in sys.stdlib_module_names:
            return f"[`{target}`]({PYTHON_LIBRARY_URL.format(target)})"
        return index.link(target.removesuffix("()"), target, chapter)

    return _LITERAL.sub(r"`\1`", _ROLE.sub(role, text))


def _markdown(text: str, index: _Index, chapter: str) -> str:
    # Converts reStructuredText literal blocks to fenced code, and inline
    # markup to Markdown.
    out: list[str] = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        i += 1
        if not line.endswith("::"):
            out.append(_inline(line, index, chapter))
            continue
        lead = line[:-2].rstrip()
        if lead:
            out.append(_inline(lead + ":", index, chapter))
        block: list[str] = []
        while i < len(lines) and (not lines[i].strip() or lines[i][:1].isspace()):
            block.append(lines[i])
            i += 1
        code = textwrap.dedent("\n".join(block)).strip()
        out.extend(["", "```python", code, "```", ""])
    return "\n".join(out).strip()


def _parameter(param: griffe.Parameter) -> str:
    kind = param.kind.value if param.kind else ""
    text = {"variadic positional": "*", "variadic keyword": "**"}.get(kind, "")
    text += param.name
    if param.annotation is not None:
        text += f": {param.annotation}"
    if param.default is not None:
        text += (
            f" = {param.default}"
            if param.annotation is not None
            else f"={param.default}"
        )
    return text


def _parameters(function: griffe.Function, *, drop_self: bool) -> str:
    parts: list[str] = []
    star_written = False
    for param in function.parameters:
        if drop_self and param.name in {"self", "cls"}:
            continue
        kind = param.kind.value if param.kind else ""
        if kind == "keyword-only" and not star_written:
            parts.append("*")
        star_written = star_written or kind in {"keyword-only", "variadic positional"}
        parts.append(_parameter(param))
    joined = ", ".join(parts)
    if len(joined) > WRAP_PARAMETERS_AT:
        return "\n    " + ",\n    ".join(parts) + ",\n"
    return joined


def _signature(name: str, obj: griffe.Object) -> str:
    short = name.rsplit(".", 1)[-1]
    if obj.is_class:
        init = _constructor(obj)
        if init is not None:
            return f"class {short}({_parameters(init, drop_self=True)})"
        bases = ", ".join(map(str, obj.bases))
        return f"class {short}({bases})" if bases else f"class {short}"
    if _is_property(obj):
        access = "" if "writable" in obj.labels else "  # read-only"
        return f"{short}: {obj.annotation}{access}"
    if obj.is_function:
        decorator = next(
            (f"@{d}\n" for d in ("staticmethod", "classmethod") if d in obj.labels), ""
        )
        returns = f" -> {obj.returns}" if obj.returns is not None else ""
        drop = obj.parent is not None and obj.parent.is_class
        return f"{decorator}def {short}({_parameters(obj, drop_self=drop)}){returns}"
    annotation = f": {obj.annotation}" if obj.annotation is not None else ""
    value = f" = {obj.value}" if obj.value is not None else ""
    return f"{short}{annotation}{value}"


_ORDER = (
    "Parameters",
    "Return Value",
    "Exceptions",
    "Attributes",
    "Remarks",
    "Example",
)


@dataclass
class _Sections:
    index: _Index
    chapter: str
    summary: str = ""
    # Set while adding a constructor, whose summary only restates the class's.
    drop_summary: bool = False
    body: dict[str, list[str]] = field(
        default_factory=lambda: {title: [] for title in _ORDER}
    )

    def md(self, text: str) -> str:
        return _markdown(text, self.index, self.chapter)

    def text(self, section: griffe.DocstringSection) -> None:
        text = self.md(section.value)
        if self.drop_summary:
            self.drop_summary = False
            _, _, text = text.partition("\n\n")
        elif not self.summary:
            self.summary, _, text = text.partition("\n\n")
        if text:
            self.body["Remarks"].extend([text, ""])

    def admonition(self, section: griffe.DocstringSection) -> None:
        title = section.title or "Note"
        body = self.md(section.value.description)
        if title.lower().startswith("example"):
            self.body["Example"].extend([body, ""])
        else:
            self.body["Remarks"].extend([f"##### {title}", "", body, ""])

    def named(self, title: str, section: griffe.DocstringSection) -> None:
        for item in section.value:
            self.body[title].extend(
                [f"`{item.name}`", "", self.md(item.description), ""]
            )

    def returns(self, section: griffe.DocstringSection) -> None:
        for item in section.value:
            self.body["Return Value"].extend([self.md(item.description), ""])

    def raises(self, section: griffe.DocstringSection) -> None:
        for item in section.value:
            name = str(item.annotation)
            link = self.index.link(name, name.rsplit(".", 1)[-1], self.chapter)
            self.body["Exceptions"].extend([link, "", self.md(item.description), ""])

    def examples(self, section: griffe.DocstringSection) -> None:
        for kind, text in section.value:
            if kind.value == "examples":
                self.body["Example"].extend(["```python", text, "```", ""])
            else:
                self.body["Example"].extend([self.md(text), ""])

    def add(self, obj: griffe.Object, *, drop_summary: bool = False) -> None:
        if obj.docstring is None:
            return
        self.drop_summary = drop_summary
        handlers: dict[str, Callable[[griffe.DocstringSection], None]] = {
            "text": self.text,
            "admonition": self.admonition,
            "parameters": lambda s: self.named("Parameters", s),
            "attributes": lambda s: self.named("Attributes", s),
            "returns": self.returns,
            "raises": self.raises,
            "examples": self.examples,
        }
        for section in obj.docstring.parse("google"):
            handler = handlers.get(section.kind.value)
            if handler is not None:
                handler(section)


def _render_python_object(
    name: str, obj: griffe.Object, index: _Index, chapter: str, display: str
) -> list[str]:
    owner = obj if obj.is_class else obj.parent
    is_class = owner is not None and owner.is_class
    index.scope = (owner.name, owner.path) if is_class else ()
    sections = _Sections(index, chapter)
    sections.add(obj)
    if obj.is_class and (init := _constructor(obj)) is not None:
        # The constructor documents the class's arguments.
        sections.add(init, drop_summary=True)
    out = [f'<a id="{_anchor(name)}"></a>', "", f"### `{display}`", ""]
    if sections.summary:
        out.extend([sections.summary, ""])
    out.extend(["#### Syntax", "", "```python", _signature(name, obj), "```", ""])
    for title in _ORDER:
        if sections.body[title]:
            out.extend([f"#### {title}", "", *sections.body[title]])
    return out


def _render_python(
    objects: list[tuple[str, griffe.Object]], index: _Index, chapter: str
) -> str:
    out: list[str] = []
    for name, obj in objects:
        out.extend(_render_python_object(name, obj, index, chapter, name))
        if obj.is_class:
            short = name.rsplit(".", 1)[-1]
            for member in _members(obj):
                full = f"{name}.{member.name}"
                display = f"{short}.{member.name}"
                out.extend(_render_python_object(full, member, index, chapter, display))
    return "\n".join(out).strip() + "\n"


# Godot


def _godot_classes() -> dict[str, ET.Element]:
    # The class reference is part of this repository, not untrusted input.
    return {
        path.stem: ET.parse(path).getroot()  # noqa: S314
        for path in sorted(GODOT_DOCS.glob("*.xml"))
    }


def _godot_anchor(kind: str, name: str) -> str:
    return f"{kind}-{name}".lower().replace("_", "-")


_GODOT_MEMBERS = (
    ("method", "methods/method"),
    ("property", "members/member"),
    ("signal", "signals/signal"),
    ("constant", "constants/constant"),
)


def _index_godot(index: _Index, chapter: str, cls: ET.Element) -> None:
    name = cls.get("name", "")
    index.add(chapter, _godot_anchor("class", name), f"godot:{name}")
    for kind, tag in _GODOT_MEMBERS:
        for element in cls.iterfind(tag):
            member = element.get("name", "")
            key = f"godot:{name}.{kind}:{member}"
            index.add(chapter, _godot_anchor(kind, member), key)
            enum = element.get("enum")
            if kind == "constant" and enum:
                key = f"godot:{name}.enum:{enum}"
                index.add(chapter, _godot_anchor("enum", enum), key)


_BBCODE_REF = re.compile(r"\[(method|member|signal|constant|enum|param) ([^\]]+)\]")
_BBCODE_CLASS = re.compile(r"\[([A-Z][A-Za-z0-9_]*)\]")
_BBCODE_CODE = re.compile(r"\[code\](.*?)\[/code\]", re.DOTALL)
_BBCODE_URL = re.compile(r"\[url=([^\]]+)\](.*?)\[/url\]", re.DOTALL)
_BBCODE_TAGS = {
    "[b]": "**",
    "[/b]": "**",
    "[i]": "*",
    "[/i]": "*",
    "[kbd]": "`",
    "[/kbd]": "`",
    "[br]": "<br>",
}
_GODOT_BUILTIN_TYPES = {"void", "int", "float", "bool"}


def _godot_type_link(type_name: str, index: _Index, chapter: str, own: str) -> str:
    base = type_name.removesuffix("[]")
    if f"godot:{base}" in index.targets:
        return index.link(f"godot:{base}", type_name, chapter)
    if base in _GODOT_BUILTIN_TYPES or base == own:
        return f"`{type_name}`"
    suffix = type_name[len(base) :]
    return f"[`{base}`]({GODOT_CLASS_URL.format(base.lower())}){suffix}"


def _godot_type(
    element: ET.Element | None, index: _Index, chapter: str, own: str
) -> str:
    if element is None:
        return "`void`"
    enum = element.get("enum")
    if enum:
        cls, _, name = enum.rpartition(".")
        return index.link(f"godot:{cls or own}.enum:{name}", enum, chapter)
    return _godot_type_link(element.get("type", "void"), index, chapter, own)


def _bbcode(text: str, index: _Index, chapter: str, own: str) -> str:
    # Converted references are set aside until the end, so that the Markdown
    # they produce is not itself mistaken for BBCode.
    stash: list[str] = []

    def keep(value: str) -> str:
        stash.append(value)
        return f"\x00{len(stash) - 1}\x00"

    def ref(match: re.Match[str]) -> str:
        kind, target = match.groups()
        if kind == "param":
            return keep(f"`{target}`")
        cls, _, member = target.rpartition(".")
        cls = cls or own
        shown = (target if cls != own else member) + ("()" if kind == "method" else "")
        kind = "property" if kind == "member" else kind
        return keep(index.link(f"godot:{cls}.{kind}:{member}", shown, chapter))

    text = _BBCODE_CODE.sub(lambda m: keep(f"`{m.group(1)}`"), text)
    text = _BBCODE_URL.sub(lambda m: keep(f"[{m.group(2)}]({m.group(1)})"), text)
    text = _BBCODE_REF.sub(ref, text)
    text = _BBCODE_CLASS.sub(
        lambda m: keep(_godot_type_link(m.group(1), index, chapter, "")), text
    )
    for tag, markdown in _BBCODE_TAGS.items():
        text = text.replace(tag, markdown)
    return re.sub(r"\x00(\d+)\x00", lambda m: stash[int(m.group(1))], text)


def _godot_text(
    element: ET.Element | None, index: _Index, chapter: str, own: str
) -> str:
    if element is None or not (element.text or "").strip():
        return ""
    text = textwrap.dedent(element.text.strip("\n")).strip()
    out: list[str] = []
    parts = re.split(r"\[codeblock\](.*?)\[/codeblock\]", text, flags=re.DOTALL)
    for i, part in enumerate(parts):
        if i % 2:
            code = textwrap.dedent(part.strip("\n").replace("\t", "    ")).strip("\n")
            out.extend(["```gdscript", code, "```", ""])
            continue
        for line in part.splitlines():
            if line.strip():
                out.extend([_bbcode(line.strip(), index, chapter, own), ""])
    return "\n".join(out).strip()


def _godot_params(element: ET.Element) -> str:
    parts = []
    for param in element.iterfind("param"):
        text = f"{param.get('name')}: {param.get('enum') or param.get('type')}"
        if param.get("default") is not None:
            text += f" = {param.get('default')}"
        parts.append(text)
    return ", ".join(parts)


def _godot_signature(method: ET.Element) -> str:
    ret = method.find("return")
    returns = "void" if ret is None else ret.get("enum") or ret.get("type", "void")
    qualifiers = f" {method.get('qualifiers')}" if method.get("qualifiers") else ""
    return f"{returns} {method.get('name')}({_godot_params(method)}){qualifiers}"


@dataclass
class _GodotPage:
    cls: ET.Element
    index: _Index
    chapter: str
    out: list[str] = field(default_factory=list)

    @property
    def own(self) -> str:
        return self.cls.get("name", "")

    def text(self, element: ET.Element | None) -> str:
        return _godot_text(element, self.index, self.chapter, self.own)

    def link(self, kind: str, name: str) -> str:
        return self.index.link(f"godot:{self.own}.{kind}:{name}", name, self.chapter)

    def anchor(self, kind: str, name: str) -> None:
        self.out.extend([f'<a id="{_godot_anchor(kind, name)}"></a>', ""])

    def header(self) -> None:
        self.anchor("class", self.own)
        inherits = self.cls.get("inherits")
        if inherits:
            link = _godot_type_link(inherits, self.index, self.chapter, self.own)
            self.out.extend([f"**Inherits:** {link}", ""])
        self.out.extend([self.text(self.cls.find("brief_description")), ""])
        description = self.text(self.cls.find("description"))
        self.out.extend(["### Description", "", description, ""])
        links = list(self.cls.iterfind("tutorials/link"))
        if links:
            self.out.extend(["### Tutorials", ""])
            for link in links:
                self.out.append(f"- [{link.get('title')}]({(link.text or '').strip()})")
            self.out.append("")

    def summaries(self) -> None:
        members = list(self.cls.iterfind("members/member"))
        if members:
            self.out.extend(["### Properties", "", "| Type | Name |", "| --- | --- |"])
            for member in members:
                kind = _godot_type(member, self.index, self.chapter, self.own)
                self.out.append(
                    f"| {kind} | {self.link('property', member.get('name', ''))} |"
                )
            self.out.append("")
        methods = list(self.cls.iterfind("methods/method"))
        if methods:
            self.out.extend(
                ["### Methods", "", "| Returns | Method |", "| --- | --- |"]
            )
            for method in methods:
                ret = _godot_type(
                    method.find("return"), self.index, self.chapter, self.own
                )
                link = self.link("method", method.get("name", ""))
                qualifiers = (
                    f" {method.get('qualifiers')}" if method.get("qualifiers") else ""
                )
                params = _godot_params(method)
                self.out.append(f"| {ret} | {link}`({params}){qualifiers}` |")
            self.out.append("")

    def signals(self) -> None:
        signals = list(self.cls.iterfind("signals/signal"))
        if signals:
            self.out.extend(["### Signals", ""])
        for signal in signals:
            name = signal.get("name", "")
            self.anchor("signal", name)
            self.out.extend([f"`{name}({_godot_params(signal)})`", ""])
            self.out.extend([self.text(signal.find("description")), ""])

    def constants(self) -> None:
        groups: dict[str, list[ET.Element]] = {}
        for constant in self.cls.iterfind("constants/constant"):
            groups.setdefault(constant.get("enum") or "", []).append(constant)
        for enum, constants in groups.items():
            if enum:
                bitfield = constants[0].get("is_bitfield") == "true"
                self.anchor("enum", enum)
                self.out.extend([f"### {'flags' if bitfield else 'enum'} `{enum}`", ""])
            else:
                self.out.extend(["### Constants", ""])
            for constant in constants:
                name = constant.get("name", "")
                self.anchor("constant", name)
                self.out.extend([f"`{name} = {constant.get('value')}`", ""])
                self.out.extend([self.text(constant), ""])

    def descriptions(self) -> None:
        members = list(self.cls.iterfind("members/member"))
        if members:
            self.out.extend(["### Property Descriptions", ""])
        for member in members:
            name = member.get("name", "")
            access = [
                f"{role} `{member.get(role)}{call}`"
                for role, call in (("setter", "(value)"), ("getter", "()"))
                if member.get(role)
            ]
            self.anchor("property", name)
            self.out.extend(
                [f"#### `{member.get('enum') or member.get('type')} {name}`", ""]
            )
            if access:
                self.out.extend([f"*{', '.join(access).capitalize()}*", ""])
            self.out.extend([self.text(member), ""])
        methods = list(self.cls.iterfind("methods/method"))
        if methods:
            self.out.extend(["### Method Descriptions", ""])
        for method in methods:
            self.anchor("method", method.get("name", ""))
            self.out.extend([f"#### `{_godot_signature(method)}`", ""])
            self.out.extend([self.text(method.find("description")), ""])

    def render(self) -> str:
        # The order of Godot's own class reference.
        self.header()
        self.summaries()
        self.signals()
        self.constants()
        self.descriptions()
        return re.sub(r"\n{3,}", "\n\n", "\n".join(self.out)).strip() + "\n"


# mdBook


def _chapters(items: list[object]) -> Iterator[dict[str, object]]:
    for item in items:
        if isinstance(item, dict) and "Chapter" in item:
            chapter = item["Chapter"]
            yield chapter
            yield from _chapters(chapter.get("sub_items", []))


def _book_items(book: dict[str, object]) -> list[object]:
    # mdBook 0.5 renamed the book's "sections" to "items".
    return book.get("items", book.get("sections", []))


def _plan(
    book: dict[str, object], package: griffe.Module, classes: dict[str, ET.Element]
) -> tuple[_Index, dict[tuple[str, str], tuple[str, object]]]:
    index = _Index()
    resolved: dict[tuple[str, str], tuple[str, object]] = {}
    for chapter in _chapters(_book_items(book)):
        path = str(chapter.get("path") or "")
        for match in _DIRECTIVE.finditer(str(chapter.get("content", ""))):
            kind, args = match.group(1), match.group(2).split()
            if kind == "godot-class":
                if len(args) != 1 or args[0] not in classes:
                    msg = f"godot-class: no class reference for {' '.join(args)}"
                    raise DirectiveError(msg)
                objects: object = classes[args[0]]
                _index_godot(index, path, objects)
            else:
                if kind == "python-api":
                    objects = [_resolve(package, name) for name in args]
                else:
                    objects = _exceptions(package)
                _index_python(index, path, objects)
            resolved[(path, match.group(0))] = (kind, objects)
    return index, resolved


def _render(book: dict[str, object]) -> dict[str, object]:
    index, resolved = _plan(book, _load_package(), _godot_classes())
    for chapter in _chapters(_book_items(book)):
        path = str(chapter.get("path") or "")

        def replace(match: re.Match[str], path: str = path) -> str:
            kind, objects = resolved[(path, match.group(0))]
            if kind == "godot-class":
                return _GodotPage(objects, index, path).render()
            return _render_python(objects, index, path)

        chapter["content"] = _DIRECTIVE.sub(replace, str(chapter.get("content", "")))
    return book


def main() -> int:
    """Run as an mdBook preprocessor.

    Returns:
        The process exit status.
    """
    if len(sys.argv) > 1 and sys.argv[1] == "supports":
        return 0
    _context, book = json.load(sys.stdin)
    try:
        rendered = _render(book)
    except DirectiveError as exc:
        sys.stderr.write(f"mdbook_reference: {exc}\n")
        return 1
    json.dump(rendered, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
