#!/usr/bin/env python3
"""Generate the maintained firmware function-definition inventory."""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOTS = (
    ROOT / "firmware/application/src",
    ROOT / "firmware/common",
    ROOT / "firmware/bootloader/src",
    ROOT / "firmware/tests",
)
OUTPUT = ROOT / "docs/deep analysis/02-function-signatures.md"
CONTROL_WORDS = {"if", "for", "while", "switch", "return", "sizeof"}


@dataclass(frozen=True)
class Function:
    path: str
    line: int
    name: str
    scope: str
    signature: str
    description: str


def mask_non_code(source: str, path: Path) -> str:
    chars = list(source)
    state = "code"
    index = 0
    while index < len(chars):
        char = chars[index]
        following = chars[index + 1] if index + 1 < len(chars) else ""
        if state == "code":
            if char == "/" and following == "*":
                chars[index] = chars[index + 1] = " "
                state = "block_comment"
                index += 2
                continue
            if char == "/" and following == "/":
                chars[index] = chars[index + 1] = " "
                state = "line_comment"
                index += 2
                continue
            if char == '"':
                chars[index] = " "
                state = "string"
            elif char == "'":
                chars[index] = " "
                state = "char"
        elif state == "block_comment":
            if char == "*" and following == "/":
                chars[index] = chars[index + 1] = " "
                state = "code"
                index += 2
                continue
            if char != "\n":
                chars[index] = " "
        elif state == "line_comment":
            if char == "\n":
                state = "code"
            else:
                chars[index] = " "
        elif state in {"string", "char"}:
            quote = '"' if state == "string" else "'"
            if char == "\\":
                chars[index] = " "
                if index + 1 < len(chars) and chars[index + 1] != "\n":
                    chars[index + 1] = " "
                    index += 2
                    continue
            elif char == quote:
                chars[index] = " "
                state = "code"
            elif char != "\n":
                chars[index] = " "
        index += 1

    if state in {"block_comment", "string", "char"}:
        raise ValueError(f"{path}: unterminated {state.replace('_', ' ')}")

    masked = "".join(chars)
    lines = masked.splitlines(keepends=True)
    in_directive = False
    for line_index, line in enumerate(lines):
        directive = in_directive or line.lstrip().startswith("#")
        if directive:
            newline = "\n" if line.endswith("\n") else ""
            lines[line_index] = " " * (len(line) - len(newline)) + newline
            in_directive = line.rstrip("\n").rstrip().endswith("\\")
        else:
            in_directive = False
    return "".join(lines)


def matching_open_parenthesis(text: str, close_index: int) -> int | None:
    depth = 0
    for index in range(close_index, -1, -1):
        if text[index] == ")":
            depth += 1
        elif text[index] == "(":
            depth -= 1
            if depth == 0:
                return index
    return None


def has_top_level_equal(text: str) -> bool:
    parentheses = brackets = 0
    for char in text:
        if char == "(":
            parentheses += 1
        elif char == ")":
            parentheses -= 1
        elif char == "[":
            brackets += 1
        elif char == "]":
            brackets -= 1
        elif char == "=" and parentheses == 0 and brackets == 0:
            return True
    return False


def describe(name: str) -> str:
    readable = name.lstrip("_").replace("_", " ")
    prefixes = (
        ("cmd_processor_", "Procesa el comando"),
        ("cmd_before_", "Prepara el comando"),
        ("cmd_after_", "Finaliza el comando"),
        ("test_", "Ejecuta la prueba"),
        ("init", "Inicializa"),
        ("uninit", "Libera"),
        ("start", "Inicia"),
        ("stop", "Detiene"),
        ("process", "Procesa"),
        ("handle", "Gestiona"),
        ("validate", "Valida"),
        ("parse", "Interpreta"),
        ("load", "Carga"),
        ("save", "Guarda"),
        ("read", "Lee"),
        ("write", "Escribe"),
        ("send", "Envia"),
        ("copy", "Copia"),
        ("get", "Obtiene"),
        ("set", "Configura"),
        ("is_", "Comprueba"),
        ("has_", "Comprueba"),
        ("on_", "Atiende"),
    )
    if name == "main":
        return "Punto de entrada del ejecutable."
    for prefix, verb in prefixes:
        if name.startswith(prefix):
            subject = name[len(prefix):].replace("_", " ") or readable
            return f"{verb} {subject}."
    return f"Implementa {readable}."


def candidate_function(masked_header: str) -> tuple[str, str] | None:
    header = masked_header.strip()
    if not header.endswith(")") or has_top_level_equal(header):
        return None
    close_index = len(header) - 1
    open_index = matching_open_parenthesis(header, close_index)
    if open_index is None:
        return None
    prefix = header[:open_index].rstrip()
    match = re.search(r"([A-Za-z_]\w*)$", prefix)
    if match is None:
        return None
    name = match.group(1)
    if name in CONTROL_WORDS:
        return None
    declaration_prefix = prefix[:match.start()]
    if re.search(r"\btypedef\b", declaration_prefix):
        return None
    if not declaration_prefix.strip():
        return None
    signature = re.sub(r"\s+", " ", header).strip()
    return name, signature


def functions_in_file(path: Path) -> list[Function]:
    source = path.read_text(encoding="utf-8")
    masked = mask_non_code(source, path)
    functions: list[Function] = []
    brace_depth = 0
    segment_start = 0

    for index, char in enumerate(masked):
        if char == "{":
            if brace_depth == 0:
                segment = masked[segment_start:index]
                parsed = candidate_function(segment)
                if parsed is not None:
                    name, signature = parsed
                    leading = len(segment) - len(segment.lstrip())
                    offset = segment_start + leading
                    line = source.count("\n", 0, offset) + 1
                    relative = path.relative_to(ROOT).as_posix()
                    scope = "static" if re.search(r"\bstatic\b", signature) else "exportada"
                    if relative.startswith("firmware/tests/"):
                        scope = "test/stub"
                    functions.append(Function(
                        path=relative,
                        line=line,
                        name=name,
                        scope=scope,
                        signature=signature,
                        description=describe(name),
                    ))
            brace_depth += 1
        elif char == "}":
            brace_depth -= 1
            if brace_depth < 0:
                raise ValueError(f"{path}: unmatched closing brace")
            if brace_depth == 0:
                segment_start = index + 1
        elif char == ";" and brace_depth == 0:
            segment_start = index + 1

    if brace_depth != 0:
        raise ValueError(f"{path}: unbalanced braces after preprocessing mask")
    return functions


def escape_markdown(value: str) -> str:
    return value.replace("|", "\\|").replace("`", "\\`")


def main() -> int:
    files = sorted(
        path
        for root in SOURCE_ROOTS
        for path in root.rglob("*")
        if path.suffix in {".c", ".h"}
        and not any(
            part.startswith("build")
            for part in path.relative_to(root).parts[:-1]
        )
    )
    if not files:
        print("No firmware source files found; run from a complete checkout.", file=sys.stderr)
        return 1

    try:
        functions = sorted(
            (function for path in files for function in functions_in_file(path)),
            key=lambda function: (function.path, function.line, function.name),
        )
    except (OSError, UnicodeError, ValueError) as error:
        print(f"Signature inventory aborted: {error}", file=sys.stderr)
        return 1

    identities = {(item.path, item.line, item.name) for item in functions}
    if len(identities) != len(functions):
        print("Signature inventory aborted: duplicate function identity.", file=sys.stderr)
        return 1

    by_file: dict[str, list[Function]] = {}
    for function in functions:
        by_file.setdefault(function.path, []).append(function)

    lines = [
        "# Indice de firmas mantenidas del firmware",
        "",
        "Generado por `firmware/tools/generate_deep_analysis_signatures.py`.",
        "Incluye definiciones de funciones C mantenidas en application, common,",
        "bootloader y tests/stubs. Excluye Nordic SDK, prototypes sin cuerpo, macros,",
        "artefactos de build y herramientas host de `software/`.",
        "",
        f"- Archivos C/H inspeccionados: {len(files)}",
        f"- Archivos con definiciones: {len(by_file)}",
        f"- Funciones definidas: {len(functions)}",
        "- Regenerar: `python3 firmware/tools/generate_deep_analysis_signatures.py`",
        "",
    ]
    for path, entries in by_file.items():
        lines.extend((
            f"## `{path}`",
            "",
            "| Linea | Ambito | Funcion | Firma | Descripcion |",
            "|---:|---|---|---|---|",
        ))
        for entry in entries:
            lines.append(
                f"| {entry.line} | {entry.scope} | `{entry.name}` | "
                f"`{escape_markdown(entry.signature)}` | {entry.description} |"
            )
        lines.append("")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {len(functions)} functions from {len(files)} files to {OUTPUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
