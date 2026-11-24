# Verification: 1.1.2

**Released 23 November 2026.** 1.1.2 is the 1.1.1 performance build without its after-frame-generation drawing route. It no longer creates a test swapchain during startup, and its log and saved reports name the exact build. Body resources, outfit refits and the Wardrobe SDK API are unchanged from 1.1.1.

| File | SHA-256 |
| --- | --- |
| `HexAppeal.asi` | `03923439968a7e122a110390241ad45cf3f749a382269fed7f92ceef49789285` |
| `HexAppeal-1.1.2.zip` | `fa0741f909fb7e53a1e0ccf0b949073a8596fb63938198518d7fc3d65cc648f1` |

## Offline checks

All **29 offline runtime checks** passed, including Windows FileVersion and ProductVersion 1.1.2, and **58 helper tests** passed. The player ZIP holds 140 files with both body choices.

## Live checks

- An AMD RX 7900 XTX with FSR Frame Generation and the Steam overlay on started and ran with the 1.1.2 plugin. The log named build 03923439, and Hex Appeal created no test swapchain.
- The 1.1.2 ZIP was then imported through DMM and the game started without a crash, including alongside Camp Happens Enhanced.
- A player on an NVIDIA RTX 5070 with DLSS reported that 1.1.2 works.

The DLSS result was reported by the player who had previously seen the Hex Appeal UI shimmer. Local live checks used AMD hardware.

## Limits

These checks cover startup, rendering and the log. They do not repeat the full fitting matrix for every witch, movement and detail level. Earlier fitting results keep their own scope in the [1.0.2](VERIFICATION-1.0.2.md), [1.0.1](VERIFICATION-1.0.1.md) and [1.0.0](VERIFICATION-1.0.0.md) records. External outfit assets still need their own rendered checks.

Crashes during startup with the Steam overlay come from Steam's own hook installation when several mods start at the same moment. Hex Appeal 1.1.2 does not take part in that race. Turning off the Steam overlay for Crimson Desert avoids it.
