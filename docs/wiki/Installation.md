# Installation

## Requirements

Use [Definitive Mod Manager (DMM)](https://www.nexusmods.com/crimsondesert/mods/633) by [AerowynX](https://www.nexusmods.com/profile/AerowynX) to import and mount the ZIP. DMM includes an ASI loader. Enable it from ASI Mods if prompted. Basic Wardrobe runs from the game once the ASI and game assets are loaded. Dynamic VENUS skin matching and automatic VENUS body routing for character add-ons use DMM's active mounted files.

Download these separately:

| Dependency | Needed for | Checked reference |
| --- | --- | --- |
| [Venus Body for Damiane](https://www.nexusmods.com/crimsondesert/mods/3318) | Only [01] VENUS; choose one Human Body or Elf Body variant | 2.6 |
| [Instant Surgery](https://www.nexusmods.com/crimsondesert/mods/3660) | Native shaping | 0.9.6 |
| [Hex's Hentai Preset](https://www.nexusmods.com/crimsondesert/mods/4234) | HEX shapes; retain its original preset file | 1.3 preferred; 1.2 also checked |

[Two Reasons Body Physics](https://www.nexusmods.com/crimsondesert/mods/3636) is optional. Hex Appeal enables the coordinated pose handoff once the plugin and its interface are available. The game and manager used for development are Crimson Desert 2.05.01 / Steam build 26776744 and DMM 5.0.1.

**Weight needs no outfit mod.** Dancer outfit integration, colours, source detection and setup are excluded from this release pending permission. Any future integration will be a separate patch. Wardrobe's generic SDK remains available for other outfits.

## Download and install

| File | Use |
| --- | --- |
| **HexAppeal-1.1.2.zip** | Player package: runtime, body routing and default outfit refits |
| **HexAppeal-Wardrobe-SDK-1.1.2.zip** | Optional asset-free SDK for outfit creators |
| **HexAppeal-Source-1.1.2.zip** | Optional source; no compiled plugin or garment PAC outputs |

1. Close Crimson Desert. Install DMM and Instant Surgery. Add Hex's Hentai Preset for HEX. Venus Body for Damiane is required only for the VENUS body option.
2. Import **HexAppeal-1.1.2.zip** into DMM. If upgrading, follow the cleanup below first. Keep only the current HexAppeal.asi in DMM's library.
3. Expand Hex Appeal and choose **[01] VENUS** or **[02] Vanilla**. In DMM 5.0.1, choosing one automatically clears the other within the same package. Enable Hex Appeal under **ASI Mods**, then mount once.
4. Launch, load near a witch and press **F9**. Open **Settings → Dependencies & paths** if detection needs a manual location.

For VENUS, let the selected Venus Body for Damiane variant (Human or Elf Body) win its body route. Witch boots use private paths, so Damiane Armor Refit Pack can retain its own boots. Vanilla also isolates the original witch bodies and underwear. You can keep Venus Body for Damiane installed for Damiane while choosing Vanilla for witches. See [Body options](Body-options.md).

Unverified readable versions warn and remain usable when their actual checks pass. Missing resources, wrong paths and incompatible native interfaces block only the affected features. See [Compatibility](Compatibility.md).

## Update or remove

**Upgrading to 1.1.2:** close the game and back up `%LOCALAPPDATA%/HexAppeal/`. Remove the previous Hex Appeal package through DMM, then import 1.1.2. Keep your saved settings. Remove any separate top-level VENUS/Vanilla entries left by the broken import, and disable old separate Bodies/Outfit Refits and external outfit resource entries. Leave separately installed third-party mods alone.

Select exactly one new body choice, enable its ASI and mount. Check that the mod group, selected body option and sole HexAppeal ASI show 1.1.2. DMM can add a new folder beside an older installation; do not rely on importing to remove the old version for you.

The earlier packaging could split the body folders and shared files during import. The corrected ZIP uses one folder with its manifest beside it, which DMM imports as a single group. Removing the old installation avoids duplicate plugins and stale selections. Your saved profiles are stored separately and do not need resetting.

Older outfit selections are retained as unavailable, with **Default** still selectable. They are not silently replaced in the saved preferences. To remove Hex Appeal, disable both its Mods and ASI switches and remount.
