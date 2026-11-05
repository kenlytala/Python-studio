#!/usr/bin/env python3
"""Oracle indépendant : résultats explicites, XML stdlib, historique et parité.

Ne démarre aucun affichage. Le probe C ne lit que le fichier temporaire donné.
L'option --python-root ajoute le vrai modèle de l'édition Python aux mêmes cas.
"""

from __future__ import annotations
import argparse
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile
from xml.dom import minidom


def split(marked):
    assert marked.count("|") == 1
    position = marked.index("|")
    return marked.replace("|", ""), position


def cases():
    fixed = [
        ('<record id="x"|', "type", ">", '<record id="x">|</record>', True),
        ("<r><a|</r>", "type", ">", "<r><a>|</a></r>", True),
        ("<a|></a>", "type", ">", "<a>|</a>", True),
        ("<a|body</a>", "type", ">", "<a>|body</a>", True),
        ("<a|<b>body</b></a>", "type", ">", "<a>|<b>body</b></a>", True),
        ("<a|<!-- </a> --></a>", "type", ">", "<a>|<!-- </a> --></a>", True),
        ("<a|<![CDATA[</a>]]></a>", "type", ">", "<a>|<![CDATA[</a>]]></a>", True),
        ("<a /|", "type", ">", "<a />|", True),
        ("<a></a|", "type", ">", "<a></a>|", True),
        ("<r><form><|</r>", "type", "/", "<r><form></form>|</r>", True),
        ("<r><a/><|", "type", "/", "<r><a/></r>|", True),
        ("<r><|</r>", "type", "/", "<r></r>|", True),
        ("<r><| \n</r>", "type", "/", "<r> \n</r>|", True),
        ("<|", "type", "/", "</|", False),
        ('<r url="https:|"/>', "type", "/", '<r url="https:/|"/>', True),
        ("<r><!-- <x| --></r>", "type", ">", "<r><!-- <x>| --></r>", True),
        ("<r><![CDATA[<x|]]></r>", "type", ">", "<r><![CDATA[<x>|]]></r>", True),
        ("<r><?note <x|?></r>", "type", ">", "<r><?note <x>|?></r>", True),
        (
            '<!DOCTYPE r [<!ENTITY x "<a|">]><r/>',
            "type",
            ">",
            '<!DOCTYPE r [<!ENTITY x "<a>|">]><r/>',
            False,
        ),
        ('<r a="x|"/>', "type", ">", '<r a="x>|"/>', True),
        ("<a></wrong><x|", "type", ">", "<a></wrong><x>|", False),
        ("<a <b|", "type", ">", "<a <b>|", False),
        ("<3bad|", "type", ">", "<3bad>|", False),
        (
            "    <form>|</form>",
            "newline",
            "",
            "    <form>\n        |\n    </form>",
            True,
        ),
        ("\t<form>|</form>", "newline", "", "\t<form>\n\t\t|\n\t</form>", True),
        ("  <r>texte:|</r>", "newline", "", "  <r>texte:\n  |</r>", True),
        ("  <a/>|", "newline", "", "  <a/>\n  |", True),
        ("<re|", "complete", "record", "<record>|</record>", True),
        (
            '<rec| id="x>y"></record>',
            "complete",
            "record",
            '<record id="x>y">|</record>',
            True,
        ),
        (
            '<rec| id="x>y">texte<b/>tail</record>',
            "complete",
            "record",
            '<record id="x>y">|texte<b/>tail</record>',
            True,
        ),
        ('<fi| name="x"/>', "complete", "field", '<field name="x"/>|', True),
        ("<r><foo></f|", "complete", "foo", "<r><foo></foo>|", False),
        (
            '<r xmlns:q="urn:q"><q:é|',
            "complete",
            "q:été",
            '<r xmlns:q="urn:q"><q:été>|</q:été>',
            False,
        ),
        (
            '<r><f| id="unfinished',
            "complete",
            "field",
            '<r><field| id="unfinished',
            False,
        ),
        ("<" + "a" * 128 + "|", "type", ">", "<" + "a" * 128 + ">|", False),
    ]
    for row in fixed:
        yield row
    rng = random.Random(1100)
    tags = ["record", "field", "template", "custom-name", "猫", "été", "q:form", "αtag"]
    for number in range(48):
        name = tags[number % len(tags)]
        attr = rng.choice(
            ["", ' id="x"', ' value="a>b"', " name='été'", ' value="&amp;"']
        )
        opening = '<r xmlns:q="urn:q"><' + name + attr
        closing = "</" + name + "></r>"
        yield opening + "|</r>", "type", ">", opening + ">|" + closing, True
        yield (
            opening + ">|</" + name + "></r>",
            "newline",
            "",
            opening + ">\n    |\n" + closing,
            True,
        )
        yield (
            opening + ">text<|</r>",
            "type",
            "/",
            opening + ">text" + closing[:-4] + "|</r>",
            True,
        )
        body = rng.choice(
            ["text", "<x>inside</x>", "<![CDATA[</fake>]]>", "<!-- </fake> -->"]
        )
        yield (
            opening + "|" + body + closing,
            "type",
            ">",
            opening + ">|" + body + closing,
            True,
        )
        prefix = name[: max(1, len(name) - 1)]
        original = '<r xmlns:q="urn:q"><' + prefix + "|" + attr + ">" + body + closing
        expected = '<r xmlns:q="urn:q"><' + name + attr + ">|" + body + closing
        yield original, "complete", name, expected, True


def python_result(model, edit_module, text, cursor, op, arg):
    b = model.TextBuffer(text, name="view.xml")
    b.cursor = b.position(cursor)
    before = b.snapshot()
    if op == "type":
        b.type_character(arg)
    elif op == "newline":
        b.newline()
    else:
        b._apply_xml_edit(edit_module.completion_edit(text, cursor, arg))
    result = b.text, b.offset(b.cursor)
    after = b.snapshot()
    undo = len(b.undo_stack)
    if undo:
        b.undo()
        assert b.snapshot() == before
        b.redo()
        assert b.snapshot() == after
    return result, undo


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("probe", type=Path)
    parser.add_argument("--python-root", type=Path)
    args = parser.parse_args()
    model = edit_module = None
    if args.python_root:
        sys.path.insert(0, str(args.python_root.resolve()))
        from editor import buffer as model, xml_edit as edit_module
    total = valid = histories = suggestions = 0
    with tempfile.TemporaryDirectory(
        prefix="python-studio-xml-edit-oracle-"
    ) as temporary:
        source = Path(temporary) / "input.xml"

        def native(text, cursor, op, arg=""):
            source.write_text(text, encoding="utf-8")
            byte_cursor = len(text[:cursor].encode("utf-8"))
            command = [
                str(args.probe.resolve()),
                "--probe",
                str(source),
                str(byte_cursor),
                op,
            ]
            if op in ("type", "complete"):
                command.append(arg)
            run = subprocess.run(
                command, capture_output=True, text=True, encoding="utf-8", timeout=15
            )
            assert run.returncode == 0, (command, run.stderr)
            assert source.read_text("utf-8") == text, (
                "A probe must never write the source."
            )
            return json.loads(run.stdout)

        for marked_input, op, arg, marked_output, well_formed in cases():
            text, cursor = split(marked_input)
            expected, caret = split(marked_output)
            actual = native(text, cursor, op, arg)
            assert actual["ok"] and actual["text"] == expected, (
                total,
                marked_input,
                op,
                arg,
                actual,
                marked_output,
            )
            assert actual["cursor"] == len(expected[:caret].encode("utf-8")), (
                marked_input,
                actual,
                marked_output,
            )
            assert actual["undo"] == int(text != expected), (marked_input, actual)
            assert actual["undo_restores"] and actual["redo_restores"]
            if well_formed:
                doc = minidom.parseString(expected)
                doc.unlink()
                valid += 1
            if model:
                pair, undo = python_result(model, edit_module, text, cursor, op, arg)
                assert pair == (expected, caret), (marked_input, pair, marked_output)
                assert undo == actual["undo"]
            total += 1
            histories += actual["undo"]
        suggestion_inputs = [
            "<",
            "<re",
            "<r><foo></f",
            '<r xmlns:q="urn:q"><q:été/><q:',
            '<r a="fi',
            "<!-- <fi",
            "<![CDATA[<fi",
            "<?note <fi",
            "<r>text",
            "<r><long-name/><long-",
            "<猫/><猫",
            "<r/><",
            "<r><x/></",
            "</",
        ]
        for text in suggestion_inputs:
            actual = native(text, len(text), "list")["items"]
            if model:
                expected = edit_module.tag_completions(text, len(text))
                if expected is not None:
                    expected = [
                        dict(
                            name=x["name"],
                            kind=x["kind"],
                            start=len(text[: x["start"]].encode()),
                            end=len(text[: x["end"]].encode()),
                        )
                        for x in expected
                    ]
                assert actual == expected, (text, actual, expected)
            if actual:
                assert all(
                    x["kind"] in ("X", "T")
                    and x["start"] <= x["end"] <= len(text.encode())
                    for x in actual
                )
            suggestions += 1
    print(
        json.dumps(
            dict(
                cases=total,
                valid_xml_documents=valid,
                atomic_history_roundtrips=histories,
                suggestion_contexts=suggestions,
                python_and_c=bool(model),
                stdlib_oracle=True,
            ),
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
