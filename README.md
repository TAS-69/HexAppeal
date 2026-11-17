<p align="center"><img src="docs/assets/banner.png" alt="Hex Appeal — Even dark magic deserves better curves" width="100%"></p>
<p align="center"><b>VENUS. HEX. FLAT. Your questionable little blend of all three.</b></p>
<p align="center"><img src="https://img.shields.io/badge/RELEASE-1.0.0-631d48?style=for-the-badge&amp;labelColor=201525" alt="Release 1.0.0"> <img src="https://img.shields.io/badge/ORIGINAL_CODE-0BSD-dcb782?style=for-the-badge&amp;labelColor=201525" alt="Original code: 0BSD"> <img src="https://img.shields.io/badge/OPEN_PANEL-F9-c35a94?style=for-the-badge&amp;labelColor=201525" alt="Open panel: F9"></p>

I made **Hex Appeal** to bring the actual installed **VENUS Human Body or Elf Body** to Crimson Desert's human witches, with an in-game shape editor and a shared **Wardrobe** for compatible outfits. Pick a witch, mix the top and bottom, and carry on. Changes save and apply automatically.

**Version 1.0.0 — Nexus preparation:** one player ZIP includes the runtime, body routing, default outfit refits and independent Weight. Dancer outfit integration, colours and setup are excluded pending permission; any future integration will be a separate patch. Wardrobe remains open to generic SDK providers. See [verification](docs/VERIFICATION-1.0.0.md) for the checked scope.

**[Downloads](https://github.com/TAS-69/HexAppeal/releases) · [Install](docs/wiki/Installation.md) · [Player guide](docs/wiki/Home.md) · [Wardrobe SDK](docs/wiki/SDK-quick-start.md) · [Requirements & credits](DEPENDENCIES-AND-REFERENCES.md)**

## A little witchcraft

| Shape | Wardrobe | Make it yours |
| :--- | :--- | :--- |
| Independent HEX / VENUS / FLAT top and bottom, blended on one pad | Default outfit or a separately installed SDK provider | Six saved witch profiles and All witches |
| All nine combinations, with gentle sticky points | Provider-defined variants, toggles and refresh | Independent Height and Weight |
| Compatible body and outfit pose deformation | Independent Hide outfit and Base underwear sections | F9, rescaling, input blocking and a pinnable quick panel |
| Included default-outfit refits | Open SDK for variants, toggles and opt-in clothing/underwear visibility | Automatic saves and restoration after loading |

Supported human forms: **Lyselia, Bari, Areciel, Elowen, Sylvia and White Crow**, including the alternate Bari appearance. Bird/drone White Crow is outside this scope.

<p align="center"><img src="docs/assets/shape-pad.svg" alt="Nine top and bottom combinations on the shape pad, with separate height and weight controls" width="820"></p>

## Get started

1. Install one **VENUS Human Body or Elf Body** variant, **Hex's Hentai Preset**, **Instant Surgery** separately. DMM can install its bundled ASI loader.
2. Close the game and import **HexAppeal-1.0.0.zip** into **Definitive Mod Manager**. Enable Hex Appeal on Mods and its ASI switch, review conflicts and mount once.
3. Load a save near a witch and press **F9**. Check **Settings → Dependencies & paths** if something is unavailable.

The [installation guide](docs/wiki/Installation.md) covers packages and priorities. Players need no Python or outfit setup; Python is needed for building tools.

## In game

<img src="docs/assets/bodyshaping.png" alt="Actual gameplay screenshot: the Hex Appeal shape panel beside Elowen in her cabin" width="100%">

*Actual gameplay screenshot supplied with the project. The banner is promotional artwork.*

## Outfit creators welcome

Register an outfit's real name and the options it actually supports. Wardrobe handles the menu, saved choices, witch targeting and pins; your mod supplies its own assets. Use engine visibility parts, complete prefab variants or your own renderer. Start with the [beginner SDK guide](docs/wiki/SDK-quick-start.md), then use the [advanced examples](docs/wiki/SDK-examples.md) for complete colour/toggle matrices, provider lifecycle, effective visibility, rebuilt instances and refresh/error handling.

## A few things to know

Base underwear does not remove arbitrary geometry from other outfits. Each provider declares what its resources support. Extreme proportions can still clip. See [compatibility and tested limits](docs/wiki/Compatibility.md).

**Validation:** this binary passed 21 runtime checks, 33 SDK cases within that suite, 23 helper tests and 44 native shaping cases. The unchanged body/refit set retains its 47 checks. Live Elowen checks covered independent visibility, startup restoration and three SDK refresh cycles. All four SDK example targets compile. See the [verification scope](docs/VERIFICATION-1.0.0.md) for the exact release binary, results and live-test limits, and [Compatibility](docs/wiki/Compatibility.md) for tested limits.

Downloaded bodies, presets, plugins and outfit files are **external dependencies**. They are not included here. My code and SDK are **0BSD**; compiled overlay/library adaptations retain their own notices. Game-derived default garment refits remain Pearl Abyss content. Thanks to **Kasprutz, Hex0x0, AlexeySova, Vincent1681**, and everyone credited in [dependency use & credits](DEPENDENCIES-AND-REFERENCES.md).
