# Outfit visibility

Open **F9 → Wardrobe → Outfit visibility** and tick **Hide outfit**. Choose one witch or **All witches**. Changes apply and save automatically.

This hides recognised default clothing, hat and footwear through the game's outfit system. Switching it off restores those outfit parts. Shape, Weight and outfit variants stay as selected.

**Base underwear is independent.** Use **Hide base bra & panties** in its own section. The checked base underwear is exposed by the game as one combined part, so this control remains combined. An outfit provider may offer separate bra and panties options when its own assets supply separate parts.

| Hide outfit | Hide base bra & panties | Result |
| --- | --- | --- |
| Off | Off | Outfit and normal base underwear rules |
| On | Off | Outfit hidden; base underwear retained |
| Off | On | Outfit retained; base underwear hidden |
| On | On | Both hidden |

Other outfits must explicitly support Hide outfit through the Wardrobe SDK. Unsupported custom spouse/barber outfits are retained; the setting waits for a supported outfit.

Right-click **Hide outfit** to pin it to the quick panel, or use **Settings → Quick panel pins**.

If an outfit operation is not acknowledged, Hex Appeal stops further writes and reports the error. Restart the game before retrying. Missing dependencies block affected controls; an unverified version alone remains a warning.

See [SDK: Outfit visibility](SDK-Outfit-Visibility.md) to add support to an outfit.
