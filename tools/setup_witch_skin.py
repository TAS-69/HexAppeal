import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile

from build_venus_prefab_isolation import BODY_MODEL, ORIGINAL_DIRECTORY, PRIVATE, PRIVATE_DIRECTORY, SHARED, UNDERWEAR, build as isolate_prefab
from local_venus_skin_isolation import build as isolate_colour
from part_registry import read_registry, write_registry
from witch_parts import PRIVATE_BOOTS


ROOT = Path(__file__).resolve().parents[1]
APPEARANCES = {
    "cd_m0001_00_phw_north_44001": ("White Crow", "cd_mon/cd_m0001_00_phw_north_44001"),
    "cd_nhw_north_20026": ("Bari", "cd_npc_south/cd_nhw_south_20092"),
    "cd_nhw_south_20165": ("Areciel", "cd_npc_south/cd_nhw_south_20165"),
    "cd_nhw_south_20166": ("Lyselia", "cd_npc_south/cd_nhw_south_20166"),
    "cd_nhw_south_20167": ("Bari", "cd_npc_south/cd_nhw_south_20167"),
    "cd_nhw_south_20168": ("Elowen", "cd_npc_south/cd_nhw_south_20168"),
    "cd_nhw_south_20169": ("Sylvia", "cd_npc_south/cd_nhw_south_20169"),
}
CUSTOM_DIR = "character/descriptors/customization/"
PLAYER_CUSTOM = CUSTOM_DIR + "cd_pc/cd_phw_damian_customization.paccd_xml"
COLOURS = ("character/texture/phw_00_nude_00_0001.dds", "character/texture/phw_00_nude_00_0001_hand.dds")
PLAYER_APPEARANCE = "character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml"
HEAD_DIRECTORY = "1_pc/02_phw/head/head"
PRIVATE_HEAD_DIRECTORY = "ha_p/02_phw/head/head"
HEAD_NAME = re.compile(r"phw_[a-z0-9]{2}_head_[0-9]{2}_[0-9]{4}")
FACE_ROUTE = re.compile(r"character/texture/[a-z0-9_./-]+\.dds")
BRIGHTNESS = re.compile(rb'<Data\s+UIKey="skinBrightness"\s+Value="([0-9]{1,3})"\s*/>')


def routes_from(manifest):
    routes = {}
    version = False
    for line in manifest.read_text(encoding="utf-8-sig").splitlines():
        if line == "dmm-runtime\t1":
            version = True
        fields = line.split("\t")
        if len(fields) == 3 and fields[0] == "file":
            _, resource, path = fields
            resource = resource.replace("\\", "/").lower()
            source = (manifest.parent / path).resolve()
            if resource in routes and routes[resource] != source:
                raise ValueError(f"Conflicting mounted routes for {resource}")
            routes[resource] = source
    if not version:
        raise ValueError("Expected the active DMM runtime manifest.")
    return routes


def local_asset(folder, route):
    for path in (folder / "0009" / route, folder / route, folder / Path(route).name):
        if path.is_file():
            return path
    raise FileNotFoundError(f"Export the original {route} to {folder}.")


class OriginalAssets:
    def __init__(self, game, folder, toolchain):
        self.game = game
        self.folder = folder
        self.toolchain = toolchain
        self.index = None

    def read(self, route):
        if self.folder:
            try:
                return local_asset(self.folder, route).read_bytes()
            except FileNotFoundError:
                if self.toolchain is None:
                    raise
        if self.toolchain is None:
            raise FileNotFoundError(f"Supply --game-assets or --archive-toolchain to read the original {route}.")
        if self.index is None:
            spec = importlib.util.spec_from_file_location("hex_skin_archive_reader", self.toolchain / "cdumm/paz_parse.py")
            reader = importlib.util.module_from_spec(spec)
            sys.modules[spec.name] = reader
            sys.path[:0] = [str(self.toolchain / "python-deps"), str(self.toolchain / "crimsonforge")]
            spec.loader.exec_module(reader)
            from core.compression_engine import decompress
            from core.crypto_engine import decrypt

            self.index = {entry.path.casefold(): entry for entry in
                          reader.parse_pamt(str(self.game / "0009/0.pamt"), full_paths=True)}
            self.decrypt = decrypt
            self.decompress = decompress
        entry = self.index.get(route.casefold())
        if entry is None:
            raise FileNotFoundError(f"The game archive has no {route}. Supply an original export with --game-assets.")
        size = entry.comp_size if entry.compressed else entry.orig_size
        with open(entry.paz_file, "rb") as archive:
            archive.seek(entry.offset)
            data = archive.read(size)
        if len(data) != size:
            raise ValueError("Truncated game archive entry: " + route)
        if route.endswith(("_xml", ".pappt")):
            data = self.decrypt(data, Path(entry.path).name)
        if entry.compressed and entry.compression_type:
            try:
                data = self.decompress(data, entry.orig_size, entry.compression_type)
            except ValueError:
                if route != COLOURS[1] or entry.compression_type != 1 or data[:4] != b"DDS " or data[84:88] != b"DXT1":
                    raise
                packed, = struct.unpack_from("<I", data, 32)
                width, height = texture_layout(data)
                expected = ((width + 3) // 4) * ((height + 3) // 4) * 8
                if not 0 < packed < expected or len(data) - packed + expected != entry.orig_size:
                    raise
                top_mip = self.decompress(data[128:128 + packed], expected, 2)
                data = data[:128] + top_mip + data[128 + packed:]
        if route in COLOURS:
            texture_layout(data)
            data = data[:32] + bytes(44) + data[76:]
        return data


def appearance_rows(targets, routes, *, requires_venus=False):
    definitions = json.loads((ROOT / "build-data" / "witch-assets.json").read_text(encoding="utf-8"))
    appearances = {row["appearance"]: row for row in definitions}
    rows, skipped = [], []
    for route, original in sorted(appearances.items()):
        witch, descriptor = APPEARANCES[Path(route).stem]
        if targets and witch not in targets:
            continue
        path = routes.get(route.lower())
        if not path or not path.is_file():
            skipped.append(f"{witch} ({Path(route).stem}): no mounted appearance record.")
            continue
        try:
            appearance = ET.fromstring(path.read_bytes())
        except (OSError, ET.ParseError):
            skipped.append(f"{witch} ({Path(route).stem}): mounted appearance could not be read.")
            continue
        if requires_venus:
            bodies = appearance.findall("./Nude/Prefab")
            if len(bodies) != 1 or bodies[0].get("Name") not in (SHARED, PRIVATE):
                skipped.append(f"{witch} ({Path(route).stem}): another body owns this appearance.")
                continue
        else:
            settings = appearance.findall("./Customization")
            if len(settings) != 1 or settings[0].get("CustomizationFile") != descriptor + "_customization":
                skipped.append(f"{witch} ({Path(route).stem}): uses a different customization file.")
                continue
        boots = tuple(sorted({part["prefab"] for part in original["parts"] if part["prefab"] in PRIVATE_BOOTS}))
        rows.append((witch, route, (original["body"]["Name"], boots), descriptor))
    excluded_witches = {text.split(" (", 1)[0] for text in skipped}
    return [row for row in rows if row[0] not in excluded_witches], skipped


def brightness_field(data):
    decoration = data.find(b"<DecorationParam>")
    mesh = data.find(b"<MeshParam>", decoration)
    if decoration < 0 or mesh < 0 or not data.lstrip(b"\xef\xbb\xbf \n\r\t").startswith(b"<CharacterCustomization"):
        raise ValueError("Expected a Crimson Desert character customization file.")
    matches = list(BRIGHTNESS.finditer(data, decoration, mesh))
    if len(matches) != 1 or int(matches[0][1]) > 100:
        raise ValueError("Expected exactly one skinBrightness value from 0 to 100.")
    return matches[0]


def apply_brightness(data, brightness):
    if not 0 <= brightness <= 100:
        raise ValueError("Brightness must be from 0 to 100.")
    field = brightness_field(data)
    start, end = field.span(1)
    return data[:start] + str(brightness).encode("ascii") + data[end:]


def brightness_setup(args, routes):
    if args.brightness is None:
        source = args.preset or routes.get(PLAYER_CUSTOM)
        if not source or not source.is_file():
            raise FileNotFoundError("No mounted Damiane customization was found. Supply --preset or --brightness.")
        brightness = int(brightness_field(source.read_bytes())[1])
    else:
        brightness = args.brightness
    rows, skipped = appearance_rows(args.witch, routes)
    patches, unavailable = {}, set()
    for witch, _, _, descriptor in rows:
        resource = CUSTOM_DIR + descriptor + "_customization.paccd_xml"
        source = routes.get(resource)
        try:
            raw = args.assets.read(resource) if source is None else source.read_bytes()
            patches["0009/" + resource] = witch, apply_brightness(raw, brightness)
        except (OSError, ValueError) as error:
            unavailable.add(witch)
            skipped.append(f"{witch} ({descriptor}): {error}")
    files = {resource: data for resource, (witch, data) in patches.items() if witch not in unavailable}
    return files, skipped, {"brightness": brightness, "textures_copied": False}


def texture_layout(data):
    if len(data) < 128 or data[:4] != b"DDS " or struct.unpack_from("<I", data, 4)[0] != 124:
        raise ValueError("Expected a complete DDS texture header.")
    height, width = struct.unpack_from("<II", data, 12)
    if not height or not width or struct.unpack_from("<I", data, 112)[0]:
        raise ValueError("Expected a 2D skin texture.")
    if data[84:88] == b"DX10":
        if len(data) < 148 or struct.unpack_from("<III", data, 132) != (3, 0, 1):
            raise ValueError("Expected a single 2D skin texture.")
    return width, height


def active_asset(assets, routes, route):
    return routes[route].read_bytes() if route in routes else assets.read(route)


def appearance_head(data):
    appearance = ET.fromstring(data)
    heads = appearance.findall("./Head/Prefab")
    if len(heads) != 1:
        raise ValueError("The appearance has no single head.")
    head = heads[0].get("Name", "")
    if not HEAD_NAME.fullmatch(head.lower()):
        raise ValueError("The appearance does not use a checked PHW head.")
    return appearance, head


def face_colour(material):
    raw = material.removeprefix(b"\xef\xbb\xbf")
    if len(raw) > 1024 * 1024 or b"<!" in raw:
        raise ValueError("Unsupported head material.")
    fragment = ET.fromstring(b"<MaterialDocument>" + raw + b"</MaterialDocument>")
    slots = fragment.findall(".//Material[@_materialName='SkinnedMeshSkinWrinkle']//MaterialParameterTexture[@_name='_baseColorTexture']/ResourceReferencePath_ITexture")
    if len(slots) != 1:
        raise ValueError("The head material has no single skin colour slot.")
    source = slots[0].get("_path", "")
    if not FACE_ROUTE.fullmatch(source.lower()) or ".." in source.split("/"):
        raise ValueError("The skin colour resource is not a valid game DDS route.")
    return source


def face_setup(args, routes):
    get = lambda route: active_asset(args.assets, routes, route)
    _, player_head = appearance_head(get(PLAYER_APPEARANCE))

    def model_route(head):
        return "character/model/1_pc/2_phw/head/head/" + head.lower() + ".pac"

    def material_route(model):
        return model.replace("/model/", "/modelproperty/") + "_xml"

    source = args.face_route or face_colour(get(material_route(model_route(player_head))))
    if not FACE_ROUTE.fullmatch(source.lower()) or ".." in source.split("/"):
        raise ValueError("Enter a mounted character/texture/...dds resource path.")
    if source.lower() not in routes:
        raise ValueError("The selected custom face DDS resource is not mounted.")
    source_layout = texture_layout(get(source.lower()))
    registry_route = "character/bin__/partprefabtable.pappt"
    stock = read_registry(args.assets.read(registry_route))
    mounted = read_registry(get(registry_route))
    merged_rows = {row["name"].lower(): row for row in stock["primary"]}
    merged_rows.update({row["name"].lower(): row for row in mounted["primary"]})
    lookups = {name.lower(): route for name, route in stock["secondary"]}
    lookups.update({name.lower(): route for name, route in mounted["secondary"]})
    registry = {"header": stock["header"], "primary": [], "secondary": []}
    files, skipped, aliased = {}, [], {}
    definitions = json.loads((ROOT / "build-data/witch-assets.json").read_text(encoding="utf-8"))
    for definition in definitions:
        route = definition["appearance"]
        witch = APPEARANCES[Path(route).stem][0]
        if args.witch and witch not in args.witch:
            continue
        try:
            appearance, head = appearance_head(get(route))
            name = head.lower()
            if sum(part.get("Name") == head for part in appearance.iter("Prefab")) != 1:
                raise ValueError("The head selector is ambiguous.")
            if name not in aliased:
                row = merged_rows.get(name)
                if not row or row["directory"].lower() != HEAD_DIRECTORY or lookups.get(name, "").lower() != HEAD_DIRECTORY:
                    raise ValueError("The head does not have both expected prefab registrations.")
                alias = "hax" + name[3:]
                if alias in merged_rows or alias in lookups:
                    raise ValueError("Disable the previous local face match and remount before rebuilding.")
                prefab_route = f"character/bin__/prefab/{HEAD_DIRECTORY}/{name}.prefab"
                metadata_route = f"character/prefab/{HEAD_DIRECTORY}/{name}.prefabdata_xml"
                model = model_route(name)
                private_model = model.replace("/1_pc/", "/ha_p/").replace(name + ".", alias + ".")
                material = get(material_route(model))
                old_face = face_colour(material)
                width, height = texture_layout(get(old_face.lower()))
                if width * source_layout[1] != height * source_layout[0]:
                    raise ValueError("The source and target face colours use different aspect ratios.")
                attr = ('_path="' + old_face + '"').encode()
                if material.count(attr) != 1:
                    raise ValueError("The head colour binding is ambiguous.")
                material = material.replace(attr, ('_path="' + source + '"').encode())
                if face_colour(material) != source:
                    raise ValueError("The new head colour binding did not round trip.")
                prefab, model_data, metadata = get(prefab_route), get(model), get(metadata_route)
                if (not prefab.startswith(b"\xff\xff\x04\0\0\0") or prefab.count(model.encode()) != 1
                        or not model_data.startswith(b"PAR ") or ET.fromstring(metadata).tag != "HeadPrefabData"):
                    raise ValueError("The source head assets do not match the checked layout.")
                physics_model = model.replace("/model/", "/bin__/meshphysics/").removesuffix(".pac") + ".hkx"
                private_physics = private_model.replace("/model/", "/bin__/meshphysics/").removesuffix(".pac") + ".hkx"
                physics = get(physics_model)
                if physics[4:8] != b"TAG0":
                    raise ValueError("The head physics asset does not match the checked layout.")
                for resource, data in (
                    (prefab_route.replace(HEAD_DIRECTORY, PRIVATE_HEAD_DIRECTORY).replace(name + ".", alias + "."),
                     prefab.replace(model.encode(), private_model.encode())),
                    (metadata_route.replace(HEAD_DIRECTORY, PRIVATE_HEAD_DIRECTORY).replace(name + ".", alias + "."), metadata),
                    (private_model, model_data), (material_route(private_model), material), (private_physics, physics),
                ):
                    files["0009/" + resource] = data
                registry["primary"].append(dict(row, name=alias, directory=PRIVATE_HEAD_DIRECTORY))
                registry["secondary"].append((alias, PRIVATE_HEAD_DIRECTORY))
                aliased[name] = alias
            patch = ET.Element("xml-patch")
            ET.SubElement(patch, "set", {"target": f"//Prefab[@Name='{head}']", "attr": "Name", "value": aliased[name]})
            files["0009/" + route + ".patch"] = ET.tostring(patch, encoding="utf-8") + b"\n"
        except (OSError, ValueError, ET.ParseError) as error:
            skipped.append(f"{witch} ({Path(route).stem}): {error}")
    if files:
        files["0009/" + registry_route] = write_registry(registry)
    return files, skipped, {"face_colour_route": source, "private_heads": len(aliased), "textures_copied": False}


def textures_setup(args, routes):
    rows, skipped = appearance_rows(args.witch, routes, requires_venus=True)
    if not rows:
        return {}, skipped, {}
    prefab = f"character/bin__/prefab/{ORIGINAL_DIRECTORY}/{SHARED}.prefab"
    definition = f"character/prefab/{ORIGINAL_DIRECTORY}/{SHARED}.prefabdata_xml"
    base = isolate_prefab(args.assets.read("character/bin__/partprefabtable.pappt"), args.assets.read(prefab),
                          args.assets.read(definition), {route: original_parts for _, route, original_parts, _ in rows})
    if args.mode == "body-only":
        return base, skipped, {"preserves_external_venus_route": True, "textures_copied": False}
    body_route = BODY_MODEL.decode()
    physics_route = body_route.replace("/model/", "/bin__/meshphysics/").removesuffix(".pac") + ".hkx"
    model = args.venus_model or routes.get(body_route)
    if not model or not model.is_file():
        raise FileNotFoundError("Mount Venus Body for Damiane (Human or Elf Body) in DMM, or supply the installed --venus-model.")
    model_from_mount = routes.get(body_route)
    model_data = model.read_bytes()
    if model_from_mount and model_from_mount.is_file() and model != model_from_mount and model_data != model_from_mount.read_bytes():
        raise ValueError("The selected Venus Body for Damiane model differs from the model in DMM's active file list.")
    physics = args.venus_physics or routes.get(physics_route)
    physics_data = args.assets.read(physics_route) if physics is None else physics.read_bytes()
    stock = {route: args.assets.read(route) for route in COLOURS}
    colours = stock
    if args.mode == "custom-textures":
        selected = (args.body_texture or routes.get(COLOURS[0]), args.hand_texture or routes.get(COLOURS[1]))
        if not any(selected):
            raise FileNotFoundError("No mounted body or hand DDS was found. Supply --body-texture or --hand-texture.")
        colours = dict(stock)
        for route, path in zip(COLOURS, selected):
            if path:
                chosen = path.read_bytes()
                width, height = texture_layout(chosen)
                original_width, original_height = texture_layout(stock[route])
                if width * original_height != height * original_width:
                    raise ValueError(f"{path.name} has a different aspect ratio from the original {Path(route).name}.")
                colours[route] = chosen
    material_route = body_route.replace("/model/", "/modelproperty/") + "_xml"
    files = isolate_colour(base, model_data, physics_data, args.assets.read(material_route), colours)
    return files, skipped, {"model": str(model), "body_and_hand_dds": args.mode}


def write_local_zip(output, files, title):
    if output.suffix.casefold() != ".zip" or "local" not in output.stem.casefold() or output.is_relative_to(ROOT):
        raise ValueError("Choose a .zip outside the project with 'local' in its filename.")
    metadata = {"title": title, "author": "TAS-69", "description": "Generated for this local install. Keep this archive private."}
    with output.open("x+b") as destination:
        try:
            with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                for route, data in files.items():
                    archive.writestr(title + "/" + route, data)
                archive.writestr(title + "/modinfo.json", json.dumps(metadata) + "\n")
            destination.seek(0)
            with zipfile.ZipFile(destination) as archive:
                if archive.testzip() is not None:
                    raise ValueError("The generated ZIP failed its integrity check.")
        except Exception:
            destination.close()
            output.unlink()
            raise


def bind_refitted_underwear(files, model, assets):
    original = UNDERWEAR.decode()
    isolated = original.replace("/1_pc/", "/ha_p/").replace("/phw_", "/hva_")
    prefab = f"0009/character/bin__/prefab/{PRIVATE_DIRECTORY}/{PRIVATE}.prefab"
    source = files.get(prefab, b"")
    if len(isolated) != len(original) or source.count(UNDERWEAR) != 1:
        raise ValueError("This setup needs the stock underwear slot in the isolated witch prefab.")
    data = model.read_bytes()
    if not data.startswith(b"PAR ") or len(data) < 256:
        raise ValueError("Expected a complete underwear PAC fitted to Venus Body for Damiane.")
    material = original.replace("/model/", "/modelproperty/") + "_xml"
    physics = original.replace("/model/", "/bin__/meshphysics/").removesuffix(".pac") + ".hkx"
    files["0009/" + isolated] = data
    files["0009/" + isolated.replace("/model/", "/modelproperty/") + "_xml"] = assets.read(material)
    files["0009/" + isolated.replace("/model/", "/bin__/meshphysics/").removesuffix(".pac") + ".hkx"] = assets.read(physics)
    files[prefab] = source.replace(UNDERWEAR, isolated.encode("ascii"))


def main():
    parser = argparse.ArgumentParser(description="Create a private skin setup for Hex Appeal from your own install.")
    parser.add_argument("mode", choices=("brightness", "body-only", "stock-textures", "custom-textures", "match-face"))
    parser.add_argument("--game", type=Path, default=Path(r"C:\Program Files (x86)\Steam\steamapps\common\Crimson Desert"))
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--game-assets", type=Path, help="Folder with original assets exported from the game archives.")
    parser.add_argument("--archive-toolchain", type=Path, help="Local archive tools folder with cdumm/paz_parse.py and crimsonforge/core.")
    parser.add_argument("--witch", action="append", choices=sorted({name for name, _ in APPEARANCES.values()}))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--status", action="store_true", help="Write a sibling status file for the Hex Appeal Settings panel.")
    brightness = parser.add_mutually_exclusive_group()
    brightness.add_argument("--preset", type=Path, help="Select the exported Damiane .paccd_xml manually.")
    brightness.add_argument("--brightness", type=int, choices=range(101), metavar="0-100")
    parser.add_argument("--body-texture", type=Path)
    parser.add_argument("--hand-texture", type=Path)
    parser.add_argument("--face-route", help="Override auto-detection with a mounted character/texture/...dds route.")
    parser.add_argument("--venus-model", type=Path)
    parser.add_argument("--venus-physics", type=Path)
    parser.add_argument("--underwear-model", type=Path, help="Use a locally refitted stock underwear PAC with a VENUS isolation mode.")
    args = parser.parse_args()
    if args.underwear_model and args.mode in ("brightness", "match-face"):
        parser.error("Use a VENUS isolation mode for the underwear model.")
    args.game = args.game.resolve()
    configured_tools = os.environ.get("HEX_APPEAL_ARCHIVE_TOOLS")
    tools = args.archive_toolchain or (Path(configured_tools) if configured_tools else None)
    args.assets = OriginalAssets(args.game, args.game_assets, tools)
    output = args.output.resolve()
    if output.is_relative_to(args.game):
        parser.error("Save the output outside your game folder.")
    try:
        routes = routes_from(args.manifest or args.game / "bin64/dmm/manifest.tsv")
        setup = {"brightness": brightness_setup, "match-face": face_setup}.get(args.mode, textures_setup)
        files, skipped, settings = setup(args, routes)
        if not files:
            raise ValueError("No eligible witches. " + " ".join(skipped))
        if args.underwear_model:
            bind_refitted_underwear(files, args.underwear_model, args.assets)
            settings["private_refitted_underwear"] = True
        label = {"brightness": "Brightness", "body-only": "Body Isolation", "match-face": "Face Match"}.get(args.mode, "Skin Isolation")
        title = "Hex Appeal - Witch " + label + " (local)"
        write_local_zip(output, files, title)
    except (OSError, ValueError, ImportError, ET.ParseError) as error:
        if args.status:
            output.with_suffix(".status.txt").write_text(str(error) + "\n", encoding="utf-8")
        parser.error(str(error))
    if args.status:
        detail = f"Prepared {len(files)} local files."
        if skipped:
            detail += "\n" + "\n".join(skipped)
        output.with_suffix(".status.txt").write_text(detail + "\n", encoding="utf-8")
    print(json.dumps({"archive": str(output), "files": len(files), "settings": settings, "skipped": skipped}, indent=2))


if __name__ == "__main__":
    main()
