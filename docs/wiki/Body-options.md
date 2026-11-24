# Body options

Enable **one** option inside Hex Appeal in DMM: **[01] VENUS** or **[02] Vanilla**. Disable the other option before mounting. Both use the same F9 editor, save system and Wardrobe SDK. The runtime reports mixed body routes instead of applying shaping to an inconsistent mount.

| | VENUS | Vanilla |
|---|---|---|
| Body | The separately installed Venus Body for Damiane (Human Body or Elf Body) | Each witch's original game body |
| Pad midpoint | VENUS | That witch's original proportions |
| Default clothing | Included VENUS refits | Original game clothing |
| Venus Body for Damiane download | Required | Not required |
| HEX and FLAT blends | Yes | Yes, relative to the original body |
| Height, Weight and Wardrobe | Yes | Yes |

For the original Vanilla appearance, centre the pad and set Height and Weight to zero. Original body variants, character scale, head and customization settings are preserved. The stock body and base-underwear resources have private paths so body or armour replacements installed for Damiane cannot replace the Vanilla witch baseline. Shared witch boots are isolated in both options.

HEX reads the installed Hex's Hentai Preset. Automatic detection prefers its original-body version when one is available. Its Venus Body for Damiane version can also supply relative shaping values; the result depends on the chosen base. A different starting body does not produce an identical final silhouette from the same slider values.

Close the game before changing body options, choose the other option in DMM and mount again. No setup executable or second setup mount is needed. Clothes made for Venus Body for Damiane still need the VENUS option or their own compatible refit.

## What works without each dependency

| Missing download | Available functionality |
|---|---|
| Venus Body for Damiane | Use the Vanilla option. Its original bodies, HEX/FLAT blends, proportions and Wardrobe do not require Venus Body for Damiane. |
| Hex's Hentai Preset | The selected baseline, FLAT, Height, Weight and Wardrobe remain available. HEX choices wait for the preset; choose a baseline or FLAT combination to continue shaping. |
| Two Reasons Body Physics | Shaping and Wardrobe work without its additional physics. If installed, its loaded interface must be compatible with coordinated shaping. |
| Character Creator | Hex Appeal's own editor, default Wardrobe and visibility controls remain available. |
| An optional outfit/provider | Default witch outfits and the other installed providers remain available. An unavailable outfit keeps its saved preference while its feature waits. |
| Instant Surgery | Mounted body and clothing files remain installed, but native interactive shaping needs Instant Surgery. Install it and restart to use the interactive features. |
| DMM/runtime integration | The supported active-file verification and mounted body integration are unavailable. Install/enable DMM and mount the selected body option. |

The F9 dependency settings remain accessible when a feature is blocked. Saved choices are retained rather than discarded when a dependency goes missing.

## Detection and recovery

Automatic detection reads DMM's configured mod directory and active mounted-file list. It also checks portable manager locations, libraries inferred from active resource paths and Instant Surgery's preset folders. Renamed checked presets, nested folders, Unicode paths, quoted paths and environment variables are supported.

If detection still misses a preset, open **Settings → Dependencies & paths** and select its extracted `.ini` file or the folder containing it. Clear an override to return to automatic detection. Selecting a ZIP/7z/RAR explains that it must be extracted first.

An unverified version gets an amber warning while remaining usable. A missing or unreadable required file blocks the feature that needs it. An actual incompatible native interface also blocks that operation. A manual path is checked as entered; an invalid override is reported instead of silently replaced with a different file.
