"""Téléchargement explicite des deux fixtures XML Odoo épinglées, pour la QA."""

from pathlib import Path
import argparse
import hashlib
import json
import os
import tempfile
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", required=True, type=Path)
    args = parser.parse_args()
    manifest = Path(__file__).resolve().parents[1] / "tests/xml-fixtures.json"
    fixtures = json.loads(manifest.read_text(encoding="utf-8"))["fixtures"]
    args.directory.mkdir(parents=True, exist_ok=True)
    for item in fixtures:
        target = args.directory / item["filename"]
        if target.is_symlink():
            raise SystemExit(f"Lien refusé : {target}")
        if target.exists():
            raw = target.read_bytes()
        else:
            request = urllib.request.Request(
                item["url"], headers={"User-Agent": "Python-Studio-XML-QA/1.9"}
            )
            with urllib.request.urlopen(request, timeout=30) as response:
                raw = response.read(8 * 1024 * 1024 + 1)
        if (
            len(raw) != item["bytes"]
            or hashlib.sha256(raw).hexdigest() != item["sha256"]
        ):
            raise SystemExit(f"Fixture modifiée, aucune publication : {item['url']}")
        if not target.exists():
            with tempfile.NamedTemporaryFile(
                dir=args.directory, delete=False
            ) as stream:
                temporary = Path(stream.name)
                try:
                    stream.write(raw)
                    stream.flush()
                    os.fsync(stream.fileno())
                except BaseException:
                    temporary.unlink(missing_ok=True)
                    raise
            try:
                temporary.chmod(0o644)
                os.replace(temporary, target)
            finally:
                temporary.unlink(missing_ok=True)
        print(
            f"OK {item['version']}: {target.name}, {len(raw)} octets, SHA256 {item['sha256']}"
        )


if __name__ == "__main__":
    main()
