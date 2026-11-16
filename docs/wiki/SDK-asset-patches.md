# Outfit asset patches

Wardrobe can expose what your outfit has been authored to support. It cannot create missing skin, separate embedded triangles or repair cloth weights just because a toggle exists.

## Removable bra and briefs

Use either independently addressable **engine visibility parts**, or alternate complete prefabs. Engine keys are not Blender object names, material names or PAC submesh names.

1. Keep the necessary skin/body coverage visible when a garment is hidden. Account for the game's body hiding/occlusion rules.
2. Separate removable geometry from the remaining dress, or provide the appropriate alternate complete assembly.
3. Preserve compatible rigging, weights, materials, texture routes, all authored detail levels and cloth behavior in every state.
4. Use unique installed routes and review conflicts. Package your assets in your own mod or permissioned patch.
5. Register the matching engine parts or a complete prefab matrix, then test rendering.

Two toggles need four complete prefab states: both visible, bra only, briefs only and both hidden. The default row must match the default prefab. Colour states must also use independently addressable resources if you want per-witch colour. A shared texture path should be a global control.

## Follow Base underwear

Declare `follow_base_underwear: true` on supported per-witch garment toggles. **1 / on means visible; 0 / off means hidden.** Hiding Base underwear makes the effective value hidden while preserving the player's stored choice. Showing it again restores that choice, including edits made during the override.

Native parts and native prefab matrices apply effective values automatically. A provider-owned renderer must use the effective value and watch both revision counters. Merely adding the flag does not identify geometry for your renderer.

## For a Dancer compatibility patch

The built-in integration uses the original external outfit and colours. Its embedded bra/top has no independent removable route. To supply one, author a separate top part or complete alternate upper assemblies with correct exposed skin, materials, rig and detail levels. Declare the real keys/prefabs in your provider, link supported garment toggles to Base underwear if wanted, and distribute that patch with your own mod. Hex Appeal should continue referencing it externally. No rejected private split-model resources are included as a shortcut.

## Acceptance checklist

Test every variant/toggle combination, supported witch, neutral shape, extremes and ordinary blends. Check front/rear views, exposed skin, materials, detail levels and moving cloth. Test Default restoration, area streaming, save reload, full restart, rapid edits, missing/returning providers and custom spouse/barber outfits. Check Base underwear hiding, saved edits during the override and restoration afterward.

API/definition tests validate the contract. They do not establish rendered fitting. Combined matrices still need live acceptance on your authored outfit. See the SDK's `AUTHOR-PATCH-GUIDE.md` for the full procedure.
