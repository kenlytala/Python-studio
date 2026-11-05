"""Independent native/Python JS/XML lexical color-byte comparison, CPU-only."""

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
    from editor.lexers import javascript_colors, xml_colors

    fixtures = [
        "export async function test(x) { const value = `hello ${x+1}`; return /[a-z\\/]+/gi.test(value); }",
        "/* multi\nline */ const x=4/2; // end\nlet y = true && null;",
        "`nested ${items.map(x => `${x}`)} tail`",
        "`a${",
        '<?xml version="1.0"?><odoo><!-- hi --><record id="é&amp;"><field name="arch" type="xml"><list/></field></record></odoo>',
        '<templates><t t-name="mod.Comp"><![CDATA[<raw>&x;]]></t></templates>',
        '<!DOCTYPE x [<!ENTITY a "text>">]><x xmlns:t="urn">&#x41;</x>',
        '<x a="unfinished',
        'const a="unfinished',
        "a\u2003b\n\u2003const x=1",
    ]
    rnd = random.Random(1200)
    alphabet = "abcXYZ_0123456789 /\\`$[]{}<>?'\";:=+*-!&|\n\t\rαé猫😀"
    cases = fixtures + [
        "".join(rnd.choices(alphabet, k=rnd.randrange(0, 320))) for _ in range(3000)
    ]
    requests = []
    expected = []
    for lang, lexer in [("js", javascript_colors), ("xml", xml_colors)]:
        for source in cases:
            requests.append(lang + "\t" + source.encode().hex() + "\n")
            colors = lexer(source)
            expected.append(
                "".join(
                    format(kind, "x") * len(char.encode())
                    for char, kind in zip(source, colors)
                )
            )
    result = subprocess.run(
        [str(args.native), "--colors-lines"],
        input="".join(requests),
        capture_output=True,
        text=True,
        check=True,
    )
    actual = result.stdout.splitlines()
    assert len(actual) == len(expected), (len(actual), len(expected))
    for i, (left, right) in enumerate(zip(expected, actual)):
        assert left == right, (
            i // len(cases),
            repr(cases[i % len(cases)]),
            left,
            right,
        )
    output = dict(
        status="passed",
        color_byte_cases=len(expected),
        languages=["javascript", "xml"],
        utf8=True,
        templates_regex_division_cdata_entities_and_incomplete_source=True,
        no_window_or_evaluation=True,
    )
    args.output.write_text(json.dumps(output, indent=2) + "\n")
    print(args.output.read_text(), end="")


if __name__ == "__main__":
    main()
