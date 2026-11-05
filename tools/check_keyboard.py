"""Parité des projections clavier C/Python et ABI XKB, sans serveur X ni fenêtre."""

import argparse
import ctypes as ct
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native_model", type=Path)
    parser.add_argument("native_provider", type=Path)
    parser.add_argument("--python-root", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    sys.path[:0] = [str(args.python_root), str(args.python_root / "tests")]
    from editor import keyboard as kb
    from test_keyboard import qwerty, azerty, qwertz, dvorak

    native = json.loads(
        subprocess.check_output([str(args.native_model), "--oracle"], text=True)
    )
    expected = []
    for symbols in (qwerty(), azerty(), qwertz(), dvorak()):
        model = kb.Keyboard()
        assert model.configure(symbols)
        expected.append(
            dict(
                logical=[model.logical(k) for k in range(512)],
                source=[model.source(k) for k in range(512)],
            )
        )
    assert native == {"layouts": expected}, "Divergence des projections C/Python"
    native_abi = json.loads(
        subprocess.check_output([str(args.native_provider), "--abi"], text=True)
    )
    python_abi = {}
    for name, fields in (
        ("XkbState", ("group", "ptr_buttons")),
        ("XkbKeyName", ()),
        ("XkbAlias", ("real", "alias")),
        ("XkbNames", ("keys", "key_aliases", "num_key_aliases", "phys_symbols")),
        ("XkbDesc", ("min_key_code", "max_key_code", "names", "map")),
    ):
        cls = getattr(kb, name)
        python_abi[name] = {
            "size": ct.sizeof(cls),
            **{field: getattr(cls, field).offset for field in fields},
        }
    assert python_abi == native_abi, "ABI ctypes incompatible avec les en-têtes XKB"
    result = dict(
        status="passed",
        layouts=4,
        projection_values=4096,
        xkb_abi=native_abi,
        no_display_window_or_layout_mutation=True,
    )
    text = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()
