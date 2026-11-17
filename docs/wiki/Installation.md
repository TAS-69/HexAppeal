# Installation

## Requirements

Use **Desert Mod Manager (DMM)** and an x64 ASI loader. The checked reference setup used Crimson Desert **2.05.00 / Steam build 26636972** and DMM **5.0.1**.

Download these separately. Hex Appeal does not include their files.

| Dependency | Needed for | Checked reference |
| --- | --- | --- |
| [VENUS Body for Damiane](https://www.nexusmods.com/crimsondesert/mods/3318), **Human Body or Elf Body** | Actual body model; choose one variant | 2.6 |
| [Hex's Hentai Preset](https://www.nexusmods.com/crimsondesert/mods/4234) | HEX shapes | 1.3 preferred; 1.2 also checked |
| [Instant Surgery](https://www.nexusmods.com/crimsondesert/mods/3660) | Native shaping | 0.9.6 |
| [Sexy Witches of Pywel Body and Dress Reshaped](https://www.nexusmods.com/crimsondesert/mods/2891) | Optional Dancer Dress and five supplied colours | Complete five-colour FOMOD 1.1 |

Development **0.5.9** separates Weight from the optional outfit. Weight needs the normal native shaping requirements, with no Sexy Witches installation or path. The published **1.0.0** package predates this change and still gates Weight on that source; [Weight controls](../WEIGHT-CONTROLS.md) explains the difference.

Keep Hex's original preset where the runtime can read it. **Two Reasons Body Physics** is optional; if loaded, its compatible pose acknowledgement interface is needed. Version 1.0.7 was checked. Legion's slider is not required.

Unverified readable versions show a warning and remain selectable when their interface checks pass. Missing files, incorrect paths and actual incompatible interfaces block only the affected features. See [Compatibility](Compatibility.md).

## Download

| Release file | Who needs it? |
| --- | --- |
| **HexAppeal-1.0.0.zip** | Players: runtime, body routing, default outfit refits and optional Dancer setup |
| **HexAppeal-Wardrobe-SDK-1.0.0.zip** | Outfit creators: asset-free headers, generator and examples |
| **HexAppeal-Source-1.0.0.zip** | Building from source; no compiled plugin or garment PAC outputs |

Only the first ZIP is needed to play. DMM shows one **Hex Appeal** entry on Mods and its normal **HexAppeal** switch on ASI Mods. Enable both.

## Install

1. Close Crimson Desert and install the external requirements. Enable exactly one VENUS Human/Elf variant.
2. Import **HexAppeal-1.0.0.zip** into DMM. Disable any old separate Hex Appeal Bodies, Runtime and Outfit Refits entries. Keep only one HexAppeal.asi.
3. If using Dancer, complete the optional setup below **before mounting**.
4. Enable Hex Appeal on Mods and ASI Mods. Review conflicts, then mount once. The plugin and configuration should land at `bin64/HexAppeal.asi` and `bin64/HexAppeal.ini`.
5. Launch, load near a witch and press **F9**. Check **Settings → Dependencies & paths** if a feature is unavailable.

Let the selected VENUS variant win over other complete body replacements, including **NaiK Elf Ears V3 — Original Damiane Body**. Recheck file priorities after switching Human/Elf. Boot routes `0160`, `0161` and `0163` overlap VENUS Armor Refit; choose the intended refit in DMM.

## Optional Dancer setup

Keep the complete original five-colour FOMOD extracted beside Hex Appeal in DMM's mods storage, on the same drive. Do not mount its complete body replacement over VENUS.

1. Install Python **3.12+** with its Windows launcher for this setup step.
2. Before mounting, open the installed Hex Appeal folder and run **Setup Dancer.cmd**. It detects the original FOMOD and prepares four local file references.
3. If upgrading from the old separate **External Dancer Resources** setup, disable that old entry. Enable the main Hex Appeal entry and its ASI switch, review conflicts and mount once.

If detection is ambiguous, open a terminal in the installed Hex Appeal folder. Inspect the plan first, then repeat with `--apply`:

```powershell
py -3 tools/prepare_dancer_dependencies.py --source "D:/DMM/mods/OriginalFOMOD" --main-mod .
py -3 tools/prepare_dancer_dependencies.py --source "D:/DMM/mods/OriginalFOMOD" --main-mod . --apply
```

Use your actual extracted source path. The helper requires the supported source layout and all five supplied colours; a missing or incompatible layout reports an error. It does not download resources, edit original contents or generate replacement meshes. There is no second mount in a fresh installation when setup is completed before the first mount.

Hard links share the original file contents. Keep the separately downloaded FOMOD, do not edit the linked contents, and **do not redistribute the configured installed folder**. Colour changes replace only the generated texture entry. The clean public ZIP contains setup code only. See [player packaging](../PLAYER-PACKAGE.md) for the checked layout and limits.

## Update or remove

Close the game first. Back up `%LOCALAPPDATA%/HexAppeal/`, replace the old package through DMM and remount. Retain settings; no reset is needed. An update may require preparing the optional Dancer references again before that update's mount.

To remove Hex Appeal, disable both its Mods and ASI switches and remount. Disable any legacy Hex Appeal resource entries too. Saved settings can remain for a later installation.
