# Wardrobe

Wardrobe is a shared **default witch outfit replacer menu**. Choose a witch or All witches, then select an installed replacement. Names come from the outfit registration: the actual mod title, or `Mod Title / Outfit Name` when one mod supplies several outfits.

## Default and Dancer

**Default** restores the recognized witch default. The separate Outfit Refits add-on determines whether that default uses Hex Appeal's VENUS baseline refit.

The optional **Sexy Witches of Pywel Reshaped / Dancer Dress** uses the original external outfit. Each witch can independently use Default or Dancer. It does not take ownership of arbitrary custom spouse/barber clothing.

In the next combined package, run **Setup Dancer.cmd before the first mount** to connect the original dependency to the main Hex Appeal entry. No separate Dancer resource entry is needed. This optional setup uses Python 3.12+ and leaves original file contents unchanged. [Installation](Installation.md) covers the steps and the older published 1.0.0 setup.

Dancer supplies **Original White, Midnight Black, Crimson Red, Arcane Pink and Emerald Green**. Its texture route is shared, so its colour is **global**, even when outfit selection is per witch. A change saves the colour, verifies the external resource and queues a Default → selected refresh automatically. Allow a short settling interval. Rapid changes are combined. Saved outfit selections restore after loading into the world.

## Base underwear

**Hide base bra & panties** controls the game's combined base underwear part. It is separate from an outfit's own accessories. Compatible SDK outfits can also follow this choice without losing their individual saved garment settings.

This cannot remove a top or briefs embedded inside another mesh. Built-in Dancer has no separate embedded bra/top removal control. Its current integration supplies outfit switching and original colours only. Outfit-specific removal needs an outfit patch with suitable assets; see [SDK asset patches](SDK-asset-patches.md).

## Options from other outfits

An outfit provider can supply choices, toggles and colours. Wardrobe shows only the options that provider declares. Some options are per witch; shared resources are global. Right-click an individual control to pin it to the quick panel.

Unavailable providers retain their saved selection and wait. They do not silently replace it with another outfit. **Default** remains the way back. A registered option is not proof that an author's assets fit every shape; check that outfit's requirements and tested scope.
