# Character add-on example for Rucchia and Stranger

This pack registers Rucchia (429) and the Stranger (1000212) as two game records for one character. Both use the same mounted appearance and outfit catalogue, with separate outfit sessions in the game.

## Build

From the exported Wardrobe SDK folder, run this with Python 3. For your own pack, change the title, DMM author name and manifest identifiers.

```powershell
python tools/package_character_addon.py examples/character-variants/rucchia-example.tsv examples/character-variants/README.md HexAppeal-Rucchia-Variant-Example-1.0.0.zip --title "Hex Appeal - Rucchia Variant Example" --author TASx69 --version 1.0.0 --minimum-core 1.1.3 --license-file LICENSE
```

When using the source checkout, run the root `tools/package_character_addon.py` and prefix the two example paths with `runtime/sdk/`. The ZIP puts the manifest under `files/0009/character/hexappeal/characters/`, where Hex Appeal finds it through the DMM mount. The example contains the character manifest and instructions, and the main Hex Appeal ASI supplies the runtime.

## Adapt the manifest

- Keep literal tabs and the `HexAppealCharacters` version header. Pick a unique TSV filename and stable IDs for the main record and each variant. Changing an ID on an existing pack creates a new settings profile.
- In the `character` row, give the display name, numeric game record ID, exact internal record name and full appearance path.
- In the `#variant` row, the last column refers back to the main record's ID. Enter the variant's own numeric key and internal name. This joins the records only where they share the same appearance and equipment catalogue.
- List the stock prefabs under `default` on the main record. `[CD] Vanilla` uses this outfit. **Follow game** restores the outfit captured for each live record.
- Set the minimum core to 1.1.3 for multi-record sharing. A variant uses one of the 24 supported add-on records even when it shares an F9 entry.

## Try it in game

Use Hex Appeal 1.1.3 with one body option. Disable the regular Rucchia pack when mounting the example since both claim the same game records. Import the example under DMM's Mods tab, enable it, mount and restart.

1. Open **F9 → Settings → Character patches**. The example should report two valid records.
2. With both records loaded, select Rucchia in F9. With **Combine instances** ticked, a preview or saved outfit applies to both. Cancel returns each to what she wore when the preview began.
3. Apply and save a temporary outfit, then select **Follow game**. Each record should return to her own game outfit.
4. Untick **Combine instances**. Choose **Rucchia** or **Rucchia (Stranger)** in the picker. Repeat with one selected record and check that the other keeps her outfit.
5. Repeat after the selected record leaves and re-enters your outfit update range. To inspect the live record IDs and outfit acknowledgements, use **F9 → Log → Save report**.

See the [character add-on format guide](../../CHARACTER-ADDONS.md) for the full manifest rules.
