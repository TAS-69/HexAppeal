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

## Combine variants and independent parts

Use a complete native matrix for controls that change the full assembly, and separate `part_visibility` bindings for controls with their own engine parts. A single control cannot own both routes. For example, two colours and two garment switches require eight authored states; an independent belt can use one engine visibility key.

The [advanced examples](SDK-examples.md) include the complete JSON, generated-header commands, lifecycle code, a shared-colour/custom-renderer provider, revision handling and error handling. All names are placeholders for your own assets.

## Test the patch

Check every combination, supported witch, shape range, detail level and moving cloth. Verify Base underwear overrides and stored-choice restoration, actor rebuilds, missing resources, save reload, full restart and custom spouse/barber equipment. Registration alone cannot establish rendering or fitting correctness.

For an outfit supplying its own bra/briefs, see [scoped base underwear replacement](SDK-Base-Underwear-Policy.md). This hides the normal pair only while the selected replacement is owned, without changing the player's saved preference.
