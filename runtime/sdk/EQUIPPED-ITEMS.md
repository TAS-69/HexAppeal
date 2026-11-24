# Add SDK controls to a regular Wardrobe item

The SDK can attach options to an item players equip in **[HA] Wardrobe**. The same picker continues to work with ordinary equipment. A supported item gains its own section under **Equipped item options** as soon as you equip it. You can preview a colour and turn the piece on or off while editing. **Apply and save** keeps both your gear and your choices. **Cancel** restores the previous look and choices. You can also change an equipped item's options from **F9 → Wardrobe** while **[HA] Wardrobe** is active.

Equipped item controls require a Hex Appeal build that exports `HexAppeal_BindWardrobeItemV1`: public 1.0.5 and later. Hex Appeal 1.0.4 and earlier only register full looks in the F9 outfit selector, and existing providers continue to use that route.

## 1. Add the equipment as usual

Keep your mod's normal DMM layout. Register the item with its slot, in-game name, icon and the default appearance. The picker uses the game and mounted item data to find the item and its appearance. The SDK definition supplies the extra controls.

For the cloak in this example, point the regular Cloak item at `example_cloak_emberglass_black`. Mount the jade appearance alongside it. Both names must resolve in the game part registry. Keep the same slot and compatible body fit in both variants. You can expose just the black cloak as a normal item and use the jade appearance through the colour option.

The equipped item binding matches a single registered appearance in its slot. Replacing that item hides its options. Re-equipping it brings back the saved choices for that witch. Independent equipment such as a cloak, belt and boots can each have its own binding. Assembled full looks with multiple components in the same slot can use the full-look SDK route.

## 2. Declare the controls

The complete [cloak definition](examples/equipped-cloak.json) defines one regular item, two colourways and one independent visibility control. The important fields are:

```json
{
  "schema": 1,
  "id": "example/emberglass-cloak",
  "label": "Emberglass Cloak",
  "prefab": "example_cloak_emberglass_black",
  "equip_item": true,
  "item_visibility": "visible",
  "controls": [
    {
      "id": "colour",
      "label": "Cloak colour",
      "type": "choice",
      "default": "black",
      "prefab_variant": true,
      "options": [
        {"id": "black", "label": "Black", "prefab": "example_cloak_emberglass_black"},
        {"id": "jade", "label": "Jade", "prefab": "example_cloak_emberglass_jade"}
      ]
    },
    {
      "id": "visible",
      "label": "Show cloak",
      "type": "toggle",
      "default": true,
      "follow_outfit_hide": true
    }
  ]
}
```

The `prefab` matches the item that the picker equips. Changing colour selects a rendered appearance while the slot keeps its original item. The visibility control hides the matching rendered piece and keeps the item and colour in place. Hide outfit temporarily overrides the saved **Show cloak** setting. The player gets that saved setting back when Hide outfit is off.

## 3. Register the item controls from your mod

Generate the header with the SDK development generator:

```powershell
python tools/generate_provider.py examples/equipped-cloak.json --output build/EquippedDefinition.hpp
```

Compile the [equipped item provider](examples/EquippedItemProvider.cpp) or use its registration code in your existing x64 ASI. The generated registration calls `BindItem` while the provider is still unavailable. The C equivalent is the optional `HexAppeal_BindWardrobeItemV1(token, "visible")` export after registering the definition and before setting availability. Pass an empty string for an item with variants alone.

Run the update after loader initialization. Pass `resourcesReady=true` after checking that the default appearance and its variants resolve from your mounted resources. Pass a readable reason if they are unavailable, such as “Emberglass Cloak's Jade material is missing from the DMM mount.” Check the capability before registering, and leave the regular item available if Hex Appeal or the extension is absent. The optional ASI can ship inside your outfit ZIP or in a separate compatibility patch.

Keep the outfit, control and option IDs stable. You can update labels independently. The equipped item stores choice IDs, so reordering its list preserves the selected colour. If the mounted variant is missing at runtime, Wardrobe uses the equipped appearance. Late provider availability re-resolves the visible look.

Applied item options use version 2 of `wardrobe-slots.txt`. This build can read the earlier format. Keep a copy of your settings before going back from 1.0.5 or later to 1.0.4 or earlier.

## Combine the features

| What you are shipping | Definition | Player result |
| --- | --- | --- |
| Normal equipment | Regular item and appearance | Stock equipment picker behavior |
| Item with an on/off accessory | `equip_item` and `item_visibility` pointing to a toggle | Show or hide a piece independently of the rest of the outfit |
| Colours or cuts | `equip_item` and a single `prefab_variant` choice | Swap the equipped item's appearance |
| Colours and cuts together | `equip_item`, `native_controls` and complete `states` | Every colour can render with every cut |
| Item variants plus visibility | Either variant route plus a separate `item_visibility` toggle | A hidden item keeps its current variant and slot |
| Independent custom bra or briefs item | A separate equipable appearance with a visibility toggle flagged `follow_base_underwear` | Hide outfit leaves the custom piece alone. Hide base bra & panties hides it when Hex Appeal controls underwear |
| Several optional accessories | One binding for each independently equipable piece | Each occupied slot exposes its own choices and saves them per witch |
| Folded hood or clasp built into the cloak mesh | A native choice or matrix with complete prefabs for each assembled look | The cloak can switch between authored versions while staying equipped |

For a four-state cloak with **colour × cut**, change `prefab` to the default black fitted cloak. Remove `prefab_variant` and the per-option prefab fields from the colour definition, add a `cut` choice and add every combination. Keep `item_visibility` and its toggle separate:

```json
{
  "schema": 1,
  "id": "example/emberglass-cloak",
  "label": "Emberglass Cloak",
  "prefab": "example_cloak_black_fitted",
  "equip_item": true,
  "item_visibility": "visible",
  "controls": [
    {"id": "colour", "label": "Cloak colour", "type": "choice", "default": "black",
     "options": [{"id": "black", "label": "Black"}, {"id": "jade", "label": "Jade"}]},
    {"id": "cut", "label": "Cloak cut", "type": "choice", "default": "fitted",
     "options": [{"id": "fitted", "label": "Fitted"}, {"id": "long", "label": "Long"}]},
    {"id": "visible", "label": "Show cloak", "type": "toggle", "default": true, "follow_outfit_hide": true}
  ],
  "native_controls": ["colour", "cut"],
  "states": [
    {"values": {"colour": "black", "cut": "fitted"}, "prefab": "example_cloak_black_fitted"},
    {"values": {"colour": "black", "cut": "long"}, "prefab": "example_cloak_black_long"},
    {"values": {"colour": "jade", "cut": "fitted"}, "prefab": "example_cloak_jade_fitted"},
    {"values": {"colour": "jade", "cut": "long"}, "prefab": "example_cloak_jade_long"}
  ]
}
```

These two definitions are alternatives for the same item. Keep one registration for the default appearance. When extending a shipped item, keep the original equipped prefab and stable colour option IDs.

## Full looks and equipped items

An equipped item opt-in uses Wardrobe's checked slot assembly and visibility paths. It accepts per-witch variants and the equipped piece's visibility. The full-look route is also available for binding independent engine visibility keys inside an assembly, replacing the game's base underwear while the registered look owns it, or applying provider-controlled material changes and global controls. If your mod includes both a regular item and a full look, give the two registrations distinct IDs.

For a complete look with custom underwear that stays visible when the dress is hidden, use separate writable parts or complete prefab states. A custom single-slot bra or briefs item can follow effective Base underwear visibility. Suppressing the game's original underwear with the full-look SDK policy requires a separately registered full look. Design and test layering accordingly.

[Native parts, prefab matrices and a custom renderer](ADVANCED-EXAMPLES.md) has working provider code for those integrations. [Outfit visibility](OUTFIT-VISIBILITY.md) covers independent clothing and underwear controls. [Base underwear policy](BASE-UNDERWEAR-POLICY.md) covers outfit-scoped suppression.

## Check the result in-game

1. Import through DMM and equip the original item in **[HA] Wardrobe**. Check its name, icon and slot. Verify a stock item next to it and switch the stock item without losing the cloak's state.
2. Try each colour and cut on two witches at once. Test rapid changes, movement, camera distance and covered skin. Confirm both renderer instances can show different appearances under the same light.
3. For an underwear piece, check all four Hide outfit and Base underwear combinations, then repeat with its own button off. Change a setting while it is overridden and verify your saved choice takes effect afterward.
4. Preview a variation then Cancel. Preview it again and Apply. Switch to Vanilla or Barbers and back. Reload the save and revisit an unloaded witch. Verify every item returns to its slot and the applied choices return.
5. Test your regular outfit with the provider ASI disabled. Test a missing variant on a disposable profile. The regular item should stay usable while the additional option gives a clear status.
6. Test the body variants you claim to fit. Check cloth, collision, materials and geometry at the neck, waist and joints. The definition generator checks the option table. The game pass checks what players see.
