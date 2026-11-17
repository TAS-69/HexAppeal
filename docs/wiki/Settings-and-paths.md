# Settings and paths

## Panel controls

**F9** opens the panel. Change the shortcut and panel scale in Settings if needed.

**Block game input** starts checked. Uncheck it to keep moving or using the camera with the panel open. Interface gestures and focused text entry still belong to the panel. The setting persists in full and quick views. Blocking input does not pause the world. Capture fails open if the interface stops drawing.

**Minimise** opens the quick panel. It keeps the witch selector, brief status and your pinned controls. Right-click a supported item to pin or unpin it, or use **Settings → Quick panel pins**. Outfit-specific controls have individual pins too. **Restore** returns to the full editor.

## Dependencies & paths

Detection checks known DMM layouts, configured storage and the active served-file manifest. Mods may be downloaded or staged in different places; merely finding a download does not prove it is mounted.

Use a manual file/folder path when detection misses a dependency. Clear that field to return to automatic detection. The runtime validates the resource before enabling its feature. The current manager adapter requires DMM's runtime and manifest format; pointing to another manager's folder does not add support for its mounting system.

| Status | What happens |
| --- | --- |
| Checked reference | Feature available when its live interface is ready |
| Readable, unverified version | Amber warning and hoverable **!**; usage remains allowed |
| Missing install, wrong or unreadable path | Affected feature disabled; correct the path/install |
| Actual native interface failure | Operation rejected with an error; other usable controls remain available |

Saved shapes needing a missing resource wait without being silently changed. In development **0.5.9**, a missing optional Dancer source affects only its outfit/colours; Weight and Height remain available when native shaping is ready. The path field is now **Dancer source file or mod folder**. Existing `WeightReference` overrides are read as legacy Dancer paths, preserving the selected location. Settings remains accessible. The published 1.0.0 package predates this separation.

## Saved files

User preferences and logs live in `%LOCALAPPDATA%/HexAppeal/`. `profiles.ini` stores shape choices, `wardrobe.ini` stores outfit/provider preferences, `interface.ini` stores panel preferences, and `paths.ini` stores overrides. Back up the folder before troubleshooting resets. Do not share it publicly without checking paths and logs first.
