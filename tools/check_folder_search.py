#!/usr/bin/env python3
"""Compare les deux moteurs et toute la boucle F3 sur de vrais fichiers locaux.

Exécution hors interface : aucun affichage, rendu, serveur ni capture.
"""

import argparse
import hashlib
import json
import os
import random
import subprocess
import sys
import tempfile
from dataclasses import asdict
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("--python-root", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.python_root))
    from editor.folder_search import FolderSearchTask, Limits, find_hits, next_hit

    native = args.native.resolve()
    models = scans = navigation = 0
    fixture_proofs = []

    def invoke(*command):
        result = subprocess.run(
            [str(native), *map(str, command)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=12,
            check=True,
        )
        return json.loads(result.stdout)

    def byte_hits(text, hits):
        return [
            [
                len(text[: h.start].encode()),
                len(text[: h.end].encode()),
                h.line,
                h.column,
                h.width,
            ]
            for h in hits
        ]

    def checked_equal(expected, actual, context):
        if expected != actual:
            raise AssertionError(
                json.dumps(
                    {"context": context, "expected": expected, "actual": actual},
                    ensure_ascii=False,
                    indent=2,
                )[:18000]
            )

    with tempfile.TemporaryDirectory(prefix="studio-search-oracle-") as tmp:
        base = Path(tmp)
        source = base / "model.txt"
        cases = [
            ("", "x", 16384),
            ("aaaaa", "aa", 16384),
            ("[a]* [a]* aaa", "[a]*", 16384),
            ("Needle needle NEEDLE", "needle", 16384),
            ("α\tété\nXété été", "été", 16384),
            ("x\tété\tété", "\tété", 16384),
            ("e\u0301 é 猫 😀 العربية", "😀", 16384),
            ("x" * 200000 + "\tété", "été", 16384),
            ("x" * 100, "x", 7),
            ("x" * (2 * 1024 * 1024 - 6) + "é\t猫", "猫", 16384),
            ("a\na\n", "a", 16384),
            ("needle", "", 16384),
            ("needle", "n\ne", 16384),
            ("needle", "é" * 513, 16384),
        ]
        rng = random.Random(1120)
        alphabet = "abxy αé猫😀\t\n.[]*e\u0301"
        for _ in range(96):
            text = "".join(rng.choice(alphabet) for _ in range(rng.randrange(1, 1400)))
            if rng.randrange(4):
                start = rng.randrange(len(text))
                query = text[start : start + rng.randrange(1, 8)].replace("\n", "x")
            else:
                query = "not-present"
            cases.append((text, query, rng.choice((1, 2, 17, 16384))))
        for text, query, limit in cases:
            source.write_text(text, encoding="utf-8")
            actual = invoke("--model", query, source, limit)
            try:
                expected = {"hits": byte_hits(text, find_hits(text, query, limit))}
            except (ValueError, UnicodeError):
                expected = {"error": True}
            if "error" in expected:
                checked_equal(True, bool(actual.get("error")), ("invalid model", query))
            else:
                checked_equal(expected, actual, ("literal model", query, limit))
            models += 1

        root = base / "files"
        root.mkdir()

        def put(name, data):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data if isinstance(data, bytes) else data.encode())
            return path

        put("a.txt", "needle + needle\r\nNeedle\rneedle\n")
        put("sub/é.xml", "α\tneedle\n猫 needle\n")
        put("z.py", 'print("needle")\n')
        put(".env", "NEEDLE=needle\n")
        put("empty", "")
        put("no", "unrelated")
        put("sub/deep/more", "needle\n" * 20)
        put("binary.big", b"\0" + b"x" * 100000)
        put("bad-utf8", b"needle\xff")
        put("control", b"needle\x7f")
        put("a\\b", "needle")
        put("vues\u2003été", "needle")
        put("line\nname", "needle")
        for dirname in (".git", ".venv", "node_modules", ".cache"):
            put(dirname + "/hidden", "needle")
        if os.name != "nt":
            os.symlink(root / "sub", root / "linkdir")
            os.symlink(root / "a.txt", root / "linkfile")
            os.mkfifo(root / "pipe")
            fd = os.open(
                os.fsencode(root) + b"/invalid-\xff", os.O_WRONLY | os.O_CREAT, 0o600
            )
            os.write(fd, b"needle")
            os.close(fd)

        def compare_scan(directory, query="needle", **limits):
            nonlocal scans, navigation
            task = FolderSearchTask(directory, query, limits=Limits(**limits))
            try:
                if not task.done.wait(10):
                    raise AssertionError("Python worker deadline")
                actual = invoke(
                    "--scan", directory, query, *[f"{k}={v}" for k, v in limits.items()]
                )
                if task.error:
                    checked_equal(
                        True,
                        bool(actual.get("error")),
                        ("worker error", str(directory)),
                    )
                else:
                    report = task.result
                    expected = {
                        key: getattr(report, key)
                        for key in (
                            "root",
                            "query",
                            "hit_count",
                            "entries",
                            "examined",
                            "skipped",
                            "ignored",
                            "read_bytes",
                            "snapshot_bytes",
                            "limited",
                            "cancelled",
                        )
                    }
                    expected["files"] = [
                        {
                            "relative": f.relative,
                            "text": f.text,
                            "hits": byte_hits(f.text, f.hits),
                        }
                        for f in report.files
                    ]
                    for direction, name in ((1, "forward"), (-1, "backward")):
                        pos = (-1, -1)
                        expected[name] = []
                        for _ in range(report.hit_count + 1 if report.hit_count else 0):
                            pos = next_hit(report, *pos, direction)
                            expected[name].append(list(pos))
                            navigation += 1
                    checked_equal(
                        expected, actual, ("scan", str(directory), query, limits)
                    )
                scans += 1
            finally:
                task.close()

        for query in (
            "needle",
            "Needle",
            "NEEDLE",
            "[a]*",
            "α\tneedle",
            "\t",
            "猫",
            "not-found",
        ):
            compare_scan(root, query)
        for limits in (
            {"files": 1},
            {"files": 3},
            {"hits": 1},
            {"hits": 5},
            {"hits": 17},
            {"file_bytes": 10},
            {"read_bytes": 20},
            {"read_bytes": 65580},
            {"snapshot_bytes": 35},
            {"entries": 2},
            {"entries": 10},
            {"depth": 1},
            {"lines": 2},
            {"lines": 1},
        ):
            compare_scan(root, **limits)
        compare_scan(root / "missing")
        compare_scan(root / "a.txt")
        if os.name != "nt":
            alias = base / "root-alias"
            os.symlink(root, alias)
            compare_scan(alias)

        boundary = base / "boundary"
        boundary.mkdir()
        (boundary / "max.txt").write_bytes(b"x" * (2 * 1024 * 1024 - 6) + b"needle")
        (boundary / "oversize").write_bytes(b"x" * (2 * 1024 * 1024 + 1))
        compare_scan(boundary)
        if args.fixtures:
            target = base / "odoo"
            target.mkdir()
            fixtures = json.loads(
                (args.python_root / "tests/xml-fixtures.json").read_text()
            )["fixtures"]
            for fixture in fixtures:
                raw = (args.fixtures / fixture["filename"]).read_bytes()
                digest = hashlib.sha256(raw).hexdigest()
                if len(raw) != fixture["bytes"] or digest != fixture["sha256"]:
                    raise AssertionError("Odoo fixture hash mismatch")
                (target / f"odoo-{fixture['version']}.xml").write_bytes(raw)
                fixture_proofs.append(
                    {"version": fixture["version"], "bytes": len(raw), "sha256": digest}
                )
            for query in (
                "sale.order",
                "partner_id",
                "view_order_form",
                '<field name="arch"',
            ):
                compare_scan(target, query)
    result = {
        "status": "passed",
        "literal_models": models,
        "real_filesystem_scans": scans,
        "global_navigation_steps": navigation,
        "offset_comparison": "UTF-8 byte offsets; scalar cells/tabs=4; normalized LF snapshots",
        "limits": asdict(Limits()),
        "odoo_fixtures": fixture_proofs,
        "scope": "actual Python and C cores; separate actual-controller/CPU-view tests; no display/capture/server",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
