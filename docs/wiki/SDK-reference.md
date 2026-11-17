# SDK reference

The C ABI is in `HexAppealWardrobe.h`; the C++17 helper is `HexAppealWardrobeClient.hpp`. These are in `runtime/sdk/` in the repository and at the root of the SDK download.

## Capabilities

| Export / flag | Purpose |
| --- | --- |
| `HexAppeal_GetWardrobeAPI(1)` | Registration, readiness, saved selections and controls; original 72-byte v1 table |
| `HexAppeal_GetNativeWardrobeAPI(1)` | Checked native replacement prefabs and visibility bindings |
| `HexAppeal_GetBaseUnderwearAPI(1)` | Base preference and effective garment values |
| `HexAppeal_GetOutfitVisibilityAPI(1)` | Separate Hide outfit preference, revision and readiness |
| `HexAppeal_RequestWardrobeRefreshV1` | Refresh after an external resource change is complete and verified |
| `HA_GLOBAL` | Shared control, such as one shared texture |
| `HA_PREFAB_VARIANT` | Simple per-witch prefab choice |
| `HA_FOLLOW_BASE_UNDERWEAR` | Opt-in per-witch toggle override; stored preference preserved |
| `HA_FOLLOW_OUTFIT_HIDE` | Opt-in per-witch clothing toggle; stored preference preserved |

`WardrobeClient::Connect()` finds the already loaded ASI and optional capabilities. Retry during normal updates; do not load a second copy. Check `HasNativeOutfits()`, `HasBaseUnderwear()` and `HasOutfitVisibility()` before using those extensions. Validate table size/version in C, initialize structure sizes, zero unused fields and check every return value.

## Definitions

Use stable namespaced outfit, control and option IDs. Labels support 63 UTF-8 bytes. Changing a label does not require changing an ID. Definitions/strings are copied; no callbacks or asset pointers are retained. A new registration starts unavailable. Unregister invalidates its token while retaining saved preferences.

Limits: **8 controls, 16 options per choice, 16 visibility bindings, 256 complete prefab states**. The generator rejects duplicate/incomplete states, mismatched defaults and invalid bindings. Existing output requires `--force`; linked output is refused.

`part_visibility` maps per-witch toggles to real engine parts. Reserved base keys are `Underwear`, `Nude`, `Body`, `Head` and `Face`. `native_controls` plus `states` supplies a complete combination matrix. A single control cannot drive both a matrix and visibility binding; distinct controls may use both.

Use `HA_FOLLOW_OUTFIT_HIDE` for outer clothing and `HA_FOLLOW_BASE_UNDERWEAR` for bra/briefs. These flags cannot share one control. Hide outfit and Base underwear remain independent in the native part policy, prefab matrix and effective values returned to a custom renderer.

## Targeting

| Index | Witch |
| --- | --- |
| 0 | Lyselia |
| 1 | Bari |
| 2 | Areciel |
| 3 | Elowen |
| 4 | Sylvia |
| 5 | Human White Crow |

Use `-1` to set All for per-witch controls or access a global control. Query individual witches for per-witch values. `GetState(-1)` reports combined Base underwear state through `mixed` and `hiddenMask`; effective garment queries require an individual witch.

## Stored, effective and rendered

`GetControl` returns the stored choice. `GetEffectiveControl` applies the optional Base underwear and Hide outfit overrides. A custom renderer must apply the effective `value`, watch outfit `revision`, `baseUnderwearRevision` and the separate Hide outfit revision from `OutfitVisibilityState`, and handle rebuilt actors even if revisions did not change. Existing API tables keep their sizes and versions. `GetSelection` describes requested state and readiness, not proof that a prefab has rendered.

`OutfitVisibilityState(witch, state)` reads Hide outfit, and `HideOutfit(witch, hidden)` changes and saves it. An outfit must explicitly opt in before its clothing is affected. See [Hide outfit integration](SDK-Outfit-Visibility.md) for the complete contract.

Native ownership is restricted to recognized default outfits and exact replacements. Natural visibility is captured/restored around owned swaps. Losing ownership stops writes into custom equipment. An unavailable provider waits with its saved choice; Default remains available for restoration.

## Refresh and lifetime

Save and verify your changed resource before requesting `Refresh`. The runtime queues Default → selected without saving Default as the requested choice, waits for assembly acknowledgement and settling, and combines rapid changes. A new selection cancels an old refresh cycle. Native prefab changes already refresh automatically. The default `[Wardrobe] RefreshDelayMs=500` is clamped to 100–10000 ms in HexAppeal.ini. Three game-robe cycles passed at 500 ms, with 625 ms observed waits. Test your own texture-cache behaviour and increase the delay if required. Do not request refresh every frame. `HA_UNAVAILABLE` can mean no matching owned visible instance; a later assembly uses current resources.

Registry/settings calls are serialized, but saves access disk: do not hold game/render locks while calling. Use one mutable helper per thread or serialize it. Read `LastError()` immediately on the same thread. Unregister before provider unload, outside the loader lock. Hot-unloading Hex Appeal itself is unsupported; API tables are valid only while its module remains loaded.

See [examples](SDK-examples.md) and the SDK README for implementation details and return handling.
