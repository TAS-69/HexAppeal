# Compatibility and known issues

## Checked references

Last reviewed **19 November 2026**. These versions were used for local checks.

| Component | Checked version | Scope |
| --- | --- | --- |
| Crimson Desert | **2.05.01 / Steam 26776744** | Native layouts, actor ownership, shaping and sampled outfit operations |
| Definitive Mod Manager | **5.0.1** | Two body choices, ordered labels, merged appearance/registry routes and private resource mounting |
| Venus Body for Damiane | **2.6 Human / Elf Body** | Body format and matching skinning; Elf core session and earlier Human comparison |
| Instant Surgery | **0.9.6** | Checked native shaping interface and live coordinated deformation |
| Hex's Hentai Preset | **1.3**, with **1.2** retained | External preset parsing and regional shaping; 1.3 is preferred |
| Two Reasons Body Physics | **1.0.7** | Optional pose interface and coordinated update acknowledgement |
| Character Creator | **9.1.41**, **9.1.42** | Optional hook coexistence, startup, shaping, Wardrobe and base visibility |
| Spouse Overhaul | **0.5.0**, **0.6.0** | Optional hook coexistence. Live checks on 0.6.0 covered saved Barber changes, save reloads and Combat Gear with Lyselia. |

These are checked versions, not a promise that every past or future release works. Unknown readable versions receive a soft warning and remain usable. Native layouts, function signatures, ownership and transaction checks must still pass. The static default-outfit refits were built against Venus Body for Damiane 2.6 and need a new fit review if its geometry changes.

**Settings → Dependencies & paths** has separate **Feature checks**, specific errors for actual interface failures, named-export validation independent of an export's address, and tolerant preset formatting. See [dependency compatibility](../DEPENDENCY-COMPATIBILITY.md) for recovery and the remaining private-interface limits.

The runtime warns when it finds an unverified Spouse Overhaul or Character Creator build. Version warnings leave Wardrobe available. Each operation still checks that the native interface and outfit are safe to use.

Both Human Body and Elf Body are recognized. Their body/hand skinning and physics match, so the existing default refits use the same baseline. Live Human checks on Elowen covered all nine combinations on Dancer, Default ↔ Dancer switching and independent minimum Height / maximum Weight on the default robe. See [Venus Body for Damiane compatibility](../VENUS-COMPATIBILITY.md) for the tested build and limits. A DMM priority rule tied to the Elf variant may need to be chosen again after switching to Human.

## Conflicts

| Overlap | Action |
| --- | --- |
| Another complete body replacement, including NaiK's Original Damiane Body | Let the selected Venus Body for Damiane variant (Human or Elf Body) win the body route |
| Damiane Armor Refit Pack on shared boot routes 0160 / 0161 / 0163 | Witch-only private boots avoid this overlap. The armour pack retains its Damiane routes. |
| Venus Body for Damiane installed for Damiane with Vanilla witches | Supported. Choose Vanilla in Hex Appeal; keep the separate Venus Body for Damiane and armour entries enabled. |
| Older Hex Appeal package still enabled | Disable the older package to avoid conflicts between its body routes and the current selection. |
| Spouse Overhaul / barber custom outfits | Use **Follow game** to leave appearance changes to Spouse Overhaul. Switch to **[SO] Barbers** once its current look is available. |
| Another modifier writing the same pose buffers | A checked transaction may reject the shape rather than overwrite foreign work |
| Another F9 binding | Change Hex Appeal's key in Settings |

## Known limits

- Hair can flicker briefly when opening Spouse Overhaul's Combat Gear preview or enabling **Always Wear Gear**. Equipped combat gear remains usable.
- A brief flicker can still appear during a switch to **[SO] Barbers**.
- Changing Maegu’s Height in photo mode can shift her hood over her eyes. The fit recovered when movement resumed in the tested scene.
- Maegu and Woosa follow the selected body mode when their original PHW body and the expected mount are available. Choose outfit versions fitted for the body in use. The Black Widow VENUS pieces were checked on both characters with Venus Body for Damiane geometry.
- Extreme proportions may clip. Coordinated pose shaping is not arbitrary mesh regeneration, cloth/collider authoring or a universal outfit-fit guarantee.
- Default garment refits are fitted to Venus Body for Damiane 2.6; changed geometry needs a fit review.
- Controller input, every detail level, long sessions and every witch in motion have not received exhaustive live acceptance.
- Base underwear can briefly reappear while an outfit rebuild settles, before its separate visibility policy reapplies.
- The combined base bra/panties part is not exposed as independent controls. Providers may supply separate garment parts.
- The 500 ms refresh setting passed game-robe assembly tests; individual outfit textures may need a longer interval.
- Earlier private outfit material/visibility problems did not establish a universal cause. That outfit integration is not part of this release.
- Dancer dependencies, setup and colour controls are excluded pending permission. Generic SDK providers remain supported and need rendered checks with their own assets.

## What was checked

The 1.1.4 release passed 33 offline runtime checks and 58 helper tests. Live DMM imports of the 1.1.3 release covered VENUS and Vanilla, the six character packs, Woosa & Maegu’s saved disabled state through its 1.0.1 update and a full 250-record roster. They cover dependency variants, Wardrobe slots and the stock equipment catalogue, wearer names on default outfit pieces, Barber looks, outfit providers, skin routes, character add-ons and game signature lookup. The native fixture covers all nine blends, 44 weighted cases and eleven reference poses. Earlier live tests near Elowen checked both body choices with the armour pack enabled.

The optional SDK base-underwear replacement policy is covered by native ownership/visibility fixtures. A separately authored custom outfit still needs its own rendered acceptance. Earlier refresh, Human-body, all-witch and moving-cloth evidence retains its original scope. See the [1.1.4](../VERIFICATION-1.1.4.md) and [1.1.3](../VERIFICATION-1.1.3.md) verification records for the exact builds and test scope, and the [1.0.2 record](../VERIFICATION-1.0.2.md) for the earlier body and packaging checks.

## Troubleshooting

**F9 does nothing:** confirm the ASI loader and only one HexAppeal.asi, then check the log and key binding. A plugin update requires a full game restart.

**Game closes during startup:** the Steam overlay can crash in its own swapchain code when several mods start at the same moment, most often on the first launch after a restart. Turn off "Enable the Steam Overlay while in-game" for Crimson Desert in Steam. Since 1.1.2, Hex Appeal no longer creates a test swapchain during startup.

**Graphics compatibility.** A player on an NVIDIA RTX 5070 who saw the UI shimmer confirmed that Hex Appeal 1.1.2 resolved it with DLSS Frame Generation enabled, and later confirmed a 1.1.3 test build. Another NVIDIA player, whose game presents through NvPresent64.dll, confirmed that F9 and the Wardrobe appear with the 1.1.4 fix. On setups where 1.1.3 already drew, 1.1.4 draws the same way.

**To report a problem**, send the file from **F9 → Log → Save report**. Look for `HexAppeal-report-*.txt` in `%LOCALAPPDATA%\HexAppeal`. It includes the exact build, feature and dependency checks, and graphics details such as your GPU and Frame Generation setting. If the game closes before you can open F9, or F9 won’t open, send `%LOCALAPPDATA%\HexAppeal\HexAppeal.log`. Its first line identifies the build.

**Preset not detected:** automatic discovery checks DMM's configured storage, mounted source paths, portable locations and Instant Surgery preset folders. Nested and renamed supported presets are accepted. In Settings, select an extracted `.ini` or its folder; clear the override to resume automatic detection. An invalid manual path is reported directly.

**Saved / waiting:** bring the selected witch into view and inspect Dependencies & paths. Downloaded files and active mounted files are different checks.

**Outfit stays default:** confirm its resources are mounted and the actor is wearing a recognized default, rather than custom spouse/barber equipment. Let the world and assembly finish loading.

**Outfit waiting:** check the message below the profile selector. You can choose a profile while the game loads the character or recovers its outfit functions. An unverified mod version is informational. Hex Appeal checks the game hook and the character's live outfit before applying the change.

Hover a blocked control or its status to see the reason. For mounted files, Hex Appeal shows the active source and game file when DMM identifies them. For a native hook, it shows the failed check and the plugin the entry routes into when that destination can be identified. **Log → Save report** includes the same dependency details.

Wardrobe displays the exact startup blocker in the panel. Its outfit controls use their own checks. Optional appearance tracing can be skipped when another mod owns that hook. If an outfit change is unconfirmed, that instance waits for the new outfit to appear while other witches remain available. **Custom outfit retained** means the witch is wearing equipment assigned by another mod. Base underwear can still operate on the game's separate underwear part. Underwear built into a custom mesh follows that outfit's own visibility support.

**Underwear removers** follow their own visibility rules in **Base underwear**. If the remover only hides a mesh, Hex Appeal can show it again when you take back control. If the remover drops the mesh reference, Wardrobe disables **Hide base bra & panties** for that witch and explains why.

Shape checks the mounted body independently. If a remover redirects a witch to a compatible replacement, the midpoint follows that body and the Log lists the affected witch. The other witches keep their selected bodies. If a replacement is missing or uses an unsupported rig, only its assigned witch is skipped.

**Provider colour or materials look wrong:** check resource conflicts, test a fresh save load and full restart, and report whether it happens with original resources and neutral shaping. Include the selected colour/shape, witch, game/dependency versions and a screenshot. Do not assume every material issue comes from shaping.

**Crash:** close the game and disable Hex Appeal through DMM for a comparison. Keep a copy of the relevant logs before retrying. A log being active at the crash is timing evidence, not proof of its cause.
