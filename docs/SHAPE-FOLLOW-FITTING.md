# Shape-follow fitting

Hex Appeal shapes witches through Instant Surgery's pose deformation. The shaping moves skeleton bones: chest, seat, hip, thigh, calf and spine helpers. A mesh follows those bones only as far as its skin weights allow. The body is weighted to all of them. Many garments are weighted only to the main bones, so with a fuller shape the body pushes through them.

`tools/shape_follow.py` gives garment vertices near the body the body's own skinning, so garments follow the live shaped proportions. It works for both body options:

- **Vanilla:** original garments against the original witch body.
- **VENUS:** garments fitted to Venus Body for Damiane, such as Hex Appeal - Outfit Refits, against the installed Venus Body for Damiane.

It is an offline, local step. The game, Hex Appeal's runtime and Instant Surgery are not changed.

## What it changes

- **Transfer.** Vertices within 1 cm of the body take the body's skinning. Between 1 and 6 cm the garment's own skinning blends back in. Beyond 6 cm the garment is untouched, so loose cloth keeps its own motion.
- **Smoothing.** The transition band is smoothed across the garment's own triangles, so neighbouring vertices do not crease.
- **Palette.** Bones a garment's palette does not contain are folded into their nearest ancestor that it does contain. Palettes, bone lists and file layout do not change.
- **Bytes written.** Only the active skin slots and weights of a vertex record change, in every LOD. Positions, normals, both texture coordinates, the per-vertex influence count (4 or 6) and file size stay as they are. The format follows the game's skinning shader (as decoded in CrimsonForge). Every record is cross-checked against CrimsonForge's decoder.
- **Fit gate.** Each garment is skinned with the native Instant Surgery poses for all eight non-neutral pad points, before and after. It is written only if:
  - fewer garment vertices end up inside the shaped body;
  - the worst case is no deeper;
  - it gains no more reversed triangles than the shaping itself puts in the body (plus ten).

  If only the crease limit fails, one retry with stronger smoothing (12 passes) is tried. Otherwise the original is kept. Pieces whose skin palette cannot be identified (some helmets) are skipped. The JSON report records every number.

## Results on the default witch garments (23 November 2026, offline)

| Body option | Fitted | Kept original | Skipped | Penetrating vertices on fitted garments, summed over the 8 pad shapes |
|---|---|---|---|---|
| VENUS (Outfit Refits baseline) | 11 | 23 | 6 | 907 → 85 |
| Vanilla (original garments) | 11 | 25 | 4 | 755 → 68 |

- **Largest gain:** upper-body piece `cd_phw_00_ub_00_0164`, where clipping under HEX/HEX dropped from 31.8 mm to 5.7 mm deep on VENUS and from 33.2 mm to 6.9 mm on Vanilla.
- **Kept originals:** most garments already showed no penetration. Two tops (`ub_00_0186`, `ub_00_0187`) would clip less but crease more than allowed, so they keep their original skinning.

## Running it

It needs the development inputs used by `tools/refit.py`. Set `HEX_APPEAL_RESEARCH` or `development-paths.json` to the external research root containing:
- `native/rig.json`;
- `native/poses/*.bin`;
- the CrimsonForge toolchain;
- the original witch sources.

```powershell
python tools/shape_follow.py --body vanilla --input "RESEARCH\sources\vanilla\witch-sources" --relative-to "RESEARCH\sources\vanilla\witch-sources" --output "NEW_FOLDER" --report "NEW_FOLDER.json" --dmm-title "Hex Appeal - Shape Follow (Vanilla, local)"
python tools/shape_follow.py --body venus --input "Hex Appeal - Outfit Refits\files" --relative-to "Hex Appeal - Outfit Refits\files" --output "NEW_FOLDER" --report "NEW_FOLDER.json" --dmm-title "Hex Appeal - Shape Follow (VENUS, local)"
```

Outputs and reports are never overwritten; choose new names. With `--dmm-title`, the output folder can be imported into DMM directly. Give it priority over Hex Appeal - Outfit Refits and enable it with the matching body option only.

## Limits

- **Measurement.** Fit is measured offline with the native shaping poses and the body's LOD0 surface. In-game appearance has not been checked.
- **Garment design.** Rigid pieces such as plates and buckles bend with the body where they sit within 6 cm of it. Garments made for the Venus Body for Damiane shape need a geometry refit to it first; this tool fits skinning, not shape.
- **Content.** Outputs contain Pearl Abyss garment data made on your computer. Do not redistribute them.
