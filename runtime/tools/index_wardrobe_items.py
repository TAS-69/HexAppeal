from argparse import ArgumentParser
import json
from pathlib import Path
from types import SimpleNamespace
import re
import struct
import sys


def virtual_path(value):
    return value.lower().replace(chr(92), "/").lstrip("/")


def mount_sources(game):
    directory = game / "bin64/dmm"
    manifest, mod_items = directory / "manifest.tsv", directory / "mod_items.json"
    files, packages = {}, set()
    if manifest.is_file():
        for line in manifest.read_text(encoding="utf-8-sig").splitlines():
            columns = line.split("\t")
            if len(columns) != 3 or columns[0] != "file":
                continue
            source = Path(columns[2])
            files[virtual_path(columns[1])] = source
            parents = source.parents
            for parent in parents:
                if parent.name.casefold() == "files" and parent.parent.joinpath("mod.json").is_file():
                    packages.add(parent.parent)
                    break
                if parent.parent.name.casefold() == "mods" and parent.is_dir():
                    packages.add(parent)
                    break
    registered_items = json.loads(mod_items.read_text(encoding="utf-8-sig")) if mod_items.is_file() else {}
    return files, packages, registered_items


def declared_bodies(package):
    data = package / "mod.json"
    details = json.loads(data.read_text(encoding="utf-8-sig")).get("modinfo", {}) if data.is_file() else {}
    text = " ".join((package.name, details.get("title", ""), details.get("description", ""))).casefold()
    return set(re.findall(r"\b(venus|vanilla)\b", text.replace("_", " ")))


def fields(rule):
    return {patch["path"]: patch.get("new") for patch in rule.get("patches", ()) if "path" in patch}


def mounted_items(packages, registered, body):
    for package in sorted(packages):
        try:
            if body not in declared_bodies(package):
                continue
        except (ValueError, OSError, TypeError):
            continue
        for path in package.glob("*.field.json"):
            try:
                if path.stat().st_size > 8 * 1024 * 1024:
                    continue
                definition = json.loads(path.read_text(encoding="utf-8-sig"))
                title = definition.get("modinfo", {}).get("title", "")
                targets = {target.get("file", "").casefold(): target.get("intents", ()) for target in definition.get("targets", ())}
                names = {str(row["entry"]): row.get("new") for row in targets.get("localizationstring_eng.paloc", ()) if "entry" in row}
                strings = {row.get("new_key"): fields(row).get("buffer") for row in targets.get("stringinfo.pabgb", ())}
                for row in targets.get("iteminfo.pabgb", ()):
                    item = row.get("new_key")
                    if row.get("op") == "clone_record" and isinstance(item, int) and registered.get(str(item)) == title:
                        yield item, fields(row), names, strings, title
            except (ValueError, OSError, TypeError, AttributeError):
                continue


def valid_label(name):
    return isinstance(name, str) and 0 < len(name.encode("utf-8")) <= 200 and not any(ch in name for ch in "\t\r\n<>")


def main():
    parser = ArgumentParser(description="Index local Crimson Desert wardrobe names and icons")
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--crimsonforge", type=Path, required=True)
    parser.add_argument("--cdumm", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--body", choices=("vanilla", "venus"), default="venus")
    parser.add_argument("--stock-only", action="store_true")
    args = parser.parse_args()
    sys.path[:0] = [str(args.crimsonforge.resolve()), str((args.cdumm / "src").resolve())]
    from cdumm.archive.paz_parse import parse_pamt
    from core.crypto_engine import hashlittle
    from core.dds_reader import decode_dds_to_rgba
    from core.item_index import parse_localization
    from core.paloc_parser import parse_paloc
    from core.pamt_parser import PamtFileEntry
    from core.vfs_manager import VfsManager
    from PIL import Image
    from lz4.block import decompress

    class Archive(VfsManager):
        def load_pamt(self, group):
            if group not in self._pamt_cache:
                index = args.game / group / "0.pamt"
                entries = parse_pamt(str(index), full_paths=True)
                files = [PamtFileEntry(x.path, x.paz_file, x.offset, x.comp_size, x.orig_size, x.flags, x.paz_index) for x in entries]
                self._pamt_cache[group] = SimpleNamespace(file_entries=files)
            return self._pamt_cache[group]

    archive = Archive(str(args.game))
    mounted = args.game / "bin64/dmm/files"
    served, packages, registered = mount_sources(args.game)
    table = "gamedata/binarystaticinfo__/bin/iteminfo.staticinfo"
    tables = [mounted / (table + suffix) for suffix in ("body", "header")]
    if all(path.is_file() for path in tables):
        body, header = [path.read_bytes() for path in tables]
    else:
        entries = {x.path.lower(): x for x in archive.load_pamt("0008").file_entries}
        body, header = [archive.read_entry_data(entries[table + suffix]) for suffix in ("body", "header")]

    count, = struct.unpack_from("<H", header)
    if len(header) != 2 + count * 8:
        raise ValueError("The item table uses a different index layout")
    offsets = sorted((offset, key) for key, offset in struct.iter_unpack("<II", header[2:]))
    if not offsets or offsets[0][0] != 0 or offsets[-1][0] >= len(body):
        raise ValueError("The item table index does not match the data")
    markers = {
        "_hel_": ("Headwear", "helmet hat headgear"),
        "_ub_": ("Upper body", "armour armor chest coat dress"),
        "_lb_": ("Lower body", "pants trousers skirt"),
        "_hand_": ("Hands", "gloves gauntlet"),
        "_foot_": ("Feet", "shoes boot boots footwear"),
        "_cloak_": ("Cloak", "cape cloak"),
        "_shoulder_": ("Shoulders", "shoulder pauldron"),
        "_belt_": ("Waist", "belt"),
        "_necklace_": ("Accessories", "necklace jewellery jewelry"),
        "_ring_": ("Accessories", "ring jewellery jewelry"),
        "_earring_": ("Accessories", "earring jewellery jewelry"),
    }
    models = {}
    for path in {virtual_path(row.path) for row in archive.load_pamt("0009").file_entries} | set(served):
        if not path.endswith(".prefab") or "/02_phw/" not in path:
            continue
        name = path.rsplit("/", 1)[-1].removesuffix(".prefab")
        for marker, slot in markers.items():
            if marker in name:
                key = hashlittle(name.encode("ascii"), 0xC5EDE)
                models.setdefault(key, set()).add((name, *slot))
                break
    icons = {}
    for row in archive.load_pamt("0012").file_entries:
        path = virtual_path(row.path)
        if path.startswith("ui/texture/icon/") and path.endswith(".dds"):
            icons[path.rsplit("/", 1)[-1].removesuffix(".dds")] = row
    loc = next((row for row in archive.load_pamt("0020").file_entries if row.path.endswith("/eng/item.paloc")), None)
    if loc:
        raw = archive.read_entry_data(loc)
        if raw.startswith(b"paloc"):
            packed, plain = struct.unpack_from("<II", raw, 9)
            if len(raw) != 512 + packed or not 0 < plain < 96 * 1024 * 1024:
                raise ValueError("Invalid item-name table size")
            raw = decompress(raw[512:], uncompressed_size=plain)
        local = {entry.key: entry.value for entry in parse_paloc(raw)}
    else:
        local = parse_localization(archive)
    mounted_icons = {Path(path).stem: source for path, source in served.items()
                     if path.startswith("ui/texture/icon/") and path.endswith(".dds") and source.is_file()}
    incompatible = set()
    for path, source in served.items():
        if not path.endswith((".prefab", ".pac")):
            continue
        for package in packages.intersection(source.parents):
            try:
                declared = declared_bodies(package)
            except (ValueError, OSError, TypeError):
                continue
            if declared and args.body not in declared:
                incompatible.add(Path(path).stem)
    icon_out = args.output / "icons"
    icon_out.mkdir(parents=True, exist_ok=True)
    rows, icon_count, mod_count = {}, 0, 0

    def add_item(key, name, matched, tags, icon_names, require_icon=False):
        nonlocal icon_count
        parts = sorted(part for part, _, _ in matched)
        slots = {match[1] for match in matched}
        families = {match[2] for match in matched}
        if not valid_label(name) or len(slots) != 1 or len(families) != 1 or len(parts) > 8 or incompatible.intersection(parts):
            return False
        icon_path = icon_out / f"{key}.rgba"
        icon_path.unlink(missing_ok=True)
        found_icon = False
        for candidate in icon_names:
            if candidate in mounted_icons:
                raw = mounted_icons[candidate].read_bytes()
            elif candidate in icons:
                asset = icons[candidate]
                loose = mounted / asset.path
                raw = loose.read_bytes() if loose.is_file() else archive.read_entry_data(asset)
            else:
                continue
            try:
                width,height,rgba = decode_dds_to_rgba(raw)
                image = Image.frombytes("RGBA", (width, height), rgba).resize((32,32),Image.Resampling.LANCZOS)
                icon_path.write_bytes(image.tobytes())
                found_icon = True
                icon_count += 1
                break
            except (OSError, ValueError, NotImplementedError):
                continue
        if require_icon and not found_icon:
            return False
        rows[key] = (name.casefold(), f"{key}\t{name}\t{slots.pop()}\t{families.pop()} {tags}\t{','.join(parts)}")
        return True

    for i, (offset, key) in enumerate(offsets):
        end = offsets[i + 1][0] if i+1 < len(offsets) else len(body)
        item = body[offset:end]
        length, = struct.unpack_from("<I", item)
        if length > 200 or 8 + length >= len(item) or struct.unpack_from("<I", item, length+4)[0] != key:
            raise ValueError(f"Invalid item record at {offset}")
        internal = item[4:4+length].decode("ascii", errors="replace")
        localized = re.search(rb"[0-9]{9,20}", item[length+8:length+210])
        name = local.get(localized.group().decode("ascii"), "") if localized else ""
        if str(key) in registered or not valid_label(name):
            continue
        matched = set()
        for at in range(length+20,len(item)-3):
            candidates = models.get(struct.unpack_from("<I", item, at)[0], ())
            if len(candidates) == 1:
                matched.update(candidates)
        targets = ["itemicon_" + internal.lower(), *("itemicon_prefab_" + part for part, _, _ in sorted(matched))]
        add_item(key, name, matched, internal, targets)
    for key, item, names, strings, title in (() if args.stock_only else mounted_items(packages, registered, args.body)):
        name_record = item.get("item_name") or {}
        name = names.get(str(name_record.get("index")))
        if not valid_label(name):
            continue
        entries = (item.get("prefab_data_list") or ())
        matched = {part for row in entries if row.get("prefab_data_type", 0) == 0
                   for key_hash in row.get("prefab_names", ()) for part in models.get(key_hash, ())}
        targets = [strings.get(icon.get("icon_path")) for icon in (item.get("item_icon_list") or ())]
        targets = [Path(virtual_path(icon)).stem for icon in targets if isinstance(icon, str)]
        tags = " ".join(f"{title} {item.get('string_key', '')}".split())
        if add_item(key, name, matched, tags, targets, require_icon=True):
            mod_count += 1
    if not rows:
        raise ValueError("No named outfit items matched local appearance records")
    temp = args.output / "items.next.tsv"
    temp.write_text("HexAppealItems 1\n"+"\n".join(row for _,row in sorted(rows.values()))+"\n",encoding="utf-8",newline="\n")
    temp.replace(args.output / "items.tsv")
    print(f"Indexed {len(rows)} named wardrobe items, {mod_count} mounted mod items and {icon_count} local icons for {args.body}")


if __name__ == "__main__":
    main()
