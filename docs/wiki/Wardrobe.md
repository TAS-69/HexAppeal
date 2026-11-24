# Wardrobe

Open **F9 → Wardrobe**, choose a witch and pick a look. **[CD] Vanilla** uses her mounted default outfit. **[HA] Wardrobe** is the look you equip and save in Hex Appeal.

Choose **Edit [HA] Wardrobe** to preview and equip items. On supported game builds, look at a witch and hold the game’s focus key (**Ctrl** by default). Use the key shown beside **[HA] Wardrobe** in the outer interaction prompts. Search by item name, ID or tag, then select an item to preview it in its slot. **Apply and save** keeps the new look active. **Cancel** returns to the previous outfit. **Clear all slots** clears the preview after you confirm.

Opening Wardrobe from the interaction prompts hides the game HUD while you preview. The HUD returns when you close the editor.

The picker reads stock equipment from your installed game. It also looks for compatible items registered by mounted outfit mods. Names and icons come from the available game or mod data. The game resolves the saved part names against the current mount. If a part is unavailable after changing mods, Wardrobe reports its name and keeps the current outfit. Replace or clear that slot to use the rest of the look.

Use **F9 → Settings → Outfit update range** to change how far away outfit swaps and visibility updates run. The game must have the character loaded. A wider range can include more characters, including ones outside the camera view.

## Barber and Combat

**Follow game** leaves outfit changes with Crimson Desert and Spouse Overhaul. **[SO] Barbers** appears when Hex Appeal can match the saved Barber slots or recognise the equipped look on the spouse. Saving in the Barber refreshes that profile. Loading a save uses its own Barber look.

**[SO] Combat** appears in Wardrobe while Spouse Overhaul supplies the active combat look. The outfit selector is disabled during this time. Use Spouse Overhaul to edit combat gear or set **Always Wear Gear**. Your previous Hex Appeal profile is available again when Spouse Overhaul releases the combat look. Empty combat slots stay empty in preview and when wearing combat gear.

## Outfit providers

A provider registers the actual mod title, or `Mod Title / Outfit Name` when one mod supplies several outfits. Wardrobe shows only the choices, toggles and variants that provider supports. Shared resources may use global controls. Right-click supported options to pin them individually.

Provider assets stay in the separate outfit mod. Use the SDK for authored controls and variants. Equipped parts can also be saved without a provider. Fit and materials still depend on the mounted outfit, rig and body.

Unavailable providers retain their saved selection and wait. Older built-in outfit choices also remain saved as unavailable. Choose **[CD] Vanilla** to return to the mounted original outfit.

## Base underwear

**Hide base bra & panties** controls the game's combined base underwear part. Compatible SDK outfits can also follow this choice while retaining their individual saved garment preferences.

**Allow external control (all witches)** is on by default. Hex Appeal defers on each witch where it detects a mounted remover or a rule keeping the exposed base part hidden. The other witches still use their own Hex Appeal visibility settings. Wardrobe shows the mod's name when it can identify the source.

Untick the box to give Hex Appeal control for all witches. With **Hide outfit** on, unchecking **Hide base bra & panties** can show the existing base part. With the outfit on, its normal coverage rules apply. A base mesh removed by the mount can only show again if the mount supplies it. Rechecking the box hands control back wherever an external rule is detected. Your per-witch hide settings stay saved.

The switch cannot remove geometry embedded inside another mesh or reconstruct omitted skin. Outfit-specific removal needs independently addressable parts, authored alternatives or a checked provider renderer. See [SDK asset patches](SDK-asset-patches.md) and [advanced examples](SDK-examples.md).

## Hide outfit

**Hide outfit** changes visibility for equipped clothing slots. **Outfit slots** also lets you hide one slot at a time. Both controls leave the equipped and saved part IDs in their slots. Base underwear stays separate. See [Hide outfit](Outfit-Visibility.md) and [provider setup](SDK-Outfit-Visibility.md).
