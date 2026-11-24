import argparse
import json
import re
from pathlib import Path


METADATA = (
    (Path("project.json"), ()),
    (Path("Hex Appeal - Bodies/mod.json"), ("modinfo",)),
    (Path("Hex Appeal - Outfit Refits/mod.json"), ("modinfo",)),
)


def validate_version(version):
    if not re.fullmatch(r"(?:0|[1-9][0-9]{0,4})(?:\.(?:0|[1-9][0-9]{0,4})){2}", version):
        raise ValueError("Use a three-part release version such as 1.0.5")
    if any(int(part) > 65535 for part in version.split(".")):
        raise ValueError("Each version component must fit in the ASI version resource")
    return version


def release_version(root):
    return validate_version((root / "VERSION").read_text(encoding="utf-8-sig").strip())


def version_metadata(root):
    for relative, keys in METADATA:
        path = root / relative
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        row = data
        for key in keys:
            row = row[key]
        yield path, data, row


def check_version_metadata(root, expected):
    for path, _, row in version_metadata(root):
        if row.get("version") != expected:
            relative = path.relative_to(root)
            raise ValueError(f"{relative} must match VERSION ({expected}). Run tools/release_version.py {expected}.")


def set_release_version(root, version):
    validate_version(version)
    metadata = list(version_metadata(root))
    for path, data, row in metadata:
        if row.get("version") == version:
            continue
        row["version"] = version
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (root / "VERSION").write_text(version + "\n", encoding="ascii")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Set a release version and synchronize the source mod metadata.")
    parser.add_argument("version")
    args = parser.parse_args()
    set_release_version(Path(__file__).resolve().parents[1], args.version)
