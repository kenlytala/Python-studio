#!/usr/bin/env python3
"""Parité des options C/Python + oracles indépendants, sans affichage ni capture."""

import argparse
from dataclasses import asdict
import hashlib
import itertools
import json
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("native", type=Path)
    p.add_argument("--python-root", required=True, type=Path)
    p.add_argument("--fixtures", type=Path)
    p.add_argument("--output", required=True, type=Path)
    a = p.parse_args()
    sys.path.insert(0, str(a.python_root))
    from editor.folder_search import FolderSearchTask, Limits, find_hits, next_hit
    from editor.search_pattern import SearchOptions, PatternError

    models = scans = navigation = stdlib = 0
    fixtures = []
    native = a.native.resolve()

    def arguments(o):
        return [f"{k}={int(v) if type(v) is bool else v}" for k, v in asdict(o).items()]

    def invoke(*args):
        r = subprocess.run(
            [str(native), *map(str, args)],
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=12,
        )
        return json.loads(r.stdout)

    def spans(text, hits):
        # A prefix map makes conversion linear even at the 16,384-hit limit.
        offsets = [0]
        for c in text:
            offsets.append(offsets[-1] + len(c.encode()))
        return [
            [offsets[h.start], offsets[h.end], h.line, h.column, h.width] for h in hits
        ]

    def equal(x, y, context):
        if x != y:
            raise AssertionError(
                json.dumps(
                    {"context": context, "expected": x, "actual": y}, ensure_ascii=False
                )[:16000]
            )

    with tempfile.TemporaryDirectory(prefix="studio-options-") as temp:
        root = Path(temp)
        source = root / "model.txt"
        cases = []
        for sensitive, whole, regex in itertools.product((False, True), repeat=3):
            o = SearchOptions(case_sensitive=sensitive, whole_word=whole, regex=regex)
            for text, query in [
                ("Cat catfish CAT _cat 猫cat cat2 cat!", "cat"),
                ("Été été ÉTÉ étés", "été"),
                ("x\tété\ny été", "été"),
                ("[a]* [A]*", "[a]*"),
                (r"\E \e", r"\E"),
            ]:
                cases.append((text, query, o, 16384))
        special = [
            ("a12 a3 x", r"a\d+"),
            ("aa bb ab", r"([ab])\1"),
            ("id=42 code=3 id=9", r"(?<=id=)\d+"),
            ("a\nb\nend", r"^\w+$"),
            ("猫😀", r"(?=.)|$"),
            ("", r"\z"),
            ("α\tA\nx\nA\tend A", r"A\nx\nA|A"),
            ("x\nα\ty\nz", r"(?s)x.*y"),
            ("ab", r"a|ab"),
            ("x" * 50000 + "é", r"é$"),
            ("a" * 10000 + "!", r"(*NO_START_OPT)(*NO_AUTO_POSSESS)(a|aa)+$"),
        ]
        for t, q in special:
            cases.append((t, q, SearchOptions(regex=True), 16384))
            cases.append(
                (
                    t,
                    q,
                    SearchOptions(regex=True, whole_word=True, case_sensitive=False),
                    16384,
                )
            )
        for q in ("(", "[", r"\C", "\\", "a{4,2}"):
            cases.append(("猫 xx", q, SearchOptions(regex=True), 16384))
        cases.append(("猫" * 40000, r"(?=.)", SearchOptions(regex=True), 17))
        rng = random.Random(1130)
        for _ in range(80):
            text = "".join(
                rng.choice("abAB αé猫😀_12\t\n") for _ in range(rng.randrange(10, 1200))
            )
            q = rng.choice((r"[ab]+", r"\d+", r"[αé猫]", r"\w{2,5}", r"a|BB", r"(?=.)"))
            cases.append(
                (
                    text,
                    q,
                    SearchOptions(regex=True, case_sensitive=rng.choice((True, False))),
                    rng.choice((1, 7, 16384)),
                )
            )
        for text, query, o, limit in cases:
            source.write_text(text, encoding="utf-8")
            actual = invoke("--model", query, source, limit, *arguments(o))
            try:
                expected = {"hits": spans(text, find_hits(text, query, limit, o))}
            except (ValueError, PatternError):
                expected = {"error": True}
            if "error" in expected:
                equal(
                    True,
                    bool(actual.get("error")),
                    ("invalid/limited model", query, asdict(o)),
                )
            else:
                equal(expected, actual, ("model", query, asdict(o)))
            models += 1
        # Independent stdlib spans for the common, nonempty PCRE2/re subset.
        text = "alpha ALPHA alpha_ a12 a3 éé\nβ\t42\nend"
        for q in (
            r"a\w+",
            r"[0-9]+",
            r"(a)(lpha)",
            r"^\w+",
            r"alpha|end",
            r"(?<=a)\d+",
            r"\béé\b",
        ):
            for sensitive in (True, False):
                o = SearchOptions(regex=True, case_sensitive=sensitive)
                expected = [
                    m.span()
                    for m in re.finditer(q, text, re.M | (0 if sensitive else re.I))
                ]
                equal(
                    expected,
                    [(h.start, h.end) for h in find_hits(text, q, options=o)],
                    ("stdlib", q, sensitive),
                )
                stdlib += 1
        tree = root / "files"
        tree.mkdir()
        data = {
            "main.py": "Cat catfish CAT cat\r\n",
            "a.XML": "CAT\r",
            "src/a.py": "α\tcat\nCAT\n",
            "src/deep/b.xml": "cat\n",
            "tests/test.py": "cat cat\n",
            "nested/src/c.py": "Cat\n",
            ".env": "cat\n",
            "empty.txt": "",
            "[ab].txt": "cat\n",
            "猫.py": "cat\n",
            "a\\b.py": "cat\n",
            ".git/hidden.py": "cat\n",
        }
        for name, text in data.items():
            path = tree / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(text.encode())
        combos = [
            SearchOptions(),
            SearchOptions(recursive=False),
            SearchOptions(case_sensitive=False),
            SearchOptions(whole_word=True),
            SearchOptions(regex=True),
            SearchOptions(regex=True, whole_word=True, case_sensitive=False),
        ]
        for field, values in {
            "include_files": [
                "*.py;*.xml",
                "src/*",
                "src/**",
                "**/?.py",
                "[ab].txt",
                "*.nonexistent",
            ],
            "exclude_files": ["*.py", "src/**;tests/*"],
            "include_dirs": ["src", "src/deep", ".", "**/src"],
            "exclude_dirs": ["src;tests", "nested/**", "**/deep"],
        }.items():
            for value in values:
                combos.append(SearchOptions(**{field: value}, case_sensitive=False))
        combos.extend(
            [
                SearchOptions(
                    case_sensitive=False,
                    whole_word=True,
                    regex=True,
                    include_files="*.py;*.xml",
                    exclude_files="b.xml",
                    include_dirs="src",
                    exclude_dirs="nested",
                ),
                SearchOptions(include_files="*.py", recursive=False),
                SearchOptions(include_dirs="src", recursive=False),
                SearchOptions(include_files="../bad"),
            ]
        )

        def scan(o, query, limits):
            nonlocal scans, navigation
            actual = invoke(
                "--scan",
                tree,
                query,
                *arguments(o),
                *[f"{k}={v}" for k, v in limits.items()],
            )
            try:
                task = FolderSearchTask(tree, query, options=o, limits=Limits(**limits))
            except ValueError:
                equal(True, bool(actual.get("error")), "invalid options")
                scans += 1
                return
            try:
                if not task.done.wait(10):
                    raise AssertionError("Python worker timeout")
                if task.error:
                    equal(True, bool(actual.get("error")), ("scan error", asdict(o)))
                    scans += 1
                    return
                r = task.result
                expected = dict(
                    root=r.root,
                    query=r.query,
                    files=[
                        dict(
                            relative=f.relative, text=f.text, hits=spans(f.text, f.hits)
                        )
                        for f in r.files
                    ],
                    hit_count=r.hit_count,
                    entries=r.entries,
                    examined=r.examined,
                    skipped=r.skipped,
                    ignored=r.ignored,
                    read_bytes=r.read_bytes,
                    snapshot_bytes=r.snapshot_bytes,
                    limited=r.limited,
                    cancelled=r.cancelled,
                )
                for direction, name in ((1, "forward"), (-1, "backward")):
                    position = (-1, -1)
                    cycle = []
                    for _ in range(r.hit_count + 1 if r.hit_count else 0):
                        position = next_hit(r, *position, direction)
                        cycle.append(list(position))
                        navigation += 1
                    expected[name] = cycle
                equal(expected, actual, ("scan", query, asdict(o), limits))
                scans += 1
            finally:
                task.close()

        for o in combos:
            scan(o, r"c[a-z]+" if o.regex else "cat", {})
        for q in (r"(?=.)", r"(?s)cat.*CAT", r"^$", r"("):
            scan(SearchOptions(regex=True, include_files="*.py;*.txt"), q, {})
        for limits in (
            {"hits": 2},
            {"files": 1},
            {"read_bytes": 20},
            {"snapshot_bytes": 20},
            {"depth": 1},
            {"entries": 4},
        ):
            scan(SearchOptions(regex=True, case_sensitive=False), "cat", limits)
        if a.fixtures:
            for f in tree.iterdir():
                if f.is_file():
                    f.unlink()
            target = tree / "odoo"
            target.mkdir()
            for item in json.loads(
                (a.python_root / "tests/xml-fixtures.json").read_text()
            )["fixtures"]:
                raw = (a.fixtures / item["filename"]).read_bytes()
                equal(
                    item["sha256"], hashlib.sha256(raw).hexdigest(), "pinned Odoo bytes"
                )
                (target / item["filename"]).write_bytes(raw)
                fixtures.append(
                    dict(version=item["version"], bytes=len(raw), sha256=item["sha256"])
                )
            scan(
                SearchOptions(
                    regex=True,
                    whole_word=True,
                    include_files="odoo/*.xml",
                    case_sensitive=False,
                ),
                r"partner_id|order_line",
                {},
            )
            scan(
                SearchOptions(regex=True, include_dirs="odoo"),
                r'id="view_order_[^"]+"',
                {},
            )
    report = dict(
        status="passed",
        models=models,
        real_filesystem_scans=scans,
        global_navigation_steps=navigation,
        independent_stdlib_regex_cases=stdlib,
        engine="system PCRE2-8, UTF/UCP/multiline, no JIT; raw \\C forbidden",
        regex_limits=dict(
            match_steps=200000, depth=512, heap_kib=8192, seconds_per_file=2
        ),
        glob_grammar="; separator; * and ? exclude /; ** crosses /; **/ may match no directory; [] literal; filenames case-sensitive",
        odoo_fixtures=fixtures,
        scope="actual independent Python and C controllers tested separately; this oracle covers both cores, flags, four filters, zero/multiline spans and global navigation; no display/capture/server",
    )
    a.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
