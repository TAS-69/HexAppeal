# Make a Hex Appeal character add-on

Character add-ons are separate downloads. Import them under **Mods** in DMM, mount, then start Crimson Desert. **F9 → Settings → Character patches** shows the detected patches, their versions and which characters are available. New patches start enabled. Uncheck a patch to remove its entries from the selector while keeping their saved settings. Enabled characters appear alphabetically below the witches and join **ALL CHARACTERS** for shaping and visibility changes.

## File format

Use a `.tsv` file with literal tabs between fields and an exact version header. The file can define one character or several. Lines beginning with `#` are comments.

```text
HexAppealCharacters	1
character	yourname_example	Example NPC	1234	NHW_Example_NPC_1234	character/appearance/3_npc/example/example_001.app_xml
default	yourname_example	Default robe	example_ub_robe
item	yourname_example	Travel robe	example_ub_travel
```

- **character** declares the stable add-on character ID, display name, numeric game record key, exact internal record name and full appearance path. Keep the stable ID when updating a pack so saved outfits still belong to the same person.
- **default** declares one of the stock equipped parts for that character. Repeat it for each supported default part and use the exact registered prefab name.
- **item** adds an optional checked equipment choice. It must already be registered by the game or by a separately mounted outfit mod. The prefab name needs a recognised slot marker, such as `_ub_` for Upper body or `_foot_` for Feet.

Give each character its own identity. If two add-ons claim the same ID, record key, record name or appearance path, the conflicting entries are skipped. Built-in witch record keys and appearance paths are reserved. The Characters tab and startup log report malformed or conflicting entries.

## Package for DMM

From the source checkout, package your TSV and instructions together.

```powershell
python tools/package_character_addon.py example.tsv README.md Example.zip --title "Hex Appeal - Example NPC" --version 1.0.5 --minimum-core 1.0.5
```

The packager creates a root `mod.json` and one XML resource under `files/0009/character/hexappeal/characters/`. The resource wraps the TSV in a versioned CDATA section. Each ZIP uses the TSV's unique filename as the mounted resource name. The main runtime discovers DMM add-ons from the active mount list. The old manual preview path in `%LOCALAPPDATA%\HexAppeal\Characters` remains available. If both formats are installed under the same filename, the mounted copy takes priority.

The packager writes the patch name, version and minimum core version into the mounted manifest. Hex Appeal accepts older patches when their records pass validation and the minimum is satisfied. Legacy manifests display their version as undeclared. Set the minimum only when the patch needs a feature from that core release.

Hex Appeal checks the exact game record and live appearance before editing. Supply the stock parts for **[CD] Vanilla** and add optional named equipment when useful. In **[01] VENUS**, the runtime uses the separately mounted Venus Body for Damiane for supported PHW appearances. **[02] Vanilla** keeps the original body. An appearance with its own body replacement retains that replacement. Wardrobe uses the loaded body to choose the same equipment catalogue as the witches. SDK item options are available when their host piece is equipped in **[HA] Wardrobe**. Keep outfit assets in their own downloads if permission is limited to interoperability.

Add-on characters use the same Wardrobe panel and edit, save and restore workflow as witches. Outfit slots store prefab names and SDK options store stable IDs. Each add-on character has an independent profile keyed by its manifest ID. Per-slot visibility, **Hide outfit**, and base underwear are separate preferences.

When **Match Damiane’s VENUS skin** is enabled, compatible add-ons can use the same optional skin matching. Each character gets a private copy of any changed customization, so another NPC can keep using the original.

## What to verify

Try the pack on a clean mount first. Select each character in the main roster and check Shape against their original proportions. Try the native **[HA] Wardrobe** prompt, preview and cancel a slot change, apply it, then reload and switch back to **[CD] Vanilla**. Test **Hide outfit** and **Hide base bra & panties** separately. Disable and re-enable the patch to check that preferences return. Barber, combat outfit and custom skin features depend on the applicable game routes and assets.
