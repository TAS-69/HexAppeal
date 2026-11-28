# Verification: 1.1.6

**Released 28 November 2026.** 1.1.6 recognises Crimson Desert 2.06.00 by the game's code instead of the Steam executable, so Shape, Wardrobe and the native **[HA] Wardrobe** also work with the Epic Games Store executable. Everything else is unchanged from 1.1.5: the rendering, frame generation and NVIDIA presentation code, body resources, outfit refits, the character packs and the Wardrobe SDK.

| File | SHA-256 |
| --- | --- |
| `HexAppeal.asi` | `d34346a0ae5bc884a7501a3faf7ada3ef24f2e20f9faf52028f832923e8c53d5` |
| `HexAppeal-1.1.6.zip` | `a1b8e12e4f1790ab26bdf66742deeca461347f3136e78fd19849dc58502b1488` |

The character packs and `HexAppeal-Wardrobe-SDK-1.1.3.zip` are the 1.1.3 files. Their checksums are in the [1.1.3 record](VERIFICATION-1.1.3.md).

## Checked references

- **Game:** Crimson Desert 2.06.00, Steam build 26949954. The executable's stamp is `6B07E5E9` and its SHA-256 is `ea2d84d947521332b002359818960ffd97765f37b447503d8a1efad4040269c8`.
- **Epic Games Store:** Crimson Desert 2.06.00, executable stamp `6B07E5C4`. A player confirmed that Shape and Wardrobe work on the test build `19678c1349caaf9a2a696f711fc752f57f2326e13e113858c2a33ba270d83d57`. 1.1.6 is built from the same source and differs only in its version number. The Epic executable was not available here, so it shows the unverified game warning.
- **Mod manager and dependencies:** unchanged from [1.1.5](VERIFICATION-1.1.5.md).

## What changed

- **Recognising 2.06.** A build uses the 2.06 record layout when all five appearance record functions match their 2.06 signatures. 1.1.5 required the Steam executable's stamp, size and checksum. On the Steam executable, every 2.06 signature must still land on its checked address.
- **The remaining 2.06 addresses.** 1.1.5 used 19 fixed Steam addresses for the animation upload, the native Interact menu hooks and handlers, its close handler and close event, the radial cursor class and the alternative focus callers. They are now found by code signature, class type information or vtable slot. They are generated from the Steam executable by `runtime/tools/signatures/gen206.py`. If one is not found, only the feature that needs it waits, and the log names it.
- **Shape status.** If no game addresses are found for a build, Shape shows Checking until a compatible animation upload is seen. 1.1.5 showed Ready on such a build.

## Offline checks

All **33 offline runtime checks** passed on the exact binary, including Windows FileVersion and ProductVersion 1.1.6, and **58 helper tests** passed. The game signature test maps the Steam 2.06 executable and checks:

- All 43 required addresses and all 19 2.06 addresses equal the addresses checked in game.
- A copy of the executable with the Epic build stamp and a different checksum is recognised as 2.06 by its code, resolves the same addresses, and is not treated as the checked build.

The player ZIP holds 140 files with both body choices. Its game files are identical to 1.1.5.

## Live checks

The Steam checks used 2.06.00 with the full PD loadout (AMD RX 7900 XTX, HDR, FSR Frame Generation):

- **Exact 1.1.6 binary:** not run in game, because the game was in use. Its code is byte-identical to the test build below. Only 12 bytes differ: the version string, the version resource and the file checksums.
- **Test build `19678c13`**, the same code: 43 of 43 addresses found in 0.6 s, and Hex Appeal attached to the 2.06 animation upload. Shape was Ready once Lyselia was visible. Native focus prompts and Interact routing were ready, with cursor styling. On Lyselia, **[HA] Wardrobe** opened through the native Interact menu: the camera held, depth of field engaged and the HUD hid. Cancel closed the native Interact group, and the Ctrl focus menu worked normally afterwards.
- **Epic Games Store:** a player confirmed the test build fixed Shape and Wardrobe.

## Limits

- The Epic executable was checked only by that player. Its digest is not in Hex Appeal's checked list, so Settings shows Crimson Desert as Unverified with the usual warning. The controls stay usable.
- Naira, Two Reasons Body Physics and the [02] Vanilla option keep the limits listed in the [1.1.5 record](VERIFICATION-1.1.5.md).
