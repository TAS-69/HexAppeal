<p align="center"><img src="docs/assets/banner.png" alt="Hex Appeal — Even dark magic deserves better curves" width="100%"></p>
<p align="center"><b>VENUS. HEX. FLAT. Your questionable little blend of all three.</b></p>
<p align="center"><img src="https://img.shields.io/badge/RELEASE-1.0.2-631d48?style=for-the-badge&amp;labelColor=201525" alt="Release 1.0.2"> <img src="https://img.shields.io/badge/ORIGINAL_CODE-0BSD-dcb782?style=for-the-badge&amp;labelColor=201525" alt="Original code: 0BSD"> <img src="https://img.shields.io/badge/OPEN_PANEL-F9-c35a94?style=for-the-badge&amp;labelColor=201525" alt="Open panel: F9"></p>

I made **Hex Appeal** to give Crimson Desert's human witches a choice of the actual installed **VENUS Human Body or Elf Body**, or their own **Vanilla bodies**, with an in-game shape editor and a shared **Wardrobe** for compatible outfits. Pick a witch, mix the top and bottom, and carry on. Changes save and apply automatically.

**Version 1.0.2:** corrected DMM import grouping and matching mod/ASI version labels. Remove the previous package before installing; keep your saved settings. One player ZIP includes both body choices and the F9 editor. Vanilla preserves each witch's original midpoint; VENUS uses your separately installed body. Dancer remains a separate future patch pending permission.

**[Downloads](https://github.com/TAS-69/HexAppeal/releases) · [Install](docs/wiki/Installation.md) · [Player guide](docs/wiki/Home.md) · [Wardrobe SDK](docs/wiki/SDK-quick-start.md) · [Requirements & credits](DEPENDENCIES-AND-REFERENCES.md)**

## A little witchcraft

| Shape | Wardrobe | Make it yours |
| :--- | :--- | :--- |
| Independent HEX / baseline / FLAT top and bottom, blended on one pad | Default outfit or a separately installed SDK provider | Six saved witch profiles and All witches |
| All nine combinations, with gentle sticky points | Provider-defined variants, toggles and refresh | Independent Height and Weight |
| Compatible body and outfit pose deformation | Independent Hide outfit and Base underwear sections | F9, rescaling, input blocking and a pinnable quick panel |
| Included default-outfit refits | Open SDK for variants, toggles and opt-in clothing/underwear visibility | Automatic saves and restoration after loading |

Supported human forms: **Lyselia, Bari, Areciel, Elowen, Sylvia and White Crow**, including the alternate Bari appearance. Bird/drone White Crow is outside this scope.

<p align="center"><img src="docs/assets/shape-pad.svg" alt="Nine top and bottom combinations on the shape pad, with separate height and weight controls" width="820"></p>

## Get started

1. Install **DMM** and **Instant Surgery**. Add **Hex's Hentai Preset** for HEX; add one **VENUS Human Body or Elf Body** variant if choosing VENUS. DMM includes ASI loader installation.
2. Close the game and import **HexAppeal-1.0.2.zip** into **Definitive Mod Manager**. Enable exactly **[01] VENUS** or **[02] Vanilla**, plus its ASI switch, then mount once. Disable the other body choice. When upgrading, remove the previous Hex Appeal package first to avoid duplicate entries; keep your saved settings. See [upgrading](docs/wiki/Installation.md#update-or-remove).
3. Load a save near a witch and press **F9**. Check **Settings → Dependencies & paths** if something is unavailable.

The [installation guide](docs/wiki/Installation.md) covers packages and priorities. Players need no Python or outfit setup; Python is needed for building tools.

## In game

<img src="docs/assets/bodyshaping.png" alt="Actual gameplay screenshot: the Hex Appeal shape panel beside Elowen in her cabin" width="100%">

*Actual gameplay screenshot supplied with the project. The banner is promotional artwork.*

## Outfit creators welcome

Register an outfit's real name and the options it actually supports. Wardrobe handles the menu, saved choices, witch targeting and pins; your mod supplies its own assets. Use engine visibility parts, complete prefab variants or your own renderer. Start with the [beginner SDK guide](docs/wiki/SDK-quick-start.md), then use the [advanced examples](docs/wiki/SDK-examples.md) for complete colour/toggle matrices, provider lifecycle, effective visibility, rebuilt instances and refresh/error handling.

## A few things to know

Base underwear does not remove arbitrary geometry from other outfits. Each provider declares what its resources support. Extreme proportions can still clip. See [compatibility and tested limits](docs/wiki/Compatibility.md).

**Validation:** 22 offline runtime checks, 36 SDK definition cases and 38 helper tests; 44 isolated native weighted cases, all nine regional combinations and eleven unchanged reference poses. Both body options received close idle Elowen checks on the earlier runtime. The corrected layout passed actual DMM import and mount checks. See [verification](docs/VERIFICATION-1.0.2.md) for exact binaries, startup evidence and limits.

Downloaded bodies, presets, plugins and outfit files are **external dependencies**. They are not included here. My code and SDK are **0BSD**; compiled overlay/library adaptations retain their own notices. Original witch bodies, underwear and default garment resources remain Pearl Abyss content. Thanks to **Kasprutz, Hex0x0, AlexeySova, Vincent1681**, and everyone credited in [dependency use & credits](DEPENDENCIES-AND-REFERENCES.md).
