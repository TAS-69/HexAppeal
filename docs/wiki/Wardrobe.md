# Wardrobe

Wardrobe is a shared **default witch outfit replacer menu**. Choose one witch or All witches, then select Default or a replacement registered by an installed provider.

**Default** uses the currently mounted original witch outfit. The player package includes forty VENUS baseline default-garment refits; file priority determines which overlapping refit is served.

The core release has **no built-in Dancer replacement or colour controls**. Its dependency detection and setup are excluded pending permission. Any future integration belongs to a separate patch. No extra outfit download is needed for Weight or the core editor.

## Outfit providers

A provider registers the actual mod title, or `Mod Title / Outfit Name` when one mod supplies several outfits. Wardrobe shows only the choices, toggles and variants that provider supports. Shared resources may use global controls. Right-click supported options to pin them individually.

Provider assets stay in the separate outfit mod. Registration alone cannot prove fitting, materials or moving cloth. Wardrobe avoids arbitrary custom spouse/barber equipment and switches only recognized defaults or its own proven replacements.

Unavailable providers retain their saved selection and wait. Older built-in outfit choices also remain saved as unavailable. Choose **Default** to return to the mounted original outfit.

## Base underwear

**Hide base bra & panties** controls the game's combined base underwear part. Compatible SDK outfits can also follow this choice while retaining their individual saved garment preferences.

The switch cannot remove geometry embedded inside another mesh or reconstruct omitted skin. Outfit-specific removal needs independently addressable parts, authored alternatives or a checked provider renderer. See [SDK asset patches](SDK-asset-patches.md) and [advanced examples](SDK-examples.md).
