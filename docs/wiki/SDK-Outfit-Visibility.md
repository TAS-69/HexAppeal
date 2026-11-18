# SDK: Outfit visibility

Hide outfit is optional. Existing providers keep their current behavior until they opt in. No provider assets are copied into Hex Appeal.

## Engine visibility parts

Give your clothing an actual engine visibility part and declare a per-witch toggle with `follow_outfit_hide: true`:

```json
{
  "id": "clothing",
  "label": "Clothing",
  "type": "toggle",
  "default": true,
  "follow_outfit_hide": true
}
```

Bind every clothing part that should disappear:

```json
"part_visibility": [
  {"control": "clothing", "part": "MyDress"},
  {"control": "clothing", "part": "MyOutfitAccessories"}
]
```

These are placeholder engine keys. Supply your own real prefab, models and part keys. A material name or modelling object name is insufficient. Keep a complete body underneath; hiding clothing cannot rebuild skin that your outfit removes.

Keep bra/briefs controls linked with `follow_base_underwear: true`, on separate parts. Hide outfit leaves them alone. For full nudity, the player must also tick **Hide base bra & panties**. Clothing, underwear and colour choices retain their stored values while the matching override is active.

If these pieces replace the normal base pair for this outfit, add top-level `replace_base_underwear: true`. The optional [base underwear policy](SDK-Base-Underwear-Policy.md) suppresses the normal pair only while Wardrobe owns this replacement. It does not alter the player's saved hide preference or suppress your linked custom pieces by itself.

The C equivalent is `HA_FOLLOW_OUTFIT_HIDE` on a per-witch `HA_TOGGLE`. Clothing and underwear follow flags are mutually exclusive on one control. Separate them so all four clothing/underwear combinations work. If a mesh combines them, provide separate visibility parts or complete prefab states; do not opt in until the supported states render correctly.

## Authored prefab combinations

A clothing control can participate in `native_controls`. Supply **every** state, including the clothing-off states for each colour and other mapped control. Wardrobe selects the supplied prefab; it does not generate meshes or materials. Use this route if simple part visibility cannot preserve correct skin coverage.

The [parts example](https://github.com/TAS-69/HexAppeal/blob/main/runtime/sdk/examples/parts-outfit.json) demonstrates the definition format. Generate and compile the registration code normally.

## A complete clothing/underwear matrix

For an outfit that needs alternate prefabs, use two independent toggles. A two-colour example needs eight authored states:

```json
{
  "controls": [
    {"id": "colour", "label": "Colour", "type": "choice", "default": "white",
     "options": [{"id": "white", "label": "White"}, {"id": "black", "label": "Black"}]},
    {"id": "clothing", "label": "Clothing", "type": "toggle", "default": true, "follow_outfit_hide": true},
    {"id": "underwear", "label": "Outfit underwear", "type": "toggle", "default": true, "follow_base_underwear": true}
  ],
  "native_controls": ["colour", "clothing", "underwear"],
  "states": [
    {"values": {"colour": "white", "clothing": false, "underwear": false}, "prefab": "my_white_body"},
    {"values": {"colour": "white", "clothing": false, "underwear": true}, "prefab": "my_white_underwear"},
    {"values": {"colour": "white", "clothing": true, "underwear": false}, "prefab": "my_white_clothes"},
    {"values": {"colour": "white", "clothing": true, "underwear": true}, "prefab": "my_white_complete"},
    {"values": {"colour": "black", "clothing": false, "underwear": false}, "prefab": "my_black_body"},
    {"values": {"colour": "black", "clothing": false, "underwear": true}, "prefab": "my_black_underwear"},
    {"values": {"colour": "black", "clothing": true, "underwear": false}, "prefab": "my_black_clothes"},
    {"values": {"colour": "black", "clothing": true, "underwear": true}, "prefab": "my_black_complete"}
  ]
}
```

This is the controls/state portion of a definition. Add your normal schema, outfit ID, label, default prefab, credits and requirements. The default prefab must match `my_white_complete` in this example. These names represent files your outfit package supplies; the generator creates registration code only. Test every state with the actual body and every authored detail level. A body-only prefab must retain complete skin coverage and use compatible material/visibility rules.

## A custom renderer

Discover the optional extension and poll effective values on your normal update:

```cpp
hexappeal::WardrobeClient wardrobe;
if (wardrobe.Connect() && wardrobe.HasOutfitVisibility()) {
    HA_OutfitVisibilityState state{};
    if (wardrobe.OutfitVisibilityState(witch, state) == HA_OK) {
        HA_EffectiveControlValue clothing{};
        if (wardrobe.Effective(witch, "myname/outfit", "clothing", clothing) == HA_OK) {
            SetMyOwnedClothingVisible(clothing.value != 0);
        }
    }
}
```

`SetMyOwnedClothingVisible` is your own renderer bridge, not a game or SDK function. Validate the selected actor, renderer and outfit ownership before writing. Query Hide outfit's revision as well as the control and Base underwear revisions when caching effective values. Apply on the correct game thread and restore natural visibility when your provider unloads.

`HexAppeal_GetOutfitVisibilityAPI(1)` supplies `GetState` and `SetHidden`. `WardrobeClient.HideOutfit(witch, true)` changes and saves the setting; `witch = -1` addresses All witches. The original Wardrobe and Base underwear tables retain their sizes and versions. Check `HasOutfitVisibility()` when supporting an older Hex Appeal build.

## Verify your patch

Test on/off, repeated toggles, save reload, colour changes, every supplied prefab state, different body combinations and detail levels. Check the complete exposed body from front and back. Check all four independent clothing/underwear combinations. Disabling Hide outfit must restore clothing without changing underwear. Confirm that non-participating outfits are left alone. These are asset checks in addition to compiling the provider.
