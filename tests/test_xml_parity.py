"""Independent stdlib XML oracle for the C probe; optional Python-edition parity.

CPU-only subprocesses; no editor window, network, framework or database.
"""

from copy import deepcopy
from pathlib import Path
import argparse
import importlib.util
import json
import random
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape, quoteattr

SPACE = "{http://www.w3.org/XML/1998/namespace}space"


def local(tag):
    return (
        tag.rsplit("}", 1)[-1]
        if isinstance(tag, str)
        else "#comment"
        if tag is ET.Comment
        else "#pi"
    )


def parse(raw):
    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True, insert_pis=True))
    return ET.fromstring(
        raw.decode("utf-8-sig") if isinstance(raw, bytes) else raw, parser=parser
    )


def signature(node, preserve=False):
    space = node.get(SPACE)
    preserve = (
        True if space == "preserve" else False if space == "default" else preserve
    )
    children = list(node)
    mixed = bool((node.text or "").strip(" \t\r\n")) or any(
        (c.tail or "").strip(" \t\r\n") for c in children
    )
    block = bool(children) and not mixed and not preserve
    text = node.text or ""
    if block and not text.strip(" \t\r\n"):
        text = ""
    parts = []
    for child in children:
        tail = child.tail or ""
        if block and not tail.strip(" \t\r\n"):
            tail = ""
        parts.append((signature(child, preserve), tail))
    return (
        node.tag if isinstance(node.tag, str) else local(node.tag),
        tuple(sorted(node.attrib.items())),
        text,
        tuple(parts),
    )


def empty_arch(node):
    if local(node.tag) == "field" and node.get("name") == "arch":
        node.text = None
        for child in list(node):
            node.remove(child)
        for attr in ("eval", "ref", "file"):
            node.attrib.pop(attr, None)
    else:
        for child in node:
            empty_arch(child)


def expected(source, selected):
    parents = {child: parent for parent in source.iter() for child in parent}
    clone = deepcopy(selected)
    clone.tail = None
    ancestor = selected
    while SPACE not in clone.attrib and ancestor in parents:
        ancestor = parents[ancestor]
        if SPACE in ancestor.attrib:
            clone.set(SPACE, ancestor.get(SPACE))
    empty_arch(clone)
    return signature(clone)


def cases():
    fixed = [
        '<odoo><record id="view_order_form" model="ir.ui.view"><field name="name">sale.order.form</field><field name="model">sale.order</field><field name="arch" type="xml"><form><x id="inside"/></form></field></record><menuitem id="menu"/></odoo>',
        '<odoo><x xml:id="skip"/><record id="duplicate" model="one"/><record id="duplicate" model="two"/></odoo>',
        '<odoo xmlns:q="urn:q"><q:r q:id="skip"/><q:r id="élément" model="modèle" q:attr="val"><q:field q:name="arch"><q:keep/></q:field><q:field name="arch" q:eval="keep" eval="discard" type="xml"/></q:r></odoo>',
        '<odoo xml:space="preserve"><r id="x">\n <!-- literal <!DOCTYPE fake> --><?note yes?><f>A\nB&#13;C<![CDATA[<z>]]></f>\n</r></odoo>',
        '<odoo xmlns="urn:a" xmlns:q="urn:q"><data xmlns:q="urn:shadow"><record id="x"><q:field name="name">été</q:field></record></data></odoo>',
        '<r id="root"><p>Hello <b id="b">bold</b> tail <i>italic</i> !</p><field name="arch" file="x" ref="r" eval="e"/><field name="name"> \n \t </field></r>',
        '<r><x id="a&#10;b&#9;c&quot;d" model="&lt;&amp;&quot;"/><q id="猫"/></r>',
        '<?xml version="1.0" encoding="ISO-8859-1"?><odoo><r id="été"/></odoo>',
        '<r><x id=""/><x xml:id="skip"/></r>',
    ]
    for i, text in enumerate(fixed):
        yield f"fixed-{i}", text.encode()
    yield "utf8-bom", b"\xef\xbb\xbf" + '<r id="été"/>'.encode()
    randomizer = random.Random(190)
    words = ["été", "猫", "<&>", "line\nline\rline", "\t ", "simple", '"quoted"']
    for i in range(36):
        parts = ['<odoo xmlns:q="urn:test"><data>']
        for j in range(randomizer.randint(1, 4)):
            identifier = f"view_{i}_{j}"
            parts.append("<record id=" + quoteattr(identifier) + ' model="ir.ui.view">')
            parts.append(
                '<field name="name">'
                + escape(randomizer.choice(words)).replace("\r", "&#13;")
                + "</field>"
            )
            parts.append(
                '<field name="model">sale.order</field><field name="arch" type="xml"><form><q:field id="inside_'
                + identifier
                + '"/></form></field>'
            )
            parts.append("</record>")
        parts.append("</data></odoo>")
        yield f"random-{i}", "".join(parts).encode()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("driver", type=Path)
    parser.add_argument("--python-root", type=Path)
    parser.add_argument("--fixtures", type=Path)
    args = parser.parse_args()
    python_class = None
    if args.python_root:
        spec = importlib.util.spec_from_file_location(
            "studio_xml_refs_parity", args.python_root / "editor/xml_refs.py"
        )
        module = importlib.util.module_from_spec(spec)
        import sys

        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        python_class = module.XMLReferences
    data = list(cases())
    if args.fixtures:
        data.extend(
            (path.name, path.read_bytes())
            for path in sorted(args.fixtures.glob("*.xml"))
        )
    fragments = identifiers = 0
    with tempfile.TemporaryDirectory(prefix="studio-xml-oracle-") as directory:
        path = Path(directory) / "source été.xml"
        for name, raw in data:
            source = parse(raw)
            nodes = [node for node in source.iter() if node.get("id")]
            py = python_class(raw) if python_class else None
            path.write_bytes(raw)
            for index in range(max(1, len(nodes))):
                result = subprocess.run(
                    [str(args.driver), "--probe", str(path), str(index)],
                    capture_output=True,
                    text=True,
                    timeout=10,
                )
                assert result.returncode == 0, (name, index, result.stderr)
                native = json.loads(result.stdout)
                wanted = [
                    (node.get("id"), local(node.tag), node.get("model", ""))
                    for node in nodes
                ]
                observed = [
                    (item["id"], item["tag"], item["model"]) for item in native["items"]
                ]
                assert observed == wanted, (name, wanted, observed)
                if py:
                    assert [
                        (item.identifier, item.tag, item.model) for item in py.items
                    ] == wanted, name
                if nodes:
                    target = expected(source, nodes[index])
                    assert signature(parse(native["snippet"])) == target, (
                        name,
                        index,
                        native["snippet"],
                    )
                    if py:
                        assert signature(parse(py.snippet(index))) == target, (
                            name,
                            index,
                            py.snippet(index),
                        )
                    fragments += 1
                else:
                    assert native["snippet"] is None, name
            identifiers += len(nodes)
        invalid = [
            b"",
            b"<r>",
            b'<q:r id="x"/>',
            b'<r id="x" id="y"/>',
            b"<r>&unknown;</r>",
            b'<!DOCTYPE r SYSTEM "file:///does-not-exist"><r/>',
            b'<!DOCTYPE r [<!ENTITY e "secret">]><r id="x">&e;</r>',
            b"<r>\0</r>",
            b"<r>\xff</r>",
            "<r/>".encode("utf-16"),
        ]
        for raw in invalid:
            path.write_bytes(raw)
            result = subprocess.run(
                [str(args.driver), "--probe", str(path)],
                capture_output=True,
                timeout=10,
            )
            assert result.returncode == 1 and result.stderr, (
                raw,
                result.returncode,
                result.stdout,
            )
            if python_class:
                try:
                    python_class(raw)
                except (ValueError, UnicodeError):
                    pass
                else:
                    raise AssertionError(raw)
    print(
        json.dumps(
            dict(
                valid_documents=len(data),
                identifiers=identifiers,
                fragments=fragments,
                invalid_documents=len(invalid),
                python_and_c=bool(python_class),
                stdlib_oracle=True,
            ),
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
