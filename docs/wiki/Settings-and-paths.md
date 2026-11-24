# Settings and paths

## Panel controls

**F9** opens the panel. Change the shortcut and panel scale in Settings. The panel scales with your resolution. The **Scale** slider adjusts both text and panel size from 80% to 130% and applies when you release it.

**Outfit update range** controls how far away Hex Appeal checks for outfit changes on loaded characters. It defaults to **45 m** and goes up to **120 m**. Lower it to check fewer characters or set it to **0** to use the original game-driven updates. The range covers outfit assembly, slot visibility and base underwear for witches and enabled character patches. Older game builds without the checked distance route use the original updates.

**Block game input** starts checked. Uncheck it to keep moving or using the camera with the panel open. Interface gestures and focused text entry still belong to the panel. The setting persists in full and quick views. Blocking input does not pause the world. Capture fails open if the interface stops drawing.

**Minimise** opens the quick panel. It keeps the witch selector, brief status and your pinned controls. Right-click a supported item to pin or unpin it, or use **Settings → Quick panel pins**. Outfit-specific controls have individual pins too. **Restore** returns to the full editor.

## Dependencies & paths

Detection checks known DMM layouts, configured storage and the active served-file manifest. Mods may be downloaded or staged in different places; merely finding a download does not prove it is mounted.

Use a manual file/folder path when detection misses a dependency. Clear that field to return to automatic detection. The runtime validates the resource before enabling its feature. The current manager adapter requires DMM's runtime and manifest format; pointing to another manager's folder does not add support for its mounting system.

**Shape preset source** lists Instant Surgery's saved body and local outfit presets. Pick one or paste the path to an exported `.ini`. The selected preset occupies the HEX slot and appears as **PRESET** when using a custom file. Each witch keeps her saved blends, Height and Weight. Choose **Automatic (Hex)** to return to the original preset source. Use a symmetric preset for blending.

| Status | What happens |
| --- | --- |
| Checked reference | Feature available when its live interface is ready |
| Readable, unverified version | Amber warning and hoverable **!**; usage remains allowed |
| Missing install, wrong or unreadable path | Affected feature disabled; correct the path/install |
| Actual native interface failure | Operation rejected with an error; other usable controls remain available |

Saved shapes needing a missing resource wait without being silently changed. Weight and Height need no outfit source. The core release has no Dancer dependency path control. Old outfit path fields remain inert settings data for compatibility; they are not inspected or used to enable a feature. Settings remains accessible when a native requirement is missing.

## Character patches

**F9 → Settings → Character patches** lists the mounted add-ons. Tick or untick a pack to control whether its NPCs appear in Hex Appeal. Your choice survives an update if the pack keeps the same manifest filename. Each character keeps her saved Shape and Wardrobe. See [Character add-ons](Character-add-ons.md) for the dropdown and **Combine instances**.

## Saved files

User preferences and logs live in `%LOCALAPPDATA%/HexAppeal/`. `profiles.ini` stores shape choices, `wardrobe.ini` stores outfit/provider preferences, `interface.ini` stores panel preferences, and `paths.ini` stores overrides. `CharacterAddons.ini` stores pack and add-on Shape preferences. `CharacterWardrobe/` stores add-on outfits. Back up the folder before troubleshooting resets. Do not share it publicly without checking paths and logs first.

## Log

Open **F9 → Log** for feature checks, detected dependency paths and each witch’s body and visibility status. **Checks and mod sources** lists mounted mod names, relevant file routes and conflicts or merged appearances recorded by DMM. **Recent runtime log** shows the latest entries with UTC timestamps.

Use **Save report** to collect graphics details and write a text file to `%LOCALAPPDATA%/HexAppeal/`. **Copy report** copies the on-screen checks and recent log. Review the report for local paths before sharing it. If the game closes before you can open F9, check `HexAppeal.log` in the same folder and the last DMM mount log.
