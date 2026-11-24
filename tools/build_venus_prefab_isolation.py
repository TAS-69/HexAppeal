import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile

from part_registry import read_registry, write_registry
from witch_parts import PRIVATE_BOOTS


SHARED = "phw_00_nude_00_0001_damian"
PRIVATE = "hva_00_nude_00_0001_damian"
ORIGINAL_DIRECTORY = "1_pc/02_phw/nude"
PRIVATE_DIRECTORY = "ha_p/02_phw/nude"
BODY_MODEL = b"character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac"
UNDERWEAR = b"character/model/1_pc/2_phw/armor/38_underwear/phw_00_uw_00_0001.pac"


def build(registry_bytes, prefab, definition, appearances):
    if not prefab.startswith(b"\xff\xff\x04\0\0\0") or prefab.count(BODY_MODEL) != 1 or prefab.count(UNDERWEAR) != 1:
        raise ValueError("Supply the original game prefab with the shared Venus Body for Damiane model path and stock underwear.")
    root = ET.fromstring(definition)
    skeleton = root.find("SkeletonName")
    if root.tag != "NudePrefabData" or skeleton is None or skeleton.get("FileName", "").lower() != "1_pc/2_phw/phw_01.pab":
        raise ValueError("The original game body definition uses a different skeleton.")
    registry = read_registry(registry_bytes)
    rows = [row for row in registry["primary"] if row["name"].lower() == SHARED]
    lookups = [directory for name, directory in registry["secondary"] if name.lower() == SHARED]
    if len(rows) != 1 or rows[0]["directory"] != ORIGINAL_DIRECTORY or lookups != [ORIGINAL_DIRECTORY]:
        raise ValueError("The shared body must have both original registry entries.")
    if any(row["name"].lower() == PRIVATE for row in registry["primary"]) or any(name.lower() == PRIVATE for name, _ in registry["secondary"]):
        raise ValueError("The isolated body is already registered. Supply a registry without the isolation patch.")

    prefix = "0009/character/"
    files = {}
    for appearance, (body, boots) in sorted(appearances.items()):
        patch = ET.Element("xml-patch")
        for name, alias in [(body, PRIVATE), *((boot, PRIVATE_BOOTS[boot]) for boot in boots)]:
            ET.SubElement(patch, "set", {"find": "Prefab", "key": name, "attr": "Name", "value": alias})
        files["0009/" + appearance + ".patch"] = ET.tostring(patch, encoding="utf-8") + b"\n"
    files[prefix + "bin__/partprefabtable.pappt"] = write_registry({
        "header": registry["header"],
        "primary": [dict(rows[0], name=PRIVATE, directory=PRIVATE_DIRECTORY)],
        "secondary": [(PRIVATE, PRIVATE_DIRECTORY)],
    })
    files[prefix + "bin__/prefab/" + PRIVATE_DIRECTORY + "/" + PRIVATE + ".prefab"] = prefab
    files[prefix + "prefab/" + PRIVATE_DIRECTORY + "/" + PRIVATE + ".prefabdata_xml"] = definition
    return files


def main():
    parser = argparse.ArgumentParser(description="Build an optional witch-only prefab from original game files.")
    parser.add_argument("--game-prefab", type=Path, required=True)
    parser.add_argument("--game-definition", type=Path, required=True)
    parser.add_argument("--registry", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = Path(__file__).resolve().parents[1] / "build-data" / "witch-assets.json"
    appearances = {
        row["appearance"]: (row["body"]["Name"], sorted({part["prefab"] for part in row["parts"] if part["prefab"] in PRIVATE_BOOTS}))
        for row in json.loads(manifest.read_text(encoding="utf-8"))
    }
    files = build(args.registry.read_bytes(), args.game_prefab.read_bytes(), args.game_definition.read_bytes(), appearances)
    files["modinfo.json"] = (json.dumps({
        "title": "Hex Appeal - Witch Body Isolation",
        "author": "TAS-69",
        "description": "Keeps the witches on VENUS with their own game body prefab. Load before Hex Appeal [01] VENUS.",
    }, indent=2) + "\n").encode()
    with zipfile.ZipFile(args.output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            archive.writestr("Hex Appeal - Witch Body Isolation/" + name, data)
    with zipfile.ZipFile(args.output) as archive:
        if archive.testzip() is not None:
            raise ValueError("The isolation archive did not round trip.")
    print(json.dumps({"files": len(files), "witch_appearance_patches": len(appearances),
                      "sha256": hashlib.sha256(args.output.read_bytes()).hexdigest()}, indent=2))


if __name__ == "__main__":
    main()
