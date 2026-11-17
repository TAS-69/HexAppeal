# Installation

## Requirements

Use [Definitive Mod Manager (DMM)](https://www.nexusmods.com/crimsondesert/mods/633) by [AerowynX](https://www.nexusmods.com/profile/AerowynX). DMM includes ASI loader installation; enable its bundled loader from ASI Mods if prompted. No separate loader download is needed. Hex Appeal currently requires DMM's loaded runtime and active file manifest; another manager's source folder alone is insufficient.

Download these separately:

| Dependency | Needed for | Checked reference |
| --- | --- | --- |
| [VENUS Body for Damiane](https://www.nexusmods.com/crimsondesert/mods/3318) | Actual body model; choose one Human Body or Elf Body variant | 2.6 |
| [Instant Surgery](https://www.nexusmods.com/crimsondesert/mods/3660) | Native shaping | 0.9.6 |
| [Hex's Hentai Preset](https://www.nexusmods.com/crimsondesert/mods/4234) | HEX shapes; retain its original preset file | 1.3 preferred; 1.2 also checked |

[Two Reasons Body Physics](https://www.nexusmods.com/crimsondesert/mods/3636) is optional. If loaded, its compatible pose acknowledgement interface is needed; 1.0.7 was checked. Legion's slider is not required. The checked game/manager references are Crimson Desert 2.05.00 / Steam build 26636972 and DMM 5.0.1.

**Weight needs no outfit mod.** Dancer outfit integration, colours, source detection and setup are excluded from this release pending permission. Any future integration will be a separate patch. Wardrobe's generic SDK remains available for other outfits.

## Download and install

| File | Use |
| --- | --- |
| **HexAppeal-1.0.0.zip** | Player package: runtime, body routing and default outfit refits |
| **HexAppeal-Wardrobe-SDK-1.0.0.zip** | Optional asset-free SDK for outfit creators |
| **HexAppeal-Source-1.0.0.zip** | Optional source; no compiled plugin or garment PAC outputs |

1. Close Crimson Desert. Install the requirements and enable exactly one VENUS Human/Elf variant.
2. Import **HexAppeal-1.0.0.zip** into DMM. Disable old separate Hex Appeal modules and keep only one HexAppeal.asi.
3. Enable Hex Appeal on Mods and its normal ASI Mods switch. Review file priorities, then mount once. No outfit setup script or Python installation is needed to play.
4. Launch, load near a witch and press **F9**. Open **Settings → Dependencies & paths** if detection needs a manual location.

Let the selected VENUS variant win over competing complete body replacements. Recheck priorities after switching Human/Elf. Default boot routes 0160, 0161 and 0163 overlap VENUS Armor Refit; choose the intended refit in DMM.

Unverified readable versions warn and remain usable when their actual checks pass. Missing resources, wrong paths and incompatible native interfaces block only the affected features. See [Compatibility](Compatibility.md).

## Update or remove

Close the game and back up `%LOCALAPPDATA%/HexAppeal/`. Replace the old package through DMM and remount without resetting settings. Disable legacy Hex Appeal external outfit resource entries when moving to this core-only package. Separately installed third-party mods remain yours to manage.

Older outfit selections are retained as unavailable, with **Default** still selectable. They are not silently replaced in the saved preferences. To remove Hex Appeal, disable both its Mods and ASI switches and remount.
