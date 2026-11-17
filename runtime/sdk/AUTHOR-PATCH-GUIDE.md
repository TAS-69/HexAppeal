# Add a Wardrobe patch to an outfit

Your outfit remains your mod. A small provider registers its real name, installed prefab, controls and readiness with Hex Appeal. Wardrobe supplies the menu, per-witch saves, All witches, pins and optional Base underwear linkage. You supply and install every outfit resource yourself.

## Choose what your assets support

| Existing outfit structure | What you need to change |
| --- | --- |
| Bra/briefs already have independent engine visibility parts | Register their exact engine part keys using `part_visibility`. No mesh edits are required. |
| Bra/top is embedded in the dress mesh | Author a separate, independently addressable part, or supply alternate complete prefabs/models. Registering a toggle cannot separate embedded triangles. |
| Several complete outfit versions already exist | Register a choice using `HA_PREFAB_VARIANT`, or a complete `native_controls`/`states` matrix for combinations. |
| Your mod already controls its own renderer or materials | Register ordinary controls, poll saved/effective values and apply them only to your own instances. |
| One texture path is shared by all instances | Expose its colour as global. Per-witch colours require independently addressable resources and renderer support. |

An engine visibility key is not a Blender object name, material name or PAC submesh name. Use the actual key resolved by your installed assembly. Wardrobe does not export a public geometry-inspection or asset-splitting service. Keep any inspection tools in your own development workflow.

## Resource work for independent bra/briefs

1. Keep the body/skin coverage required when either garment is hidden. Supply compatible skin/body data through your own assembly if needed. A visibility switch cannot reconstruct omitted skin.
2. Keep separately hidden geometry out of the remaining dress piece. Otherwise it will still render even when the named accessory is hidden.
3. Preserve the compatible rig, skin weights, materials, texture routes, detail levels and cloth behavior for every state. Verify engine-specific body hiding/occlusion behavior in game; no universal recipe is established here.
4. Choose unique installed prefab/model routes for your patch. Package those resources in your own DMM mod, review shared routes and avoid replacing another outfit's assembly inadvertently.

For alternate prefabs, two independent toggles need all four states: both on, bra only, briefs only and both off. The default state must match the outfit's default prefab. Add colour states only if the resources actually allow independent colours. The SDK validates a complete matrix; it does not generate any assets.

## Minimal provider

Copy [parts-outfit.json](examples/parts-outfit.json) and [PartsOutfitProvider.cpp](examples/PartsOutfitProvider.cpp) into your x64 C++17 plugin project. Replace the namespaced outfit ID, display label, credits, requirements, prefab and both part keys with your actual values. If your outfit has no independently removable top, omit that control and binding.

```powershell
python sdk/tools/generate_provider.py your-outfit.json --output build/PartsDefinition.hpp
```

The generator emits a registration function named from your outfit ID. Update the provider's call to that generated function, include the generated header, and compile it with `HexAppealWardrobe.h` and `HexAppealWardrobeClient.hpp`. JSON alone is a build input; placing it in a mod folder does not register an outfit. The supplied example is an object to integrate into your plugin, not a finished ASI loader.

For complete prefab combinations, start from [outfit.json](examples/outfit.json) and [NativeOutfitProvider.cpp](examples/NativeOutfitProvider.cpp). For your own renderer, use [WardrobeProvider.cpp](examples/WardrobeProvider.cpp).

Call `Update(mountedAssetsReady)` from your normal update after loader initialization. Retry connection/registration while Hex Appeal is not ready. Report available only after your actual resource routes are mounted and usable. A downloaded folder is insufficient. Check every API result, expose a useful unavailable reason, and call `Shutdown()` before your provider unloads. Do not call into Hex Appeal from `DllMain` or a destructor under the loader lock.

Stable IDs preserve saves; changing a display name does not require a new ID. Use `Actual Mod Title / Outfit Name` for multiple outfits. Limits are eight controls, sixteen options per choice, sixteen visibility bindings and 256 combined states. Native geometry controls are per-witch. Base keys `Underwear`, `Nude`, `Body`, `Head` and `Face` are reserved.

## Link to Base underwear

Set `follow_base_underwear: true` on each supported per-witch garment toggle, with true/on meaning visible. Linked parts resolve hidden while **Hide base bra & panties** is checked. Each saved garment preference remains intact, including edits made during that override.

Native parts and prefab matrices apply effective values automatically. A custom renderer must check `HasBaseUnderwear()`, poll `Effective(...)`, and apply the returned `value`, watching both revision counters and rebuilt instances. Ordinary `GetControl` returns the stored value. The flag alone cannot hide a component your code never addresses. Leave unrelated accessories unlinked.

## Reloads, changes and missing dependencies

Wardrobe owns native switching only for recognized default witch outfits and its own replacements. Custom spouse/barber outfits remain outside that ownership. Native part bindings apply after the exact replacement is identified; natural visibility is retained and restored before swaps. A missing provider retains its saved choice and waits. Default is always the way back.

If your provider changes a shared external resource, finish and verify that change before requesting `Refresh`. Do not request refresh every frame. Unchanged choices are idempotent, rapid changes coalesce, and the saved outfit is retained during Default → replacement. A successful native call is not proof of correct rendered materials.

Use soft warnings for unverified but readable dependency releases. Missing installations or wrong/unreadable paths should disable the affected capability. If an actual native operation is incompatible, report its error; version optimism cannot make an incompatible interface work.

## Acceptance for your patch

Check all toggle/variant combinations, each supported witch, shape extremes and ordinary blends. Confirm visible skin, correct materials, every authored detail level and moving cloth. Test Base underwear override and restoration, including editing a choice while overridden. Test Default → outfit, leaving/re-entering the area, save reload, full restart, provider unavailable/available, rapid edits and custom spouse/barber clothing. Restore the player's settings after testing.

Native accessory visibility and Base underwear linkage have sampled live coverage on Elowen. Each adopting outfit still needs its own rendered checks for exposed skin, materials and every authored state.

Combined multi-control prefab matrices have offline coverage; rendered coverage is still required for an adopting outfit. See [advanced integrations](ADVANCED-EXAMPLES.md) for a complete eight-state matrix, independent accessory bindings, custom renderer code and refresh/error handling.

My SDK code and examples are 0BSD. They confer no licence to game assets or another outfit mod. Obtain any permissions needed for your own patch and retain the original credits.
