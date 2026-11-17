# Installation

## Before installing

Use a working **Desert Mod Manager (DMM)** setup and an x64 ASI loader. The checked reference setup used Crimson Desert **2.05.00 / Steam build 26636972** and DMM **5.0.1**.

Download these separately:

| Dependency | Needed for | Checked reference |
| --- | --- | --- |
| [VENUS Body for Damiane](https://www.nexusmods.com/crimsondesert/mods/3318), **Elf Body** | Actual body model | 2.6 |
| [Hex's Hentai Preset](https://www.nexusmods.com/crimsondesert/mods/4234) | HEX shapes | 1.3 preferred; 1.2 also checked |
| [Instant Surgery](https://www.nexusmods.com/crimsondesert/mods/3660) | Native shaping | 0.9.6 |
| [Sexy Witches of Pywel Body and Dress Reshaped](https://www.nexusmods.com/crimsondesert/mods/2891) | Optional Weight reference and Dancer Dress | FOMOD 1.1 |

Retain Hex's original preset file where the runtime can read it. If **Two Reasons Body Physics** is already installed, its compatible pose acknowledgement interface is needed; 1.0.7 was checked. It is not otherwise required. Legion's slider is not required.

Unknown readable versions receive a warning and remain selectable. Missing or incorrect/unreadable paths disable the affected feature. An actual incompatible native interface can still reject an operation.

Development 0.5.7 recognizes both VENUS 2.6 Human Body and Elf Body; choose one, then review body file priorities again. Release 1.0.0 and its original instructions below remain unchanged. See [the variant comparison](../VENUS-COMPATIBILITY.md).

## Choose the packages

The next player package combines Bodies, Runtime and default Outfit Refits into **one ZIP**, checked in DMM 5.0.1 as one **Hex Appeal** entry on Mods plus its normal **HexAppeal** switch on ASI Mods. Enable both and mount with the game closed; disable both and remount to remove it. Development modules remain separate. This does not replace the published 1.0.0 downloads below. Source and SDK downloads are optional resources for creators. See [player packaging](../PLAYER-PACKAGE.md).

| Release download | Install? | Contains |
| --- | --- | --- |
| **Hex Appeal - Bodies-1.0.0.zip** | Required | Seven appearance patches referencing the external VENUS body |
| **Hex Appeal - Runtime-1.0.0.zip** | Required for the editor | My ASI, default settings, guides and notices |
| **Hex Appeal - Outfit Refits-1.0.0.zip** | Optional | Forty game-derived default garment refits to the VENUS baseline |
| **Hex Appeal - Wardrobe SDK-1.0.0.zip** | Outfit creators only | Asset-free headers, generator and examples |
| **Hex Appeal - Source-1.0.0.zip** | Building only | Source, SDK, build metadata and documentation; no compiled plugin |

## Install through DMM

1. Close Crimson Desert completely.
2. Install and enable the external requirements. Select VENUS **Elf Body**.
3. Import Bodies and Runtime into DMM. Import Outfit Refits if wanted. Enable their entries.
4. Review conflicts, then mount. The runtime must land at `bin64/HexAppeal.asi` with `bin64/HexAppeal.ini` beside it. Keep only one copy of the plugin.
5. Launch, load a save near a witch and press **F9**. Open **Settings → Dependencies & paths** to correct detection if needed.

Give VENUS Elf Body priority over **NaiK Elf Ears V3 — Original Damiane Body** on the body PAC. Default boot routes `0160`, `0161` and `0163` overlap VENUS Armor Refit: choose which refit wins in DMM. See [Compatibility](Compatibility.md).

## Dancer in the next combined package

The combined package includes **Setup Dancer.cmd** and its helper. Keep the complete original five-colour FOMOD beside Hex Appeal in DMM's mods storage, on the same drive. With Python 3.12+ and its launcher installed, run Setup Dancer from the installed Hex Appeal folder **before mounting**. Then enable the main mod and its ASI switch and mount once. No separate External Dancer Resources entry or source download is needed. See [player packaging](../PLAYER-PACKAGE.md) for manual paths and upgrade details.

The public ZIP contains the setup code only. Generated local references use the separately obtained originals and must never be redistributed as part of Hex Appeal.

## Optional Dancer setup in published 1.0.0

Keep the separately downloaded Sexy Witches FOMOD fully extracted, including all five colour folders. Avoid mounting its complete body replacement over VENUS. The helper creates a **separate local DMM resource entry** with four hard links to the original upper/lower models, material properties and selected supplied texture. It does not generate or refit Dancer meshes.

1. Close the game and install Python **3.12 or newer** for this setup step.
2. Extract Hex Appeal's source download somewhere separate from both the FOMOD and your DMM storage.
3. Run the helper with a new destination in DMM's mods storage, on the same drive as the extracted FOMOD. First omit `--apply` to inspect the plan.

```powershell
python tools/prepare_dancer_dependencies.py --source "D:/Mods/SexyWitchesFOMOD" --destination "D:/DMM/mods/Hex Appeal - External Dancer Resources (local)"
```

4. Repeat with `--apply` to create that entry. In DMM, refresh discovery, enable it, review the shared `0191` routes and mount. If your DMM uses another storage folder, use its actual mods directory.
5. Start the game and check **Settings → Dependencies & paths**, then **Wardrobe**. Retain all five original colour folders for colour changes.

Hard links share file contents. Do not edit their contents, and do not distribute the generated folder or archive it as part of Hex Appeal. The helper refuses an existing destination. Colour changes replace only the generated texture entry, leaving the original dependency file unchanged. Remove the generated local entry through DMM when no longer wanted. A surviving hard link can retain the data even after the original filename is removed.

## Update or remove

Close the game before replacing or disabling any plugin or resource package. Back up `%LOCALAPPDATA%/HexAppeal/` if you want to preserve your choices, then use DMM to replace the old package and remount. Do not reset your settings just to update. To remove, disable the Hex Appeal entries and optional local Dancer entry in DMM and remount. Settings may be retained for a later install.
