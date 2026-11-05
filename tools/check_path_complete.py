#!/usr/bin/env python3
"""Oracle indépendant sans fenêtre : contextes UTF-8, éditions/historique, fusion de dossiers."""

import argparse
import importlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("--python-root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.python_root.resolve()))
    module = importlib.import_module("editor.path_complete")
    TextBuffer = importlib.import_module("editor.buffer").TextBuffer
    total = edits = history = catalogs = 0

    def invoke(*arguments):
        return json.loads(
            subprocess.run(
                [str(args.native.resolve()), *map(str, arguments)],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                timeout=10,
            ).stdout
        )

    def marked(value):
        assert value.count("|") == 1
        cursor = value.index("|")
        return value.replace("|", ""), cursor

    def byte(text, offset):
        return len(text[:offset].encode("utf-8"))

    def context_json(text, ctx):
        if ctx is None:
            return None
        return {
            key: byte(text, getattr(ctx, key))
            for key in ("start", "end", "component", "component_end")
        } | {key: getattr(ctx, key) for key in ("directory", "prefix", "quote")}

    cases = [
        ("sale/|", "views", True, "sale/views/|"),
        ("sale/vi|ews", "views", True, "sale/views/|"),
        ("sale/vi|ews/a.xml", "views", True, "sale/views/|a.xml"),
        ('"sale/|"', "views", True, '"sale/views/|"'),
        ("'sale/|'", "views.xml", False, "'sale/views.xml|'"),
        ('"sale/"|', "views", True, '"sale/views/|"'),
        ('open("sale/vi|', "views", True, 'open("sale/views/|'),
        ('<field file="sale/|"/>', "views", True, '<field file="sale/views/|"/>'),
        ("sale/|", "mes vues", True, '"sale/mes vues/|"'),
        ("sale/|", 'a"b.xml', False, "'sale/a\"b.xml|'"),
        ("sale/|", "a'b.xml", False, '"sale/a\'b.xml|"'),
        ("sale/a.xml|", "a.xml", False, "sale/a.xml|"),
        ("# été\n猫/sale/vi|ews", "vues_été", True, "# été\n猫/sale/vues_été/|"),
        ("../sale/|", "views", True, "../sale/views/|"),
        ("/opt/addons/sale/|", "views", True, "/opt/addons/sale/views/|"),
        ("~/sale/|", "views", True, "~/sale/views/|"),
    ]
    for whitespace in (
        " ",
        "\u0085",
        "\u00a0",
        "\u1680",
        "\u2000",
        "\u2003",
        "\u2028",
        "\u2029",
        "\u202f",
        "\u205f",
        "\u3000",
    ):
        name = f"mes{whitespace}vues.xml"
        cases.append(("sale/|", name, False, f'"sale/{name}|"'))
    for delimiter in ("(", ")", "[", "]", "{", "}", ",", ";", "=", "<", ">", "`"):
        name = f"a{delimiter}b.xml"
        cases.append(("sale/|", name, False, f'"sale/{name}|"'))
    contexts = [case[0] for case in cases] + [
        "sale|",
        "xml_form|",
        "<fi|",
        "</|",
        "<field /|>",
        '<field name="x" /|>',
        "sale/ |",
        "http://sale/|",
        "file:///tmp/|",
        "`sale/`|",
        "x = 2 / |",
        "sale/|\nnext",
        "é" * 2048 + "/|",
        'sale/"',
        '"sale/|',
        "a/sale/vi|ews/x/",
    ]
    with tempfile.TemporaryDirectory(prefix="studio-path-oracle-") as scratch:
        root = Path(scratch)
        source = root / "input.xml"
        for marked_text in contexts:
            if "|" not in marked_text:
                continue
            text, cursor = marked(marked_text)
            source.write_text(text, encoding="utf-8")
            ctx = module.path_context(text, cursor)
            c = invoke("--probe", source, byte(text, cursor), "context")
            assert c["context"] == context_json(text, ctx), (marked_text, c, ctx)
            total += 1
        for before, name, directory, expected in cases:
            text, cursor = marked(before)
            after, position = marked(expected)
            source.write_text(text, encoding="utf-8")
            b = TextBuffer(text, name="view.xml")
            b.cursor = b.position(cursor)
            snapshot = b.snapshot()
            ctx = module.path_context(text, cursor)
            b._apply_xml_edit(
                module.choice_edit(text, ctx, module.PathChoice(name, directory, ""))
            )
            assert b.text == after and b.offset(b.cursor) == position, (
                before,
                name,
                b.text,
            )
            if b.undo_stack:
                current = b.snapshot()
                assert len(b.undo_stack) == 1
                b.undo()
                assert b.snapshot() == snapshot
                b.redo()
                assert b.snapshot() == current
                history += 1
            c = invoke(
                "--probe", source, byte(text, cursor), "edit", name, int(directory)
            )
            assert (
                c["ok"]
                and c["text"] == after
                and c["cursor"] == byte(after, position)
                and c["history"]
                and c["undo"] == len(b.undo_stack)
            ), (before, name, c)
            edits += 1
        bases = [root / "first", root / "second"]
        specifications = [
            [
                ("views", True),
                ("models", True),
                ("a.xml", False),
                ("été.xml", False),
                ("deux mots.csv", False),
                (".hidden", False),
            ],
            [("views", True), ("reports", True), ("a.xml", False), ("z.py", False)],
        ]
        for base, entries in zip(bases, specifications):
            (base / "sale").mkdir(parents=True)
            for name, directory in entries:
                path = base / "sale" / name
                if directory:
                    path.mkdir()
                else:
                    path.write_bytes(b"unchanged; not parsed")
        for prefix in ("", "v", "a", "absent"):
            expected = {}
            for base, entries in zip(bases, specifications):
                for name, directory in entries:
                    if name.startswith(prefix):
                        expected.setdefault(
                            (name, directory),
                            {
                                "name": name,
                                "directory": directory,
                                "parent": str(base / "sale"),
                            },
                        )
            expected = sorted(
                expected.values(),
                key=lambda row: (not row["directory"], row["name"].encode("utf-8")),
            )[:32]
            task = module.PathLoadTask(
                module.path_context("sale/" + prefix, 5 + len(prefix)),
                bases,
                lambda p: p,
            )
            try:
                assert task.done.wait(5) and not task.error
                c = invoke("--list", bases[0] / "sale", prefix, bases[1] / "sale")
                python = [
                    {"name": x.name, "directory": x.directory, "parent": x.parent}
                    for x in task.result.choices
                ]
                assert (
                    c["ok"]
                    and c["items"] == expected == python
                    and not c["limited"]
                    and c["scanned"] == task.result.scanned
                )
            finally:
                task.close()
            catalogs += 1
        for i in range(60):
            (bases[0] / "sale" / f"f{i:03}.xml").touch()
        c = invoke("--list", bases[0] / "sale", "f")
        assert c["limited"] and [x["name"] for x in c["items"]] == [
            f"f{i:03}.xml" for i in range(32)
        ]
        catalogs += 1
    result = {
        "contexts": total,
        "edits": edits,
        "atomic_history_roundtrips": history,
        "catalog_snapshots": catalogs,
        "unicode_offsets": "native UTF-8 bytes / Python code points",
        "status": "passed",
    }
    output = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    print(output, end="")
    if args.output:
        args.output.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
