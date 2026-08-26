#!/usr/bin/env python3
"""Generate one discoverable GTest case for every retained upstream check.

The retained source functions still provide useful ordering, shared setup,
and teardown.  Each function runs once and its results are cached; this
generator exposes every recorder boundary as a normal, filterable GTest while
keeping the check bodies in their original source files.
"""

from __future__ import annotations

import argparse
import ast
import json
import pathlib
import re
import sys


def mask_comments(source: str) -> str:
    chars = list(source)
    index = 0
    state = "code"
    while index < len(source):
        current = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if current == "/" and following == "/":
                chars[index] = chars[index + 1] = " "
                index += 2
                state = "line-comment"
                continue
            if current == "/" and following == "*":
                chars[index] = chars[index + 1] = " "
                index += 2
                state = "block-comment"
                continue
            if current == '"':
                state = "string"
            elif current == "'":
                state = "character"
        elif state == "line-comment":
            if current == "\n":
                state = "code"
            else:
                chars[index] = " "
        elif state == "block-comment":
            if current == "*" and following == "/":
                chars[index] = chars[index + 1] = " "
                index += 2
                state = "code"
                continue
            if current != "\n":
                chars[index] = " "
        elif state in ("string", "character"):
            if current == "\\":
                index += 2
                continue
            if (state == "string" and current == '"') or (
                state == "character" and current == "'"
            ):
                state = "code"
        index += 1
    return "".join(chars)


def find_call_arguments(source: str, token: str) -> list[tuple[int, str]]:
    masked = mask_comments(source)
    calls: list[tuple[int, str]] = []
    cursor = 0
    while True:
        start = masked.find(token, cursor)
        if start < 0:
            break
        opening = masked.find("(", start + len(token))
        if opening < 0:
            raise ValueError(f"unterminated call after offset {start}")
        depth = 1
        index = opening + 1
        state = "code"
        while index < len(masked) and depth:
            char = masked[index]
            if state == "code":
                if char == '"':
                    state = "string"
                elif char == "'":
                    state = "character"
                elif char == "(":
                    depth += 1
                elif char == ")":
                    depth -= 1
            else:
                if char == "\\":
                    index += 1
                elif (state == "string" and char == '"') or (
                    state == "character" and char == "'"
                ):
                    state = "code"
            index += 1
        if depth:
            raise ValueError(f"unterminated argument list after offset {start}")
        calls.append((start, source[opening + 1 : index - 1]))
        cursor = index
    return calls


STRING_LITERAL = re.compile(r'"(?:\\.|[^"\\])*"')


def evaluate_adjacent_strings(expression: str) -> str | None:
    literals = STRING_LITERAL.findall(expression)
    if not literals:
        return None
    remainder = STRING_LITERAL.sub("", expression)
    remainder = remainder.replace("\\", "").strip()
    if remainder:
        return None
    return "".join(ast.literal_eval(item) for item in literals)


def sanitize_identifier(value: str) -> str:
    result = re.sub(r"[^A-Za-z0-9]+", "_", value).strip("_")
    if not result:
        result = "Unnamed"
    if result[0].isdigit():
        result = "N_" + result
    return result


def suite_name(path: pathlib.Path, root: pathlib.Path) -> str:
    relative = path.relative_to(root)
    parts = relative.parts
    category = parts[1] if len(parts) > 2 and parts[0] == "tests" else parts[0]
    stem = path.stem.removeprefix("test_")
    stem = stem.removeprefix(category + "_")

    def pascal(value: str) -> str:
        return "".join(
            token[:1].upper() + token[1:]
            for token in re.split(r"[^A-Za-z0-9]+", value)
            if token
        )

    return "Upstream" + pascal(category) + pascal(stem)


def extract_cases(path: pathlib.Path) -> tuple[str, list[tuple[int, str]]]:
    source = path.read_text(encoding="utf-8")
    function_match = re.search(
        r"\b(?:static\s+)?int\s+(obol_run_upstream_[A-Za-z0-9_]+)\s*\(",
        source,
    )
    if not function_match:
        raise ValueError(f"{path}: upstream entry function not found")

    cases: list[tuple[int, str]] = []
    for position, expression in find_call_arguments(source, "runner.startTest"):
        name = evaluate_adjacent_strings(expression)
        if name is not None:
            cases.append((position, name))

    macro_patterns = (
        ("TEST_SF_INITIALIZED", " class initialized"),
        ("TEST_MF_INITIALIZED", " initialized"),
    )
    masked = mask_comments(source)
    for macro, suffix in macro_patterns:
        pattern = re.compile(rf"\b{macro}\s*\(\s*(\"(?:\\.|[^\"\\])*\")")
        for match in pattern.finditer(masked):
            # The macro definition uses an identifier, so only invocations
            # with a string literal match here.
            cases.append((match.start(), ast.literal_eval(match.group(1)) + suffix))

    cases.sort(key=lambda item: item[0])
    if not cases:
        raise ValueError(f"{path}: no upstream checks found")
    return function_match.group(1), cases


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def generate(output: pathlib.Path, root: pathlib.Path,
             paths: list[pathlib.Path]) -> int:
    entries: list[tuple[pathlib.Path, str, list[tuple[int, str]]]] = []
    total = 0
    for path in paths:
        function, cases = extract_cases(path)
        entries.append((path, function, cases))
        total += len(cases)

    lines = [
        "// Generated by generate_upstream_gtests.py. Do not edit.",
        '#include "framework/upstream_test_registration.h"',
        "",
    ]
    for _, function, _ in entries:
        lines.append(f"extern int {function}();")
    lines.extend(
        [
            "",
            "namespace {",
            "",
            "using UpstreamEntry = int (*)();",
            "",
            "class GeneratedUpstreamTest : public ::testing::Test {",
            "public:",
            "  GeneratedUpstreamTest(UpstreamEntry entry, int index,",
            "                        const char * expected)",
            "    : entry_(entry), index_(index), expected_(expected) {}",
            "",
            "private:",
            "  void TestBody() override {",
            "    ObolTest::runUpstreamCase(entry_, index_, expected_);",
            "  }",
            "  UpstreamEntry entry_;",
            "  int index_;",
            "  const char * expected_;",
            "};",
            "",
            "struct GeneratedUpstreamFactory {",
            "  UpstreamEntry entry;",
            "  int index;",
            "  const char * expected;",
            "  GeneratedUpstreamTest * operator()() const {",
            "    return new GeneratedUpstreamTest(entry, index, expected);",
            "  }",
            "};",
            "",
            "void registerUpstreamTest(const char * suite, const char * test,",
            "                          const char * file, int line,",
            "                          UpstreamEntry entry, int index,",
            "                          const char * expected) {",
            "  ::testing::RegisterTest(suite, test, nullptr, nullptr, file, line,",
            "                          GeneratedUpstreamFactory{entry, index, expected});",
            "}",
            "",
            "const bool upstream_cases_registered = [] {",
        ]
    )

    for path, function, cases in entries:
        relative = path.relative_to(root).as_posix()
        suite = suite_name(path, root)
        used: set[str] = set()
        for index, (position, name) in enumerate(cases):
            line = path.read_text(encoding="utf-8").count("\n", 0, position) + 1
            test = f"Check{index:03d}_{sanitize_identifier(name)}"
            if test in used:
                test += f"_{index}"
            used.add(test)
            lines.append(
                "  registerUpstreamTest("
                f"{cpp_string(suite)}, {cpp_string(test)}, "
                f"{cpp_string(relative)}, {line}, &{function}, {index}, "
                f"{cpp_string(name)});"
            )
    lines.extend(["  return true;", "}();", "", "} // namespace", ""])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8")
    return total


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--root", type=pathlib.Path, required=True)
    parser.add_argument("sources", nargs="*", type=pathlib.Path)
    args = parser.parse_args()

    paths = [path.resolve() for path in args.sources]
    if not paths:
        paths = sorted(
            path.resolve()
            for path in (args.root / "tests").rglob("*.cpp")
            if "UpstreamCheckRecorder" in path.read_text(encoding="utf-8")
        )
    total = generate(args.output.resolve(), args.root.resolve(), paths)
    print(f"Generated {total} native GTest registrations from {len(paths)} sources")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
