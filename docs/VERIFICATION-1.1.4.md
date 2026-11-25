# Verification: 1.1.4

**Released 25 November 2026.** 1.1.4 fixes F9 and the native **[HA] Wardrobe** not appearing on NVIDIA setups where NVIDIA's presentation layer (NvPresent64.dll) handles the game's frames. When the game's swapchain and its present queue report different devices, Hex Appeal now draws with the queue's device. If it can't draw, the log says why. Body resources, outfit refits, the character packs and the Wardrobe SDK are unchanged from 1.1.3.

| File | SHA-256 |
| --- | --- |
| `HexAppeal.asi` | `3789528a8196420bfc46c37dfcd9c0d54bb6f929d0394892d9dd1687585a371a` |
| `HexAppeal-1.1.4.zip` | `9ff0dac58950a430e9a2da08e6362b5e40aa560c17996a48db7f4fa3a53653f5` |

The character packs and `HexAppeal-Wardrobe-SDK-1.1.3.zip` are the 1.1.3 files. Their checksums are in the [1.1.3 record](VERIFICATION-1.1.3.md).

## What changed in the runtime

Three files changed, all in the DirectX 12 overlay: `runtime/src/hooks/dx12/resources.h`, `draw.h` and `install.h`.

- With DLSS on, Streamline gives Hex Appeal the game's native present queue. On the affected setup, NvPresent64.dll owned the swapchain's Present, and the swapchain reported a different device from that queue. 1.1.3 took its drawing device from the swapchain and drew only when the queue's device matched it, so it skipped every frame there without logging why.
- 1.1.4 uses the queue's device when the two differ. It then checks that each back buffer belongs to that device before drawing on it.
- When the two devices match, 1.1.4 uses the swapchain's device exactly as 1.1.3 did. Every setup where 1.1.3 drew behaves the same.
- Each reason for not drawing is logged once, starting with `[overlay] Not drawing:`. The log also records the game swapchain's device, its present queue and which device the overlay uses.

## Offline checks

All **33 offline runtime checks** passed, including Windows FileVersion and ProductVersion 1.1.4, and **58 helper tests** passed. The player ZIP holds 140 files with both body choices. Its game files are identical to 1.1.3. Apart from the runtime, version fields and documentation, the ZIP matches 1.1.3.

## Live checks

- A player whose NVIDIA game presents through NvPresent64.dll, with Streamline loaded, confirmed that the fix works. They used the NVIDIA Present Test build (ASI `8858054b`). Its runtime source is identical to this release; only the version number differs.
- The [1.1.3 live checks](VERIFICATION-1.1.3.md#live-checks) cover the unchanged character packs, body options and the 250-record roster.

## Limits

This exact 1.1.4 binary has not been played on the AMD test PC. Its drawing path there is the same as 1.1.3's, because the swapchain and present queue report the same device. Earlier fitting results keep their own scope in the [1.0.2](VERIFICATION-1.0.2.md), [1.0.1](VERIFICATION-1.0.1.md) and [1.0.0](VERIFICATION-1.0.0.md) records.

Crashes during startup with the Steam overlay come from Steam's own hook installation when several mods start at the same moment. Turning off the Steam overlay for Crimson Desert avoids it.
