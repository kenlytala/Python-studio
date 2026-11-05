"""Parité des grammaires C/Python des règles PyLint ; processus CPU sans fenêtre."""

import argparse
import json
from pathlib import Path
import random
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("--python-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.python_root))
    from editor.lint_rules import normalize_disabled

    cases = [
        "",
        " ",
        ",;\t\r\n",
        "w0611; C0114\nUNUSED-IMPORT, w0611",
        "all",
        "ALL",
        "W",
        "é",
        "a\x00b",
        "a_b",
        "a.b",
        "-a",
        "a-",
        "a--b",
        "ab-2",
        "--init-hook=print(42)",
        "a" * 95,
        "a" * 96,
        " " * 4090 + "W0611",
        " " * 4091 + "W0611",
        "W0611," * 100,
        ",".join(f"W{i:04}" for i in range(64)),
        ",".join(f"W{i:04}" for i in range(65)),
    ]
    randomizer = random.Random(1170)
    tokens = [
        "W0611",
        "c0114",
        "E0602",
        "unused-import",
        "missing-module-docstring",
        "too-few-public-methods",
        "all",
        "--bad",
        "invalid_name",
        "é",
        "a--b",
        "a.b",
        "R0903",
        "F0001",
    ]
    for _ in range(2000):
        values = randomizer.choices(tokens, k=randomizer.randrange(0, 85))
        cases.append(randomizer.choice([",", ";", " ", "\n", "\t", ",;"]).join(values))
    for _ in range(1000):
        cases.append(
            "".join(
                randomizer.choices(
                    "ABCWRIefwcri0123456789-_,; \t\r\n\v\x00é",
                    k=randomizer.randrange(0, 160),
                )
            )
        )
    expected = []
    for value in cases:
        try:
            expected.append(dict(ok=True, value=normalize_disabled(value)))
        except (ValueError, UnicodeError):
            expected.append(dict(ok=False))
    payload = "".join(value.encode("utf-8").hex() + "\n" for value in cases)
    output = subprocess.check_output(
        [str(args.native), "--parse-lines"], input=payload, text=True
    )
    actual = [json.loads(line) for line in output.splitlines()]
    assert len(actual) == len(expected), (len(actual), len(expected))
    for value, before, after in zip(cases, expected, actual):
        assert before == after, (repr(value), before, after)
    result = dict(
        status="passed",
        cases=len(cases),
        accepted=sum(item["ok"] for item in expected),
        independent_native_parser=True,
        max_bytes=4095,
        max_rules=64,
        max_name=95,
        no_window_or_project_execution=True,
    )
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(args.output.read_text(), end="")


if __name__ == "__main__":
    main()
