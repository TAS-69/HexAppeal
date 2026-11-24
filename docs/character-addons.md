# Make a Hex Appeal character add-on

Character add-ons are separate downloads. Import them under **Mods** in DMM, mount, then start Crimson Desert. **F9 → Settings → Character patches** shows the detected patches, their versions and which characters are available. Patches start enabled unless their manifest sets a different default. Uncheck a patch to remove its entries from the selector while keeping their saved settings. Enabled characters appear alphabetically below the witches and join **ALL CHARACTERS** for shaping and visibility changes.

## File format

Use a `.tsv` file with literal tabs between fields and an exact version header. The file can define one character or several. Lines beginning with `#` are comments except for the `#variant`, `#patch`, `#multiCharacter` and `#defaultEnabled` directives.

```text
HexAppealCharacters	1
character	yourname_example	Example NPC	1234	NHW_Example_NPC_1234	character/appearance/3_npc/example/example_001.app_xml
default	yourname_example	Default robe	example_ub_robe
item	yourname_example	Travel robe	example_ub_travel
```

- **character** declares the stable add-on character ID, display name, numeric game record key, exact internal record name and full appearance path. Keep the stable ID when updating a pack so saved outfits still belong to the same person.
- **#variant** declares another exact record that shares a character's appearance and equipment catalogue. Use the same fields as `character`, with the earlier character's ID in place of the appearance path. Give the variant a distinct ID and display name. Each record gets its own live outfit session. By default all of a character's records share one F9 entry and the same Shape and Wardrobe settings. Players can untick **Combine instances** beside the character's name in F9 to choose a record and edit its settings separately. While combined, a **[HA] Wardrobe** preview shows on every loaded record, and Cancel returns each to its own previous outfit. The display name appears in the record picker. Older builds skip this directive and continue to load the main entry.
- **default** declares one of the stock equipped parts for that character. Repeat it for each supported default part and use the exact registered prefab name.
- **item** adds an optional checked equipment choice. It must already be registered by the game or by a separately mounted outfit mod. The prefab name needs a recognised slot marker, such as `_ub_` for Upper body or `_foot_` for Feet.

Give each character its own identity. Duplicate IDs, record keys and record names conflict. To share an appearance within a pack, declare its additional records as variants. Hex Appeal requires the exact record name for these entries, so each loaded copy of the character keeps its own outfit session. Built-in witch record keys and appearance paths are reserved. The Characters tab and startup log report malformed or conflicting entries.

## Choose the pack's F9 controls

In Hex Appeal 1.1.3, add `#multiCharacter` and one tab followed by `0` or `1` before the character entries.

- `0` is for one NPC. Declare one `character` row and use `#variant` for additional records. F9 shows **Combine instances** when the pack has multiple records for that character.
- `1` is for separate NPCs. Give each NPC her own `character` row and defaults. The pack appears once in the F9 sidebar. Use the dropdown at the top of its page to choose an NPC. Choosing one switches the editor while the other NPCs keep their saved looks.

When the option is omitted, packs with multiple independent NPCs use the dropdown. Packs with variants keep their instance controls. Older core versions skip the option and read the character entries they support.

There are 250 add-on slots alongside the six built-in characters. Each `character` and `#variant` row uses one slot.

## Choose whether the pack starts enabled

Hex Appeal 1.1.3 accepts `#defaultEnabled`, followed by a literal tab and `0` or `1`, once before the character entries. Use `0` to start with the pack unchecked in **F9 → Settings → Character patches**. Use `1` or omit the directive to start enabled. This applies to the whole pack, including its dropdown and any variants. Disabled packs still count toward the roster limit so they can be enabled from F9.

The default applies when the player has no saved choice for that manifest filename. Once they tick or untick the pack, their choice takes precedence on future launches. Older builds ignore the directive and start enabled if the player has no saved choice.

## Package for DMM

From the source checkout, package your TSV and instructions together.

```powershell
python tools/package_character_addon.py example.tsv README.md Example.zip --title "Hex Appeal - Example NPC" --version 1.0.5 --minimum-core 1.0.5
```

The packager creates a root `mod.json` and one XML resource under `files/0009/character/hexappeal/characters/`. The resource wraps the TSV in a versioned CDATA section. Each ZIP uses the TSV's unique filename as the mounted resource name. The main runtime discovers DMM add-ons from the active mount list. The old manual preview path in `%LOCALAPPDATA%\HexAppeal\Characters` remains available. If both formats are installed under the same filename, the mounted copy takes priority.

The packager writes the patch name, version and minimum core version into the mounted manifest. To set a different F9 label from your DMM title, put a `#patch` row immediately below `HexAppealCharacters`. Separate the directive, label, pack version and minimum core version with literal tabs. The versions must match the packaging arguments. Hex Appeal accepts older patches when their records pass validation and the minimum is satisfied. Legacy manifests display their version as undeclared. Set the minimum only when the patch needs a feature from that core release.

Hex Appeal checks the exact game record and live appearance before editing. Supply the stock parts for **[CD] Vanilla** and add optional named equipment when useful. An add-on can name an NPC appearance or a female player-class appearance (`character/appearance/1_pc/2_phw/`), except Damiane's own. In **[01] VENUS**, the runtime gives the separately mounted Venus Body for Damiane to appearances on the standard female body, including character variants of it such as Nevra Yailar's. **[02] Vanilla** keeps the original body; a character who normally wears Damiane's body, such as Naira, gets Damiane's original body from the Vanilla option, as Sylvia does. An appearance with its own body replacement retains that replacement. Wardrobe uses the loaded body to choose the same equipment catalogue as the witches. SDK item options are available when their host piece is equipped in **[HA] Wardrobe**. Keep outfit assets in their own downloads if permission is limited to interoperability.

Add-on characters use the same Wardrobe panel and edit, save and restore workflow as witches. Outfit slots store prefab names and SDK options store stable IDs. Each add-on character has an independent profile keyed by its manifest ID. Per-slot visibility, **Hide outfit**, and base underwear are separate preferences.

When **Match Damiane’s VENUS skin** is enabled, compatible add-ons can use the same optional skin matching. Each character gets a private copy of any changed customization, so another NPC can keep using the original.

## What to verify

Try the pack on a clean mount first. Select each character in the main roster and check Shape against their original proportions. Try the native **[HA] Wardrobe** prompt, preview and cancel a slot change, apply it, then reload and switch back to **[CD] Vanilla**. Test **Hide outfit** and **Hide base bra & panties** separately. Disable and re-enable the patch to check that preferences return. Barber, combat outfit and custom skin features depend on the applicable game routes and assets.
