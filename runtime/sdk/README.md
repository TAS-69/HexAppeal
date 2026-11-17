# Wardrobe SDK

Wardrobe supplies a shared default-witch outfit selector, automatic saves, per-witch/All targeting, variants, toggles and individual quick-panel pins. Your mod supplies and installs its own resources. The SDK contains no outfits, game files or dependency binaries.

The SDK is asset-free. Native accessory visibility has sampled live coverage on Elowen; authored prefab matrices have offline coverage and require rendered acceptance for each adopting outfit. Registration does not establish fitting or rendering correctness. See [the patch guide](AUTHOR-PATCH-GUIDE.md).

## Quick start

1. Copy the SDK into your x64 Windows C++17 project, or use its CMake interface target.
2. Copy an example JSON and replace its IDs, labels and prefab/part names with your actual resources.
3. Generate its registration header. Call the example's `Update()` from your normal update after loader initialization, and `Shutdown()` before unloading your provider. Never call from `DllMain`.
4. Report available only after your compatible assets are mounted. A download folder alone is insufficient.
5. Test selection, streaming, rebuilds, Default restoration and fitting on each supported witch.

```powershell
python sdk/tools/generate_provider.py sdk/examples/parts-outfit.json --output build/PartsDefinition.hpp
```

The generator validates definitions and emits ordinary C++. It does not run a mod, load assets or install anything. Existing output requires `--force`; linked output is refused. Limits: eight controls, sixteen options per choice, sixteen visibility bindings and 256 combined prefab states.

| Example | Who applies the options? |
| --- | --- |
| [Parts definition](examples/parts-outfit.json), [provider](examples/PartsOutfitProvider.cpp) | Wardrobe handles declared engine visibility parts on its owned replacement. |
| [Combined variants](examples/outfit.json), [provider](examples/NativeOutfitProvider.cpp) | Wardrobe chooses the supplied prefab for each bra/briefs combination. |
| [Polling provider](examples/WardrobeProvider.cpp) | Your checked renderer integration applies saved/effective values. |
| [Eight-state outfit](examples/advanced-outfit.json), [provider](examples/AdvancedOutfitProvider.cpp) | Wardrobe combines colour/garment prefabs with an independent belt part. |
| [Custom renderer definition](examples/renderer-outfit.json), [provider](examples/CustomRendererProvider.cpp) | Your renderer handles effective visibility, rebuilt instances and shared colour refresh. |

All example prefab/part names are placeholders. No outfit asset accompanies them. [Advanced integrations](ADVANCED-EXAMPLES.md) provides complete definitions, code and acceptance steps.

## Connect and register

`HexAppealWardrobeClient.hpp` provides `WardrobeClient`, bounded `Text()` and zero-initializing `Sized<T>()`. `Connect()` finds the already loaded `HexAppeal.asi` and optional capabilities. Retry during normal updates if it is not loaded yet. Never load a second instance.

```cpp
hexappeal::WardrobeClient wardrobe;
uint64_t registration = 0;
if (wardrobe.Connect() && !registration)
    Register_example_parts_dress(wardrobe, registration);
if (registration) wardrobe.Available(registration, true);
wardrobe.Unregister(registration);
wardrobe.Reset();
```

Use stable namespaced IDs, such as `yourname/dress`; `hexappeal/` is reserved. Display the actual mod title, or `Mod Title / Outfit Name` for several outfits. Labels hold 63 UTF-8 bytes; put the full title in credits/requirements if necessary. Correcting a label must not change its ID. Keep control/option IDs stable too.

Definitions and strings are copied immediately; temporary arrays may be released. No callback or asset pointer is retained. Registrations start unavailable. `SetAvailable` changes readiness; `Unregister` invalidates its token and preserves saved choices. Re-registration receives a new token. A removed selected provider stays identifiable; Default remains the restoration route.

The [C header](HexAppealWardrobe.h) works without C++. Resolve `HexAppeal_GetWardrobeAPI(1)` and check size/version. The original v1 table remains 72 bytes. Zero unused fields, set every structure's `size`, and check return codes. Optional exports leave that table unchanged.

## Engine visibility parts

The optional `HexAppeal_GetNativeWardrobeAPI(1)` extension is discovered by `HasNativeOutfits()`. `HA_VisibilityPart` maps a per-witch toggle to an **actual engine visibility part** in your prefab. This is not an arbitrary mesh object name. Author bra/briefs as independently addressable parts. Base keys `Underwear`, `Nude`, `Body`, `Head` and `Face` are reserved.

JSON `part_visibility` generates these bindings. A toggle may control several parts; each part has one binding. Wardrobe applies only after its checked switch owns the selected live replacement and exact assembly. It restores captured natural visibility before its swaps. Lost ownership drops the policy without writing into a custom spouse/barber outfit. Provider removal restores only components still proven to belong to the owned replacement. Visibility does not change skin weights, cloth simulation or collision.

## Authored prefab combinations

For alternate geometry, declare `native_controls` and a complete `states` matrix. Each row names a registered prefab for one combination. The default combination must match the default prefab. Mapped controls must be per-witch; unmapped values are zero. Duplicate or incomplete states are rejected before registration.

Separate bra/briefs toggles require four authored states. Colours can be included if they use genuinely independent prefabs/material resources. Shared texture replacement remains global. A control cannot drive both a matrix and a visibility binding; distinct controls may use both routes in one outfit. `HA_PREFAB_VARIANT` remains available for one simple per-witch choice without a matrix. Unchanged saved values do not queue redundant rebuilds.

## Provider-owned options

Ordinary choices/toggles are rendered and saved by Wardrobe; your mod applies their behavior. Poll `GetControl`, identify your own live instances and reapply after actor creation/rebuild even if settings revisions have not changed. Use `HA_GLOBAL` for shared resources. Wardrobe does not guess vertex ranges, generate missing garments or recolour shared materials automatically.

## Follow Base underwear

`HexAppeal_GetBaseUnderwearAPI(1)` supplies `GetState`, `SetHidden` and `GetEffectiveControl`; check `HasBaseUnderwear()`. The separate menu section controls the game's combined base bra/panties and this opt-in policy.

Declare `HA_FOLLOW_BASE_UNDERWEAR` on **per-witch toggles**, with 1 visible and 0 hidden. While Base underwear is hidden, effective value is 0. Stored choices remain unchanged, including edits made while hidden. Showing Base underwear restores each saved outfit choice. Unmarked accessories are unaffected.

Native parts and prefab combinations apply effective values automatically. Provider-owned rendering must poll `GetEffectiveControl` and apply `value`. The flag alone does not identify geometry. Watch both outfit `revision` and `baseUnderwearRevision`, and handle rebuilt instances. `GetControl` retains its stored-value semantics.

`GetState(-1)` reports All through `mixed` and `hiddenMask`. `SetHidden(-1, ...)` changes all six witches. `GetEffectiveControl` requires one witch. `nativeReady`/`nativePending` describe only the game's base part, never your garment renderer. Failed saves preserve the preference.

## Targeting, threading and lifetime

Witches: 0 Lyselia, 1 Bari, 2 Areciel, 3 Elowen, 4 Sylvia, 5 human White Crow. `-1` sets All for per-witch controls or accesses a global control. Query individual witches for per-witch values. The UI reports mixed values.

Runtime calls serialize registry/settings access. Do not hold game/render locks while calling: saves access disk. Use one `WardrobeClient` per thread or serialize its mutable state. Read `LastError()` immediately on the same thread after failure. Unregister before provider unload. The examples avoid destructor callbacks under the loader lock. Hot-unloading Hex Appeal itself is unsupported; tables are valid only while its module remains loaded.

`GetSelection` describes requested/saved state and dependency readiness, not proof that a prefab is rendered. Native assembly is asynchronous. Unavailable providers retain their saved selection waiting; valid ownership and native checks remain mandatory.

## Refresh changed resources

After saving and verifying a shared resource, call optional `HexAppeal_RequestWardrobeRefreshV1` (`WardrobeClient::Refresh`). It queues Default → selected replacement on visible owned instances without saving Default as the choice. Assemblies are acknowledged with a non-blocking settling interval. Rapid changes coalesce; another outfit selection cancels the old cycle.

Native prefab changes apply automatically. Provider-owned files need refresh only after they are ready. `HA_UNAVAILABLE` can mean no owned matching replacement is visible; its next assembly uses current resources. Assembly acknowledgement alone does not establish correct materials.

## Build and compatibility

```powershell
cmake -S sdk -B build/sdk -A x64 -DHEX_APPEAL_BUILD_EXAMPLE=ON
cmake --build build/sdk --config Release
python -m unittest discover -s sdk/tests
```

Examples compile as object libraries for your plugin, without an ASI loader. Python is needed only to generate headers/build examples. My helper, generator, API and examples are 0BSD: attribution is welcome, not required. This grants no rights to outfit/game assets.

Compatibility follows the ABI, stable IDs and resource contract. Unknown readable dependency versions receive soft warnings and remain usable. Missing/unreadable files block affected capabilities; an invalid native interface reports its operation error. No universal past/future dependency or arbitrary garment-fitting guarantee is made.

See the Player Guide Compatibility page and release verification summary for tested scope. Offline coverage includes complete matrices, malformed definitions, copied schemas, missing capabilities, concurrent edits, failed saves, natural visibility restoration, ownership loss and provider removal. Separate live acceptance is required for adopting outfits.
