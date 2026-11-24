"""Shape-follow skin transfer for witch garments.

Hex Appeal shapes bodies through Instant Surgery's pose deformation: the
shaping moves skeleton bones, and every mesh follows those bones only as far
as its skin weights allow. Garments that are weighted only to the main bones
do not follow breast, seat, hip or thigh shaping, so the shaped body pushes
through them.

This tool gives garment vertices near the body the body's own skinning,
blended out with distance, so the garment follows the live shaped
proportions in both body options. Only the skin bytes of affected vertex
records change: positions, normals, UVs, palettes, indices, LODs and file size
stay as they are. Bones a garment's palette does not contain are folded into
their nearest ancestor that it does contain, so no structure changes.

It runs locally on game-derived inputs and writes fresh files only. Outputs
contain Pearl Abyss content and are not for redistribution.
"""
from __future__ import annotations

from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
import sys

WEIGHT_TOTAL = 255
# Original active weights sum to 253..257 after rounding. A record outside
# this range is left untouched.
SUM_RANGE = (240, 270)
# Bytes this tool may change: the packed slots and the weights. Bytes 12-15
# are a second texture coordinate and byte 39 holds the influence count; both
# are never written.
SKIN_BYTE_OFFSETS = tuple(range(20, 36))


# ---------------------------------------------------------------------------
# Pure helpers (no game toolchain needed; covered by tools/tests).
#
# PAC vertex record skin layout, as the game's skinning shader reads it:
#   bytes 20-27  six 10-bit palette slots (two u32, three slots each)
#   bytes 28-35  u8 weights; only the first N are read
#   byte  39     low six bits == 63 selects N = 6, otherwise N = 4
# Unread slots and weights hold unrelated data and are preserved.

def active_influences(record: bytes) -> int:
    if len(record) < 40:
        raise ValueError('vertex record is too short for skin data')
    return 6 if (record[39] & 0x3F) == 63 else 4


def _slots(record: bytes) -> list[int]:
    low, high = struct.unpack_from('<II', record, 20)
    return [low & 0x3FF, (low >> 10) & 0x3FF, (low >> 20) & 0x3FF,
            high & 0x3FF, (high >> 10) & 0x3FF, (high >> 20) & 0x3FF]


def decode_skin(record: bytes) -> tuple[list[int], list[int]]:
    """Return the active palette slots and their u8 weights."""
    count = active_influences(record)
    return _slots(record)[:count], list(record[28:28 + count])


def encode_skin(record: bytearray, slots: list[int], weights: list[int]) -> None:
    """Write the active slots and weights; every other bit stays as found."""
    count = active_influences(record)
    if len(slots) != count or len(weights) != count:
        raise ValueError(f'this record takes exactly {count} slots and weights')
    if any(not 0 <= s < 1024 for s in slots) or any(not 0 <= w <= 255 for w in weights):
        raise ValueError('slot or weight out of range')
    packed = _slots(record)
    packed[:count] = slots
    low = struct.unpack_from('<I', record, 20)[0] & 0xC0000000
    high = struct.unpack_from('<I', record, 24)[0] & 0xC0000000
    low |= packed[0] | (packed[1] << 10) | (packed[2] << 20)
    high |= packed[3] | (packed[4] << 10) | (packed[5] << 20)
    struct.pack_into('<II', record, 20, low, high)
    record[28:28 + count] = bytes(weights)


def quantize(weights: dict[int, float], limit: int, total: int = WEIGHT_TOTAL) -> list[tuple[int, int]]:
    """Keep the strongest influences and round them to integers summing to total."""
    strongest = sorted(((w, k) for k, w in weights.items() if w > 0), reverse=True)[:limit]
    mass = sum(w for w, _ in strongest)
    if mass <= 0:
        raise ValueError('no positive weight to quantize')
    scaled = [(w / mass * total, k) for w, k in strongest]
    floors = [(int(math.floor(v)), v - math.floor(v), k) for v, k in scaled]
    remainder = total - sum(f for f, _, _ in floors)
    order = sorted(range(len(floors)), key=lambda i: -floors[i][1])
    result = {k: f for f, _, k in floors}
    for i in order[:remainder]:
        result[floors[i][2]] += 1
    return sorted(((k, w) for k, w in result.items() if w > 0), key=lambda item: -item[1])


def fold_to_palette(weights: dict[int, float], allowed: set[int], parent: list[int]) -> dict[int, float]:
    """Move weight on bones outside 'allowed' to their nearest allowed ancestor.

    Weight whose bone has no allowed ancestor is dropped; the caller
    renormalises. Returns a new mapping.
    """
    folded: dict[int, float] = {}
    for bone, weight in weights.items():
        target = bone
        while target >= 0 and target not in allowed:
            target = parent[target]
        if target >= 0:
            folded[target] = folded.get(target, 0.0) + weight
    return folded


def blend_amount(distance: float, full: float, none: float) -> float:
    """1 at or inside 'full' metres from the body surface, 0 from 'none' outwards."""
    distance = abs(distance)
    if distance <= full:
        return 1.0
    if distance >= none:
        return 0.0
    return (none - distance) / (none - full)


def blend(garment: dict[int, float], body: dict[int, float], amount: float) -> dict[int, float]:
    result = {k: (1.0 - amount) * w for k, w in garment.items()}
    for k, w in body.items():
        result[k] = result.get(k, 0.0) + amount * w
    return result


def smooth(weights: list[dict[int, float]], neighbours: list[set[int]], movable: list[bool],
           iterations: int) -> list[dict[int, float]]:
    """Average each movable vertex's weights with its mesh neighbours.

    Vertices that are not movable keep their weights and anchor the
    smoothing, so the blend into untouched garment areas stays gradual.
    """
    current = [dict(w) for w in weights]
    for _ in range(iterations):
        following = []
        for i, own in enumerate(current):
            if not movable[i] or not neighbours[i]:
                following.append(own)
                continue
            mixed = {k: 0.5 * w for k, w in own.items()}
            share = 0.5 / len(neighbours[i])
            for j in neighbours[i]:
                for k, w in current[j].items():
                    mixed[k] = mixed.get(k, 0.0) + share * w
            following.append(mixed)
        current = following
    return current


# ---------------------------------------------------------------------------
# Mesh IO and geometry (needs numpy, scipy and the CrimsonForge toolchain).

def _toolchain():
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from development_paths import use_toolchain
    use_toolchain()
    import logging
    logging.disable(logging.CRITICAL)


def _load_pac(path: Path) -> bytes:
    from core.compression_engine import _decompress_type1_par
    data = path.read_bytes()
    if data[:8] != b'PAR \x06\x01\x00\x00':
        data = _decompress_type1_par(data)
    return data


def _palette(data: bytes, hashes: dict[int, int]) -> list[int]:
    from core.mesh_parser import _scan_pac_skin_palette
    palette = _scan_pac_skin_palette(data, set(hashes))
    if palette:
        return list(palette)
    runs, offset = [], 0
    while offset + 4 <= len(data):
        values, cursor = [], offset
        while cursor + 4 <= len(data):
            value = int.from_bytes(data[cursor:cursor + 3], 'little')
            if value not in hashes:
                break
            values.append(value)
            cursor += 4
        if values:
            runs.append(tuple(values))
            offset = cursor
        else:
            offset += 1
    longest = max(map(len, runs), default=0)
    candidates = {run for run in runs if len(run) == longest}
    if not longest or len(candidates) != 1:
        raise ValueError('the skin palette could not be identified unambiguously')
    return list(candidates.pop())


class Mesh:
    """Decoded skinned PAC: per-layout positions, raw skin and faces."""

    def __init__(self, path: Path, rig: dict):
        import numpy as np
        from core.mesh_parser import _decode_pac_vertex_record
        from pac_lods import lod_layout
        self.path = Path(path)
        self.data = _load_pac(self.path)
        self.descriptors, self.layouts = lod_layout(self.data)
        hashes = {bone['hash24']: i for i, bone in enumerate(rig['bones'])}
        self.palette = [hashes[h] for h in _palette(self.data, hashes)]
        self.parts = []
        for layout in self.layouts:
            if layout['stride'] != 40:
                raise ValueError(f'unsupported vertex stride {layout["stride"]}')
            positions, skins = [], []
            for i in range(layout['vc']):
                offset = layout['voff'] + i * 40
                record = self.data[offset:offset + 40]
                descriptor = self.descriptors[layout['di']]
                decoded = _decode_pac_vertex_record(self.data, offset, descriptor)
                slots, weights = decode_skin(record)
                # Cross-check with CrimsonForge's shader-verified decoder,
                # including its per-descriptor slot mapping.
                mapped = [descriptor.palette[s] if s < len(descriptor.palette) else s
                          for s, w in zip(slots, weights) if w]
                if tuple(mapped) != tuple(decoded[3]):
                    raise AssertionError('skin decoding disagrees with CrimsonForge')
                positions.append(decoded[0])
                skins.append((slots, weights, descriptor.palette))
            faces = np.asarray(struct.unpack_from('<' + 'H' * layout['ic'], self.data, layout['ioff'])).reshape(-1, 3)
            self.parts.append({'layout': layout, 'p': np.asarray(positions, float).reshape(-1, 3),
                               'skin': skins, 'faces': faces})

    def slot_bone(self, slot, descriptor_palette):
        index = descriptor_palette[slot] if slot < len(descriptor_palette) else slot
        return self.palette[index] if index < len(self.palette) else None

    def weights_of(self, slots, weights, descriptor_palette) -> dict[int, float]:
        total = sum(weights)
        result: dict[int, float] = {}
        for slot, weight in zip(slots, weights):
            bone = self.slot_bone(slot, descriptor_palette) if weight else None
            if bone is not None:
                result[bone] = result.get(bone, 0.0) + weight / total
        return result

    def lod0(self):
        import numpy as np
        parts = [p for p in self.parts if p['layout']['lod'] == 0 and len(p['p'])]
        points, faces, influences, base = [], [], [], 0
        for part in parts:
            points.append(part['p'])
            faces.append(part['faces'] + base)
            influences.extend(self.weights_of(*skin) for skin in part['skin'])
            base += len(part['p'])
        return np.concatenate(points), np.concatenate(faces), influences


class Surface:
    """Signed distance to the nearest triangle (positive along its normal)."""

    def __init__(self, points, faces):
        from scipy.spatial import cKDTree
        self.triangles = points[faces]
        self.tree = cKDTree(self.triangles.mean(1))

    def distance(self, points):
        import numpy as np
        result = []
        for start in range(0, len(points), 256):
            values = np.asarray(points[start:start + 256])
            _, ids = self.tree.query(values, k=min(64, len(self.triangles)))
            t = self.triangles[ids]
            p = values[:, None, :]
            a, b, c = t[:, :, 0], t[:, :, 1], t[:, :, 2]
            ab, ac = b - a, c - a
            normal = np.cross(ab, ac)
            normal = normal / np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1e-15)
            projection = p - np.sum((p - a) * normal, axis=2, keepdims=True) * normal
            v = projection - a
            d00, d01, d11 = np.sum(ab * ab, 2), np.sum(ab * ac, 2), np.sum(ac * ac, 2)
            d20, d21 = np.sum(v * ab, 2), np.sum(v * ac, 2)
            den = d00 * d11 - d01 * d01
            u = (d11 * d20 - d01 * d21) / np.maximum(den, 1e-20)
            w = (d00 * d21 - d01 * d20) / np.maximum(den, 1e-20)
            valid = (u >= 0) & (w >= 0) & (u + w <= 1) & (den > 1e-18)
            candidates = [np.where(valid[:, :, None], projection, np.inf)]
            for first, last in ((a, b), (b, c), (c, a)):
                edge = last - first
                fraction = np.clip(np.sum((p - first) * edge, 2) / np.maximum(np.sum(edge * edge, 2), 1e-20), 0, 1)
                candidates.append(first + fraction[:, :, None] * edge)
            candidates = np.stack(candidates, 2)
            square = np.sum((p[:, :, None, :] - candidates) ** 2, 3)
            best = np.argmin(square.reshape(len(values), -1), 1)
            ti, ci = best // 4, best % 4
            closest = candidates[np.arange(len(values)), ti, ci]
            n = normal[np.arange(len(values)), ti]
            result.append(np.sum((values - closest) * n, 1))
        return np.concatenate(result)


def _skin_points(points, influences, matrices):
    import numpy as np
    width = max((len(w) for w in influences), default=1)
    index = np.zeros((len(points), width), dtype=np.int64)
    weight = np.zeros((len(points), width))
    for i, weights in enumerate(influences):
        for k, (bone, w) in enumerate(weights.items()):
            index[i, k] = bone
            weight[i, k] = w
    homogeneous = np.column_stack((points, np.ones(len(points))))
    out = np.zeros_like(homogeneous)
    for k in range(width):
        out += np.einsum('vi,vij->vj', homogeneous, matrices[index[:, k]]) * weight[:, k, None]
    return out[:, :3]


def _flips(points, faces, moved):
    import numpy as np
    n0 = np.cross(points[faces[:, 1]] - points[faces[:, 0]], points[faces[:, 2]] - points[faces[:, 0]])
    n1 = np.cross(moved[faces[:, 1]] - moved[faces[:, 0]], moved[faces[:, 2]] - moved[faces[:, 0]])
    active = np.linalg.norm(n0, axis=1) > 1e-9
    return int(((n0 * n1).sum(1)[active] < 0).sum())


def transfer(garment: Mesh, body: Mesh, rig: dict, full: float, none: float, iterations: int = 6):
    """Return the rewritten PAC bytes and per-layout statistics."""
    import numpy as np
    from scipy.spatial import cKDTree
    parent = [bone['parent'] for bone in rig['bones']]
    slot_maps: dict[tuple, dict[int, int]] = {}

    def slots_for(descriptor_palette):
        # bone -> first vertex slot that reaches it through this descriptor
        if descriptor_palette not in slot_maps:
            mapping: dict[int, int] = {}
            for slot in range(min(1024, len(garment.palette))):
                bone = garment.slot_bone(slot, descriptor_palette)
                if bone is not None and bone not in mapping:
                    mapping[bone] = slot
            slot_maps[descriptor_palette] = mapping
        return slot_maps[descriptor_palette]

    body_points, body_faces, body_weights = body.lod0()
    surface = Surface(body_points, body_faces)
    tree = cKDTree(body_points)
    out = bytearray(garment.data)
    stats = {'records': 0, 'rewritten': 0, 'skipped_layout': 0, 'folded_weight': 0.0}
    for part in garment.parts:
        layout = part['layout']
        if not len(part['p']):
            continue
        distances = surface.distance(part['p'])
        near_d, near_i = tree.query(part['p'], k=3)
        inverse = 1.0 / np.maximum(near_d, 1e-4) ** 2
        inverse /= inverse.sum(1, keepdims=True)
        # First pass: blended weights for every vertex near the body.
        blended, movable, transferred = [], [], []
        for i, (slots, weights, descriptor_palette) in enumerate(part['skin']):
            stats['records'] += 1
            stats['six_influence_records'] = stats.get('six_influence_records', 0) + (len(slots) == 6)
            own = garment.weights_of(slots, weights, descriptor_palette)
            amount = blend_amount(float(distances[i]), full, none)
            valid = SUM_RANGE[0] <= sum(weights) <= SUM_RANGE[1]
            if amount > 0 and not valid:
                stats['skipped_layout'] += 1
            if amount <= 0 or not valid:
                blended.append(own)
                movable.append(False)
                continue
            body_here: dict[int, float] = {}
            for j, share in zip(near_i[i], inverse[i]):
                for bone, weight in body_weights[j].items():
                    body_here[bone] = body_here.get(bone, 0.0) + share * weight
            blended.append(blend(own, body_here, amount))
            # Vertices hugging the body keep the body's own (already smooth)
            # skinning; only the transition band is smoothed.
            movable.append(amount < 1.0)
            transferred.append(i)
        # Second pass: smooth across the garment's own triangles so adjacent
        # vertices that reach different parts of the body do not crease.
        neighbours = [set() for _ in blended]
        for a, b, c in part['faces']:
            if max(a, b, c) < len(blended):
                neighbours[a].update((b, c))
                neighbours[b].update((a, c))
                neighbours[c].update((a, b))
        blended = smooth(blended, neighbours, movable, iterations)
        for i in transferred:
            slots, weights, descriptor_palette = part['skin'][i]
            slot_of = slots_for(descriptor_palette)
            mixed = blended[i]
            folded = fold_to_palette(mixed, set(slot_of), parent)
            stats['folded_weight'] += sum(w for b, w in mixed.items() if b not in slot_of)
            if not any(w > 0 for w in folded.values()):
                # Nothing this palette can reach: the vertex keeps its skin.
                stats['unreachable'] = stats.get('unreachable', 0) + 1
                continue
            # The record keeps its own influence count (4 or 6).
            pairs = quantize(folded, len(slots))
            new_slots = [slot_of[b] for b, _ in pairs]
            new_weights = [w for _, w in pairs]
            new_slots += slots[len(new_slots):]
            new_weights += [0] * (len(slots) - len(new_weights))
            offset = layout['voff'] + i * 40
            record = bytearray(out[offset:offset + 40])
            encode_skin(record, new_slots, new_weights)
            if decode_skin(record) != (new_slots, new_weights):
                raise AssertionError('skin round trip failed')
            if record != out[offset:offset + 40]:
                out[offset:offset + 40] = record
                stats['rewritten'] += 1
    # Only skin bytes of vertex records may differ.
    allowed_bytes = bytearray(len(out))
    for part in garment.parts:
        layout = part['layout']
        for i in range(layout['vc']):
            for b in SKIN_BYTE_OFFSETS:
                allowed_bytes[layout['voff'] + i * 40 + b] = 1
    if len(out) != len(garment.data) or any(a != b and not allowed_bytes[k]
                                            for k, (a, b) in enumerate(zip(garment.data, out))):
        raise AssertionError('bytes outside skin data changed')
    stats['folded_weight'] = round(stats['folded_weight'], 3)
    return bytes(out), stats


def evaluate(path_or_bytes, body: Mesh, rig: dict, poses: dict, label: str):
    """Penetration and flip counts for LOD0 under each shaped pose."""
    import numpy as np
    import tempfile
    if isinstance(path_or_bytes, (bytes, bytearray)):
        with tempfile.TemporaryDirectory() as folder:
            temporary = Path(folder) / 'candidate.pac'
            temporary.write_bytes(path_or_bytes)
            mesh = Mesh(temporary, rig)
    else:
        mesh = path_or_bytes
    points, faces, influences = mesh.lod0()
    body_points, body_faces, body_weights = body.lod0()
    before = Surface(body_points, body_faces).distance(points)
    outside = (before > 0) & (before < 0.05)
    result = {}
    for name, matrices in poses.items():
        shaped_body = _skin_points(body_points, body_weights, matrices)
        moved = _skin_points(points, influences, matrices)
        after = Surface(shaped_body, body_faces).distance(moved)
        penetrating = outside & (after < -0.002)
        result[name] = {'penetrating_vertices': int(penetrating.sum()),
                        'deepest_mm': round(float(max(0.0, -after[outside].min()) * 1000), 1) if outside.any() else 0.0,
                        'reversed_triangles': _flips(points, faces, moved)}
    return {'label': label, 'near_body_vertices': int(outside.sum()), 'poses': result}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--body', choices=['venus', 'vanilla'], required=True,
                        help='Body option the garments are fitted for.')
    parser.add_argument('--input', type=Path, nargs='+', required=True,
                        help='Garment .pac files or folders. VENUS inputs should already be fitted to Venus Body for Damiane.')
    parser.add_argument('--relative-to', type=Path, help='Mirror paths below this folder into --output.')
    parser.add_argument('--output', type=Path, required=True, help='A new folder; existing files are never replaced.')
    parser.add_argument('--report', type=Path, required=True, help='A new JSON report file.')
    parser.add_argument('--full', type=float, default=0.01, help='Metres from the body with full body skinning.')
    parser.add_argument('--none', type=float, default=0.06, help='Metres from the body where garment skinning is kept.')
    parser.add_argument('--smooth', type=int, default=6,
                        help='Smoothing passes over the 1-6 cm transition band of each garment (0 to 20).')
    parser.add_argument('--retry-smooth', type=int, default=12,
                        help='Smoothing passes for one retry when a fit creases too much (0 disables).')
    parser.add_argument('--no-evaluate', action='store_true',
                        help='Skip the before/after shaping comparison and write every garment unchecked.')
    parser.add_argument('--dmm-title', help='Write a DMM mod folder (mod.json and files/) with this title.')
    args = parser.parse_args(argv)
    if args.output.exists() or args.report.exists():
        parser.error('Choose a new --output folder and --report file.')
    if not 0 <= args.full < args.none <= 0.2:
        parser.error('Use 0 <= --full < --none <= 0.2 metres.')
    if not 0 <= args.smooth <= 20 or not 0 <= args.retry_smooth <= 20:
        parser.error('Use 0 to 20 smoothing passes.')
    _toolchain()
    import numpy as np
    from development_paths import RESEARCH, SOURCES, dependency
    rig = json.loads((RESEARCH / 'native' / 'rig.json').read_text(encoding='utf-8'))
    bind = np.asarray([b['bind'] for b in rig['bones']]).reshape(-1, 4, 4)
    inverse = np.linalg.inv(bind)
    poses = {}
    for name in ['1-1', '2-2', '1-2', '2-1', '0-1', '1-0', '0-2', '2-0']:
        pose = np.fromfile(RESEARCH / 'native' / 'poses' / f'{name}.bin', dtype='<f4').reshape(-1, 4, 4)
        poses[name] = inverse @ pose
    if args.body == 'venus':
        try:
            body_path = dependency('venus')
        except (FileNotFoundError, StopIteration):
            body_path = next((SOURCES / 'VENUS Elf Body').rglob('phw_00_nude_00_0001_damian.pac'))
    else:
        body_path = next((SOURCES / 'vanilla' / 'originals').rglob('phw_00_nude_00_0001.pac'))
    body = Mesh(body_path, rig)
    files = []
    for item in args.input:
        files.extend(sorted(item.rglob('*.pac')) if item.is_dir() else [item])
    report = {'tool': 'shape_follow', 'body_option': args.body, 'body_sha256': hashlib.sha256(Path(body_path).read_bytes()).hexdigest(),
              'blend_metres': [args.full, args.none], 'smoothing_passes': args.smooth,
              'in_game_tested': False, 'garments': []}
    # Creases the shaping itself puts in the body: the allowance a fitted
    # garment that follows the body may add on top of its own.
    body_points, body_faces, body_weights = body.lod0()
    body_creases = {name: _flips(body_points, body_faces, _skin_points(body_points, body_weights, matrices))
                    for name, matrices in poses.items()}
    report['body_reversed_triangles'] = body_creases
    from witch_parts import isolated_model
    root = args.output / 'files' if args.dmm_title else args.output
    written = 0
    for path in files:
        entry = {'input': str(path)}
        try:
            try:
                garment = Mesh(path, rig)
            except ValueError as error:
                # Pieces whose skin palette cannot be identified (some
                # helmets) are not touched.
                entry['skipped'] = str(error)
                report['garments'].append(entry)
                print(f'{path.name}: skipped, {error}', flush=True)
                continue
            data, stats = transfer(garment, body, rig, args.full, args.none, args.smooth)
            entry.update(stats)
            entry['smoothing_passes'] = args.smooth
            keep = True
            if not args.no_evaluate:
                entry['before'] = evaluate(garment, body, rig, poses, 'original skin')
                entry['after'] = evaluate(data, body, rig, poses, 'shape-follow skin')
                keep, entry['decision'] = improved(entry['before'], entry['after'], body_creases)
                # One retry with stronger smoothing when the first fit creases too much.
                if not keep and args.retry_smooth > args.smooth and 'reversed' in entry['decision']:
                    retry, retry_stats = transfer(garment, body, rig, args.full, args.none, args.retry_smooth)
                    retry_after = evaluate(retry, body, rig, poses, 'shape-follow skin, stronger smoothing')
                    retry_keep, retry_decision = improved(entry['before'], retry_after, body_creases)
                    entry['first_attempt'] = {'decision': entry['decision'], 'after': entry['after']}
                    if retry_keep:
                        data, keep = retry, True
                        entry.update(retry_stats)
                        entry['after'], entry['decision'] = retry_after, retry_decision
                        entry['smoothing_passes'] = args.retry_smooth
                    else:
                        entry['decision'] = f'{entry["decision"]}; with {args.retry_smooth} passes: {retry_decision}'
            if keep:
                relative = path.relative_to(args.relative_to) if args.relative_to else Path(path.name)
                destination = root / isolated_model(relative.as_posix())
                destination.parent.mkdir(parents=True, exist_ok=True)
                with destination.open('xb') as handle:
                    handle.write(data)
                entry['output'] = str(destination)
                entry['output_sha256'] = hashlib.sha256(data).hexdigest()
                written += 1
            worst_before = max((p['penetrating_vertices'] for p in entry.get('before', {}).get('poses', {}).values()), default=None)
            worst_after = max((p['penetrating_vertices'] for p in entry.get('after', {}).get('poses', {}).values()), default=None)
            print(f'{path.name}: {stats["rewritten"]} of {stats["records"]} records rewritten; '
                  f'most penetrating vertices {worst_before} -> {worst_after}; '
                  f'{"written" if keep else "original kept: " + entry.get("decision", "")}', flush=True)
        except Exception as error:  # report and continue with the next garment
            entry['error'] = repr(error)
            print(f'{path.name}: FAILED {error!r}', flush=True)
        report['garments'].append(entry)
    if args.dmm_title and written:
        (args.output / 'mod.json').write_text(json.dumps({'modinfo': {
            'title': args.dmm_title, 'version': '1.0.0', 'author': 'local',
            'description': f'Local shape-follow skinning for {written} witch garments ({args.body} body). '
                           'Game-derived files made on this computer; not for redistribution.'}}, indent=2) + '\n',
            encoding='utf-8')
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open('x', encoding='utf-8') as handle:
        handle.write(json.dumps(report, indent=2) + '\n')
    failed = sum('error' in g for g in report['garments'])
    skipped = sum('skipped' in g for g in report['garments'])
    print(f'Done: {written} written, {len(files) - failed - skipped - written} kept original, '
          f'{skipped} skipped, {failed} failed. Report: {args.report}')
    return 1 if failed else 0


def improved(before: dict, after: dict, body_creases: dict) -> tuple[bool, str]:
    """Keep a fitted garment only if shaping clips it less and creases stay bounded."""
    poses = before['poses']
    if not before['near_body_vertices']:
        return False, 'no vertices near the body'
    total_before = sum(p['penetrating_vertices'] for p in poses.values())
    total_after = sum(after['poses'][name]['penetrating_vertices'] for name in poses)
    if total_after >= total_before:
        return False, f'no fewer penetrating vertices ({total_before} -> {total_after})'
    deepest_before = max(p['deepest_mm'] for p in poses.values())
    deepest_after = max(after['poses'][name]['deepest_mm'] for name in poses)
    if deepest_after > deepest_before + 1.0:
        return False, f'deeper worst case ({deepest_before} -> {deepest_after} mm)'
    for name, p in poses.items():
        added = after['poses'][name]['reversed_triangles'] - p['reversed_triangles']
        if added > body_creases.get(name, 0) + 10:
            return False, f'{added} new reversed triangles under {name}'
    return True, f'penetrating vertices {total_before} -> {total_after}, deepest {deepest_before} -> {deepest_after} mm'


if __name__ == '__main__':
    raise SystemExit(main())
