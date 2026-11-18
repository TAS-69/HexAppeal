# Installation

## Requirements

Use [Definitive Mod Manager (DMM)](https://www.nexusmods.com/crimsondesert/mods/633) by [AerowynX](https://www.nexusmods.com/profile/AerowynX). DMM includes ASI loader installation; enable its bundled loader from ASI Mods if prompted. No separate loader download is needed. Hex Appeal currently requires DMM's loaded runtime and active file manifest; another manager's source folder alone is insufficient.

Download these separately:

| Dependency | Needed for | Checked reference |
| --- | --- | --- |
| [VENUS Body for Damiane](https://www.nexusmods.com/crimsondesert/mods/3318) | Only [01] VENUS; choose one Human Body or Elf Body variant | 2.6 |
| [Instant Surgery](https://www.nexusmods.com/crimsondesert/mods/3660) | Native shaping | 0.9.6 |
| [Hex's Hentai Preset](https://www.nexusmods.com/crimsondesert/mods/4234) | HEX shapes; retain its original preset file | 1.3 preferred; 1.2 also checked |

[Two Reasons Body Physics](https://www.nexusmods.com/crimsondesert/mods/3636) is optional. If loaded, its compatible pose acknowledgement interface is needed; 1.0.7 was checked. Legion's slider is not required. The checked game/manager references are Crimson Desert 2.05.00 / Steam build 26636972 and DMM 5.0.1.

**Weight needs no outfit mod.** Dancer outfit integration, colours, source detection and setup are excluded from this release pending permission. Any future integration will be a separate patch. Wardrobe's generic SDK remains available for other outfits.

## Download and install

| File | Use |
| --- | --- |
| **HexAppeal-1.0.2.zip** | Player package: runtime, body routing and default outfit refits |
| **HexAppeal-Wardrobe-SDK-1.0.2.zip** | Optional asset-free SDK for outfit creators |
| **HexAppeal-Source-1.0.2.zip** | Optional source; no compiled plugin or garment PAC outputs |

1. Close Crimson Desert. Install DMM and Instant Surgery. Add the external Hex preset for HEX. VENUS is required only for the VENUS body option.
2. Import **HexAppeal-1.0.2.zip** into DMM. If upgrading from 1.0.0, follow the cleanup below first. Keep only the current HexAppeal.asi in DMM's library.
3. Expand Hex Appeal and enable **[01] VENUS** or **[02] Vanilla**. Disable the other body choice. Enable the normal ASI Mods switch, then mount once. No setup script, second setup mount or Python installation is needed to play.
4. Launch, load near a witch and press **F9**. Open **Settings → Dependencies & paths** if detection needs a manual location.

For VENUS, let the selected Human/Elf variant win its body route. Witch boots use private paths, so Damiane Armor Refit Pack can retain its own boots. Vanilla also isolates the original witch bodies and underwear. You can keep VENUS installed for Damiane while choosing Vanilla for witches. See [Body options](Body-options.md).

Unverified readable versions warn and remain usable when their actual checks pass. Missing resources, wrong paths and incompatible native interfaces block only the affected features. See [Compatibility](Compatibility.md).

## Update or remove

**Upgrading to 1.0.2:** close the game and back up `%LOCALAPPDATA%/HexAppeal/`. Remove the previous Hex Appeal package through DMM, then import 1.0.2. Keep your saved settings. Remove any separate top-level VENUS/Vanilla entries left by the broken import, and disable old separate Bodies/Outfit Refits and external outfit resource entries. Leave separately installed third-party mods alone.

Select exactly one new body choice, enable its ASI and mount. Check that the mod group, selected body option and sole HexAppeal ASI show 1.0.2. DMM can add a new folder beside an older installation; do not rely on importing to remove the old version for you.

The earlier packaging could split the body folders and shared files during import. The corrected ZIP uses one folder with its manifest beside it, which DMM imports as a single group. Removing the old installation avoids duplicate plugins and stale selections. Your saved profiles are stored separately and do not need resetting.

Older outfit selections are retained as unavailable, with **Default** still selectable. They are not silently replaced in the saved preferences. To remove Hex Appeal, disable both its Mods and ASI switches and remount.
