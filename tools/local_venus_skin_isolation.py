import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile

from build_venus_prefab_isolation import BODY_MODEL, PRIVATE, PRIVATE_DIRECTORY


def build(base, model, physics, material, original_colours):
    packed_physics = physics.startswith(b"\x57\xe0\xe0\x57\x10\xc0\xc0\x10")
    tagged_physics = physics.startswith(b"TAG0SDKV") or physics[4:8] == b"TAG0"
    if model[:4] != b"PAR " or not (packed_physics or tagged_physics):
        raise ValueError("Supply the currently mounted VENUS model and its matching physics.")
    colour_paths = {
        f"character/texture/{name}.dds": f"character/texture/hva_{name[4:]}.dds"
        for name in ("phw_00_nude_00_0001", "phw_00_nude_00_0001_hand")
    }
    if set(original_colours) != set(colour_paths) or any(data[:4] != b"DDS " for data in original_colours.values()):
        raise ValueError("Supply the original body and hand DDS from the game archives.")
    body_path = BODY_MODEL.decode()
    private = f"character/model/ha_p/2_phw/nude/{PRIVATE}.pac"
    prefab_name = f"0009/character/bin__/prefab/{PRIVATE_DIRECTORY}/{PRIVATE}.prefab"
    prefab = base[prefab_name]
    if prefab.count(BODY_MODEL) != 1 or len(private) != len(body_path):
        raise ValueError("The base isolation prefab does not contain the supported shared model path.")
    original = ET.fromstring("<Root>" + material.decode("utf-8-sig") + "</Root>")
    changed = material
    for before, after in colour_paths.items():
        refs = [
            node.get("_path")
            for colour in original.iter("MaterialParameterTexture")
            if colour.get("_name") == "_baseColorTexture"
            for node in colour
        ]
        if refs.count(before) != 1 or changed.count(before.encode()) != 1:
            raise ValueError("The game material has an unexpected base-colour binding: " + before)
        changed = changed.replace(before.encode(), after.encode())
    ET.fromstring("<Root>" + changed.decode("utf-8-sig") + "</Root>")
    result = dict(base)
    result[prefab_name] = prefab.replace(BODY_MODEL, private.encode())
    result["0009/" + private] = model
    result["0009/" + private.replace("/model/", "/modelproperty/") + "_xml"] = changed
    result["0009/" + private.replace("/model/", "/bin__/meshphysics/").removesuffix(".pac") + ".hkx"] = physics
    for source, destination in colour_paths.items():
        result["0009/" + destination] = original_colours[source]
    return result


def main():
    parser = argparse.ArgumentParser(description="Create a local VENUS skin isolation trial from this installation. The output is for local testing only.")
    parser.add_argument("--body-isolation", type=Path, required=True)
    parser.add_argument("--venus-model", type=Path, required=True)
    parser.add_argument("--venus-physics", type=Path, required=True)
    parser.add_argument("--game-assets", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    project = Path(__file__).resolve().parents[1]
    if output.is_relative_to(project) or "local" not in output.stem.lower():
        parser.error("Put the local trial ZIP outside the project and include 'local' in its name.")
    source_dir = "Hex Appeal - Witch Body Isolation/"
    with zipfile.ZipFile(args.body_isolation) as archive:
        base = {name.removeprefix(source_dir): archive.read(name) for name in archive.namelist() if name.startswith(source_dir)}
    colours = {
        "character/texture/" + name: (args.game_assets / name).read_bytes()
        for name in ("phw_00_nude_00_0001.dds", "phw_00_nude_00_0001_hand.dds")
    }
    model, physics = args.venus_model.read_bytes(), args.venus_physics.read_bytes()
    result = build(base, model, physics, (args.game_assets / "phw_00_nude_00_0001_damian.pac_xml").read_bytes(), colours)
    title = "Hex Appeal - Witch Skin Isolation (local)"
    result["modinfo.json"] = (json.dumps({"title": title, "author": "TAS-69", "description": "Local skin routing trial generated from this installation."}) + "\n").encode()
    with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in result.items():
            archive.writestr(title + "/" + name, data)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise ValueError("The local isolation archive did not round trip.")
    print(json.dumps({"scope": "local only", "files": len(result), "venus_sha256": hashlib.sha256(model).hexdigest(),
                      "zip_sha256": hashlib.sha256(output.read_bytes()).hexdigest()}, indent=2))


if __name__ == "__main__":
    main()
