# Changes

## 1.1.2

- Removed the after-frame-generation drawing for DLSS and XeSS Frame Generation that came with the previous 1.1.1 upload. Some players reported the game not starting after that update. 1.1.2 is the 1.1.1 performance build without that drawing route.
- Hex Appeal no longer creates a test swapchain during startup. It skips it once it has seen the game create its own swapchain, and otherwise waits at least 30 seconds.
- The automatic log no longer lists loaded graphics libraries; F9 > Log > Save report still includes them. Graphics details in the log update once the game has created its swapchain.
- The log and saved reports name the exact build, for example "Hex Appeal 1.1.2 session (build 03923439)".

## 1.1.1

- Reduced rendering and input overhead while F9 and Wardrobe are closed.
- Reduced repeated dependency checks, Wardrobe searches and NPC pose-buffer copying.
- Logs and saved reports include the graphics adapter, driver, resolution, Frame Generation settings and loaded graphics library versions.
- The Nexus upload also drew F9 and native [HA] Wardrobe on finished frames on DLSS and XeSS Frame Generation paths, and labelled default witch outfit pieces with the wearer's name. 1.1.2 does not include these.

## 1.1.0

- Improved F9 and **[HA] Wardrobe** rendering when Streamline or XeSS Frame Generation hands the game its swapchain. Hex Appeal uses the command queue associated with that swapchain and retries renderer discovery when no frames arrive.
- Native **[HA] Wardrobe** waits until its editor draws before hiding the game HUD. If the editor stops drawing, Hex Appeal releases the interaction and restores the HUD.
- Wardrobe retries outfit capture while characters load and recovers readable outfits from partially damaged profile files. If a saved part is unavailable, other slots can apply and the affected slot keeps its current item.
- The equipment picker keeps usable results while a catalog refresh retries. Mounted pieces can appear without an icon, and a fit advisory identifies pieces whose body fit needs checking.

## 1.0.9

- Hover a blocked control to see why it is unavailable. I show the mod or file Hex Appeal can identify, along with the specific check that failed.
- Wardrobe checks shared game hooks by what they do, so an unfamiliar mod or version can still use the same outfit and visibility controls.
- Outfit selection stays available while the game is getting ready. **Follow game** and **[CD] Vanilla** also work from **ALL CHARACTERS**.
- The Wardrobe editor retries when a character's live outfit is still loading. A rejected outfit change can retry without disabling Wardrobe for other characters.
- Wardrobe, visibility and the native interaction prompt identify characters directly from the game. Instant Surgery is required for Shape. Visibility choices save while the renderer is getting ready.
- Outfit changes initiated by Hex Appeal use the game's checked outfit routine directly. Other mods can still handle their own outfit changes.
- The Wardrobe editor shows its own status and retry messages. A missing Shape dependency is explained on the Shape controls.
- The panel has its own DirectX presentation path. ReShade drawing and the Two Reasons Body Physics handoff activate when their plugins are loaded and available.
- Hex Appeal attaches to Crimson Desert's own DXGI factory calls, leaving the shared factory table alone. This addresses the startup stack overflow seen with Steam Overlay enabled.
- Shaping can check the live body when a mount report is missing. Custom skin matching still uses DMM's mounted file routing.

## 1.0.8

- Increased the default text and panel size at 4K.
- Fixed a potential crash when increasing **Settings → Scale**. The panel uses the same font texture at every size and resizes when you release the slider.

## 1.0.7

- Fixed witch detection on some mod setups where **Shape** and **Wardrobe** showed Ready but could not find a witch. Hex Appeal now checks the witch's live appearance if the usual name lookup fails.
- Fixed a case where the native **[HA] Wardrobe** prompt never appeared. The interaction system now starts as soon as the game's interaction table is ready.
- Fixed **Hide outfit** and outfit switching getting stuck after Wardrobe opened. The fix was confirmed on Bari and Elowen.
- Fixed Wardrobe outfit switching and native prompts when Live Transmog or FAPS shares the same game update.
- Added a menu rendering fallback for setups where several mods share the graphics factory.
- Opening **[HA] Wardrobe** from a character's interaction prompts hides the game's HUD for the preview. The HUD comes back when the editor closes.

The first 1.0.7 test build resolved the witch detection report on the affected loadout.

## 1.0.6

- Added **Settings → Outfit update range** for outfit assembly, slot visibility and base underwear. It defaults to 45 m and goes up to 120 m for loaded characters. Set it to 0 to use the original update behavior.
- Expanded automatic Wardrobe discovery for mounted outfit items and refits. The picker uses available names and icons and flags pieces whose fit is unverified.
- Improved Wardrobe startup compatibility with Character Creator and Pywel Wardrobe. Hex Appeal retries when an active outfit hook is still being initialized.
- Supports the existing Woosa & Maegu add-on 1.0.0. Outfit range applies to enabled add-on characters too.

## 1.0.5

- Added optional character packs. Enabled entries appear beneath the witches and join **ALL CHARACTERS** for supported batch edits. **Settings → Character patches** shows each pack's own version and validation status.
- Add-on characters follow the core body mode and use the shared Shape and Wardrobe controls, with their own saved outfits and visibility settings.
- Native **[HA] Wardrobe** holds the character facing you during preview on checked game builds. The preview keeps the stock wheel and selector hidden, then releases the interaction when the editor closes.
- Added equipped-item SDK controls to the regular Wardrobe editor, including registered variants and visibility.
- Improved mounted equipment discovery, outfit restoration and slot visibility after an actor rebuild.
- Compatible replacement bodies keep their own shaping and underwear routes. If the base underwear mesh has been removed, Wardrobe disables its checkbox and shows why.
- Kept **[SO] Combat** read-only while Spouse Overhaul has combat gear active. Improved Barber assembly and startup coexistence with other DirectX hooks.

Spouse Overhaul hair or outfit parts can still flicker briefly during transitions. In a Maegu test, changing Height in photo mode shifted her hood until normal movement resumed.

## 1.0.4

- Added saved **[HA] Wardrobe** outfits. Edit them in F9 or use the witch’s interaction menu on supported game builds. Search stock and compatible mounted items by name, ID or tag, with names and icons from the game or mod data.
- Added per-slot preview, clear, save and cancel. The equipped parts stay saved when you hide a slot or the whole outfit.
- Improved switching between **[CD] Vanilla**, **[HA] Wardrobe** and available Spouse Overhaul looks. Saved outfits restore as the witch reappears, and a pending outfit assembly stays local to that witch.
- Saving in Spouse Overhaul’s Barber updates the **[SO] Barbers** outfit in Wardrobe. Loading a game save refreshes the look from that save.
- Empty Combat Gear slots stay empty during preview and combat wear. The Barber look comes back when the combat gear comes off.
- Improved detection of external underwear rules. The global preference hands over control for each affected witch, and Hex Appeal retains the other witches’ settings.
- Added optional **Settings → Skin tone** matching for a custom Damiane VENUS skin. It covers supported witch face, collar and hand routes, starts off and takes effect after restart.
- Updated compatibility with Crimson Desert 2.05.01. Unverified dependency versions can still work when their required resources and native interfaces pass the checks.
- The Shape tab now shows when a saved change is waiting for the witch to appear.
- Added a bounded recovery attempt when a DXGI factory probe hits an access violation during startup.
- Grouped both body choices with automatic single selection in DMM and kept one shared ASI. Updated the optional SDK guides and source tools.

Known issue with Spouse Overhaul: hair can flicker briefly when opening Combat Gear or enabling Always Wear Gear.

## 1.0.3

- Added a Log tab showing feature checks, active mod names, mounted paths and relevant DMM merge or conflict entries. Reports can be saved or copied.
- Limited unsupported body routes to the affected witch. All witches applies to the supported bodies and preserves the others' saved profiles.
- Added manually selected Instant Surgery presets to the HEX slot and improved detection of scoped external underwear removers.
- Reworked Wardrobe outfit hide and restoration around delayed rebuilds. It can switch between Default and a recognized Spouse Overhaul barber outfit, with the visibility preferences kept separate.
- Kept the verified default outfit parts when a spouse unloads or changes actor. Wardrobe checks the new actor’s live outfit records before offering a switch.
- Gave Vanilla witch underwear separate stock texture paths so Damiane can use White Pearl without changing how the witches' underwear looks.
- Kept DMM's shared ASI and both body choices in one player download. The SDK includes the existing provider API and updated guides.
- Added an optional local skin setup to the source tools. It can match brightness without copying textures or isolate game or user-supplied albedo locally.

## 1.0.2

### Player download

- Fixed DMM import grouping: VENUS and Vanilla stay inside one Hex Appeal group.
- Set the package, body options and ASI version to 1.0.2. The plugin now has Windows file/product version metadata too.
- Clarified clean installation after removing the archived package, while preserving saved settings.
- Retained the tested body resources, shaping and Wardrobe functionality from 1.0.1.

### Wardrobe SDK download

- Updated package verification and installation guidance. The public API and examples are unchanged.

### Source download

- Corrected the archive builder, added manifest-layout assertions and recorded actual DMM import/mount results.
- Added plugin version resources and an offline check for the displayed version.
- Updated beginner installation and upgrade documentation.

## 1.0.1

### Player download

- Corrected the archive import layout so DMM keeps VENUS and Vanilla inside one Hex Appeal group. Runtime and game resources are unchanged by this packaging correction.
- Clarified the 1.0.0 upgrade cleanup: keep saved settings, remove obsolete package/plugin copies, then select one current body option and mount.
- Added [01] VENUS and [02] Vanilla in one download, sharing the F9 runtime. Enable one body choice.
- Vanilla midpoint preserves each witch's original body, scale and customization. VENUS can remain installed separately for Damiane.
- Isolated witch boots from Damiane Armor Refit Pack; isolated original Vanilla bodies and underwear from shared body/armour replacements.
- Added complete body prefab lookup and definition resources, fixing the world-load crash and invisible witches found during private testing.
- Expanded preset discovery through DMM's configured storage, active source paths, portable locations and Instant Surgery folders. Nested/renamed presets and manual file/folder paths are supported.
- Kept baseline/FLAT, independent Height/Weight and Wardrobe available when their requirements pass, even without the external Hex preset. Vanilla does not require VENUS.
- Corrected proportion-slider padding and full track backgrounds, visibility labels, centred witch names and warning glyphs.

### Wardrobe SDK download

- Added optional OutfitPolicy v1 and HA_REPLACE_BASE_UNDERWEAR. The original ABI tables remain unchanged.
- Added replace_base_underwear definitions, capability checks and rollback in the generator/helper.
- An owned custom outfit can suppress natural base underwear without changing the saved user preference. Hide outfit can keep custom underwear; Hide base bra & panties hides its linked controls.
- Expanded ownership, restoration and unsupported-runtime tests, beginner guidance and complete implementation examples. Custom assets still need their own rendered checks.

### Source download

- Corrected the player ZIP builder and added checks for the main manifest beside its single folder. Documented actual DMM import and upgrade results.
- Included the complete original runtime, SDK, tools and source tests for these changes.
- Added original-game resource builders/validators and dual-choice packaging with tested-resource allowlists.
- Updated body-choice, missing-dependency, SDK, installation and verification documentation. Downloaded dependencies, private research/permissions, compiled plugins and game model outputs remain excluded from source.

## 1.0.0

First Nexus release.

- Applies the separately installed VENUS Human or Elf Body to the witches, with independent HEX / baseline / FLAT top and bottom shapes, all nine combinations, continuous blends and automatic saves.
- Independent Height and Weight. Weight needs no outfit mod.
- Seven appearance patches and forty checked default outfit refits.
- **Hide outfit** removes recognised default clothing, hats and footwear. **Hide base bra & panties** is a separate control.
- Per-witch and All witches settings, the F9 panel, input blocking, Minimise and quick-panel pins.
- The Wardrobe SDK for outfit creators, with an optional outfit visibility extension and separate clothing and underwear flags.
- Dependency checks that keep unknown but readable versions usable with a warning.
