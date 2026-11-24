import argparse
import json
import re
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


def character_addon_entries(source, readme, title, version, minimum_core="1.0.5", *, license_file=None):
    if not re.fullmatch(r"[a-z0-9_-]{1,64}", source.stem):
        raise ValueError("Use a lowercase manifest filename with letters, digits, hyphens or underscores")
    manifest = source.read_text(encoding="utf-8-sig").rstrip("\n")
    if not manifest.startswith("HexAppealCharacters\t1\n") or "]]>" in manifest or "\0" in manifest:
        raise ValueError("Expected a HexAppealCharacters version 1 manifest")
    if not re.fullmatch(r"[ -~]{1,63}", title) or "##" in title or "]]>" in title:
        raise ValueError("Use a plain-text patch title of at most 63 characters")
    if any(not re.fullmatch(r"\d+\.\d+\.\d+", value, re.ASCII) for value in (version, minimum_core)):
        raise ValueError("Use numeric major.minor.patch versions")
    header, rows = manifest.split("\n", 1)
    manifest = f"{header}\n#patch\t{title}\t{version}\t{minimum_core}\n{rows}"
    payload = f'<HexAppealCharacters version="1"><![CDATA[\n{manifest}\n]]></HexAppealCharacters>\n'
    if len(payload.encode("utf-8")) > 32 * 1024:
        raise ValueError("The mounted character add-on must fit within 32 KiB")
    route = f"character/hexappeal/characters/{source.stem}.xml"
    entries = {
        "mod.json": (json.dumps({
            "modinfo": {
                "title": title, "author": "TASx69", "version": version,
                "description": "Adds optional characters to Hex Appeal.",
            },
            "new_paths": ["character/hexappeal/characters/"],
        }, indent=2) + "\n").encode("utf-8"),
        f"files/0009/{route}": payload.encode("utf-8"),
        "README.md": readme.read_bytes(),
    }
    if license_file is not None:
        entries["LICENSE"] = license_file.read_bytes()
    return route, entries


def package_character_addon(source, readme, output, title, version, minimum_core="1.0.5", *, license_file=None):
    route, entries = character_addon_entries(source, readme, title, version, minimum_core,
                                             license_file=license_file)
    output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(output, "x", compression=ZIP_DEFLATED) as archive:
        for name, data in entries.items():
            archive.writestr(name, data)
    with ZipFile(output) as archive:
        if archive.testzip() or set(archive.namelist()) != set(entries) or any(
            archive.read(name) != data for name, data in entries.items()
        ):
            raise ValueError("Character add-on archive did not round trip")
    return route


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Package a Hex Appeal character manifest as a regular DMM mod.")
    parser.add_argument("source", type=Path)
    parser.add_argument("readme", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--title", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--minimum-core", default="1.0.5")
    parser.add_argument("--license-file", type=Path, help="Include a license for the add-on's own files.")
    args = parser.parse_args()
    print(package_character_addon(args.source, args.readme, args.output, args.title, args.version,
                                  args.minimum_core, license_file=args.license_file))
