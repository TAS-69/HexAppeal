# Releases and versioning

**The current release is 1.1.6.** Every public version is listed below, newest first. The [changelog](../../CHANGELOG.md) has the full notes for each one.

## Release history

| Version | Released | Highlights |
| --- | --- | --- |
| **1.1.6** | 28 Nov 2026 | Works with the Epic Games Store version of Crimson Desert 2.06.00: 2.06 is recognised by the game's code instead of the Steam executable. Shape no longer shows Ready on a build whose game addresses were not found. Source updated. |
| 1.1.5 | 27 Nov 2026 | Crimson Desert 2.06.00 support for Shape, Wardrobe and the native **[HA] Wardrobe**. Sylvia keeps Shape when another mod renames her body. Body warnings show only on the witch they apply to. Source updated. |
| 1.1.4 | 25 Nov 2026 | F9 and **[HA] Wardrobe** appear on NVIDIA setups where NvPresent64.dll handles the game's frames. If the menu can't draw, the log says why. Source updated. |
| 1.1.3 | 24 Nov 2026 | Character packs for Alida, Naira, Nevra Yailar, Rucchia and Selvia, and up to 250 add-on records. A pack can list several characters under one F9 sidebar entry. Shape and Wardrobe recognise ally records. Checks for game functions after a Crimson Desert update. Graphics details are collected by **Save report**. Source and SDK updated. |
| 1.1.2 | 23 Nov 2026 | Removes 1.1.1's Frame Generation drawing route after reports of the game not starting, and keeps its performance work. No test swapchain at startup. Logs and reports name the exact build. Source and SDK updated on 24 Nov. |
| 1.1.1 | 23 Nov 2026 | Less rendering and input work while F9 and Wardrobe are closed. Graphics adapter, driver and Frame Generation details in logs. Replaced by 1.1.2. |
| 1.1.0 | 23 Nov 2026 | F9 and **[HA] Wardrobe** render on Streamline and XeSS Frame Generation swapchains. Wardrobe recovers outfits while characters load and from damaged profile files. |
| 1.0.9 | 22 Nov 2026 | Hover a blocked control to see why. Wardrobe checks shared game hooks by what they do. The panel has its own DirectX presentation path. Fixes a startup stack overflow with the Steam overlay. |
| 1.0.8 | 22 Nov 2026 | Larger default text and panel at 4K. Fixes a crash when increasing **Settings → Scale**. |
| 1.0.7 | 22 Nov 2026 | Fixes witch detection on some setups, the missing **[HA] Wardrobe** prompt and **Hide outfit** getting stuck. Works alongside Live Transmog and FAPS. Hides the HUD during the Wardrobe preview. |
| 1.0.6 | 21 Nov 2026 | **Outfit update range** setting, wider Wardrobe item discovery, Character Creator and Pywel Wardrobe startup compatibility, and Woosa & Maegu add-on support. |
| 1.0.5 | 21 Nov 2026 | Optional character packs. **[HA] Wardrobe** holds the character facing you during preview. SDK controls for equipped items. |
| 1.0.4 | 20 Nov 2026 | Saved **[HA] Wardrobe** outfits with item search, Spouse Overhaul Barber and Combat Gear support, optional skin tone matching, Crimson Desert 2.05.01. Source and SDK updated. |
| 1.0.3 | 18 Nov 2026 | Log tab and saved reports, manually selected Instant Surgery presets, reworked outfit hide and restore, local skin setup tools. Source and SDK updated. |
| 1.0.2 | 18 Nov 2026 | Fixes DMM import grouping for the two body choices. Adds version details to the plugin. |
| 1.0.1 | 18 Nov 2026 | VENUS and Vanilla body choices in one download. Fixes a world-load crash and invisible witches. Wider preset discovery. |
| 1.0.0 | 16 Nov 2026 | First release: the VENUS body option for the witches, HEX / baseline / FLAT blending, Height and Weight, Hide outfit, Wardrobe and its SDK. |

Each release is one player download, `HexAppeal-<version>.zip`, containing the runtime, both body choices and the default outfit refits. The optional **Source** and **Wardrobe SDK** downloads were published with 1.0.0 to 1.0.4 and again with 1.1.2 and 1.1.3. 1.1.4 to 1.1.6 update the Source and keep the 1.1.3 SDK. The 1.1.3 to 1.1.6 releases also have a bundle with the player ZIP, Source, Wardrobe SDK and six [character packs](Character-add-ons.md).

Superseded downloads stay under Old files on Nexus. Checksums and the verification records ([1.1.6](../VERIFICATION-1.1.6.md), [1.1.5](../VERIFICATION-1.1.5.md), [1.1.4](../VERIFICATION-1.1.4.md), [1.1.3](../VERIFICATION-1.1.3.md), [1.1.2](../VERIFICATION-1.1.2.md), [1.0.2](../VERIFICATION-1.0.2.md), [1.0.1](../VERIFICATION-1.0.1.md), [1.0.0](../VERIFICATION-1.0.0.md)) identify the exact files.

## How versions are numbered

Each public release takes the next number after the last public one, and a number is never reused. Development builds use their own `DEV_VERSION`, so ordinary edits, commits and test builds never change the public number. Packaging never increments it automatically.

API table versions describe SDK compatibility independently of either number. Tags use the `v1.1.6` style.
