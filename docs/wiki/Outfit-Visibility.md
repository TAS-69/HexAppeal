# Outfit visibility

Open **F9 → Wardrobe → Outfit visibility** and tick **Hide outfit**. Choose one witch or **All witches**. Changes apply and save automatically.

This masks the recognised clothing slots in the renderer. To hide a single slot, open **Wardrobe → Outfit slots**. Uncheck its box to restore visibility. Equipped part IDs and saved outfits stay in their slots.

**Base underwear is independent.** Use **Hide base bra & panties** in its own section. The checked base underwear is exposed by the game as one combined part, so this control remains combined. An outfit provider may offer separate bra and panties options when its own assets supply separate parts.

| Hide outfit | Hide base bra & panties | Result |
| --- | --- | --- |
| Off | Off | Outfit and normal base underwear rules |
| On | Off | Outfit hidden; base underwear retained |
| Off | On | Outfit retained; base underwear hidden |
| On | On | Both hidden |

Registered clothing parts can be masked while equipped. A mod that combines parts into one draw group shares that group’s visibility. If a remover strips a part from the mounted asset, the visibility switch has no mesh to show. SDK providers can supply their own controls for additional parts.

Right-click **Hide outfit** to pin it to the quick panel, or use **Settings → Quick panel pins**.

If an outfit operation is not acknowledged, Hex Appeal stops further writes and reports the error. Restart the game before retrying. Missing dependencies block affected controls; an unverified version alone remains a warning.

See [SDK: Outfit visibility](SDK-Outfit-Visibility.md) to add support to an outfit.
