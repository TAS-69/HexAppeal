# Verification: 1.1.3

**Released 24 November 2026.** 1.1.3 adds character packs for Alida, Naira, Nevra Yailar, Rucchia and Selvia, and holds up to 250 add-on records alongside the six witches. A pack can list several characters under one F9 sidebar entry and choose whether it starts enabled. Shape and Wardrobe recognise ally records, such as Naira's 1004236 and 412. On an unfamiliar Crimson Desert build, Hex Appeal checks for its native functions, and **Save report** collects graphics details. Body resources and outfit refits are unchanged from 1.1.2.

| File | SHA-256 |
| --- | --- |
| `HexAppeal.asi` | `3ed014c04dc6facf9e856544f12dbb01c898c1826de54aa87032f14531e30d29` |
| `HexAppeal-1.1.3.zip` | `3fd81e22946fa66e7ca156e1628619b9869f96c5bfa2888bbcf7e725bde7ff2f` |

## Character packs

Each pack is a separate DMM download. Woosa & Maegu 1.0.1 gives the shared F9 sidebar entry its **Woosa & Maegu** label. Hex Appeal 1.1.3 also groups the older pack under its original label.

| Pack | Version | SHA-256 |
| --- | --- | --- |
| Alida | 1.0.0 | `b760df54f247f065f16806f7c6d098a50a438f18870b1d2b9a6ec2c4717e2ac7` |
| Naira | 1.0.0 | `8c33c7b227dd80d85ec102b399c47dab0c0f07c0b626d4b8e5619b472a0bd1e8` |
| Nevra Yailar | 1.0.0 | `d3dce6572fbaeef6ded6e74736b15a4d9fc147c7f8345292874a9a940efc4257` |
| Rucchia | 1.0.0 | `74eb21c70c13e039b077e8890b2040264c327bc41a209767792f2126fcd6e5b1` |
| Selvia | 1.0.0 | `5abcdd1a6398b85b50177e9405c75c3edc4eb5a2b1230c37bd772f5c790613be` |
| Woosa & Maegu | 1.0.1 | `eb3993307e67fcded5d5b60db49d101b93d6cce6e797b4d779d46c677a3bdc65` |

## Offline checks

All **33 offline runtime checks** passed, including Windows FileVersion and ProductVersion 1.1.3, and **58 helper tests** passed. The player ZIP holds 140 files with both body choices. Its game files are identical to 1.1.2.

## Live checks

The release archives were imported through DMM and played on an AMD RX 7900 XTX:

- **[01] VENUS**, with Alida, Naira, Nevra Yailar, Rucchia and Selvia 1.0.0 and the published Woosa & Maegu 1.0.0:
  - All ten add-on records loaded with no errors.
  - Each new character previewed a different top in **[HA] Wardrobe** and returned on Cancel.
  - The published Woosa & Maegu 1.0.0 appeared as one sidebar entry with a character dropdown.
- **Update to Woosa & Maegu 1.0.1 on [02] Vanilla:**
  - A pack unticked under **Character patches** stayed unticked after the update.
  - Ticked again, it showed one **Woosa & Maegu** entry with a Maegu/Woosa dropdown, and both previewed and returned on Cancel.
- **250 add-on records**, 240 of them temporary test definitions, on [02] Vanilla:
  - The 251st was rejected with the roster-limit message.
  - With F9 closed the game ran at 123.5 FPS presented. With F9 open it ran at 122.6 FPS for a full 60 seconds.
  - With **ALL CHARACTERS → Wardrobe** open it ran at 121.9 FPS.

The ZIP’s embedded verification summary predates these live passes. This page has the completed results for the installer SHA-256 above.

These ran on earlier 1.1.3 test builds:

- A player on an NVIDIA RTX 5070 with DLSS Frame Generation confirmed that the 1.1.3 Character Instance Test works. This release's rendering, Frame Generation and hook code is identical to that build.
- On an AMD RX 7900 XTX with **[01] VENUS**, Shape and Wardrobe reached Naira's records 1004236, 412 and 444, and both Rucchia records. Combined previews, Cancel, **Apply and save** and **Follow game** applied to every loaded copy.
- A 256-slot test build accepted 250 add-on records and rejected the next one. With F9 closed, it measured 124.07 FPS at 10 records and 123.87 FPS at 250.

## Limits

These checks do not repeat the full fitting matrix for every witch, movement and detail level. Earlier fitting results keep their own scope in the [1.0.2](VERIFICATION-1.0.2.md), [1.0.1](VERIFICATION-1.0.1.md) and [1.0.0](VERIFICATION-1.0.0.md) records. External outfit assets still need their own rendered checks.

Crashes during startup with the Steam overlay come from Steam's own hook installation when several mods start at the same moment. Turning off the Steam overlay for Crimson Desert avoids it.
