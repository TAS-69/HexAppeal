# Dependencies, references and credits

I use the following resources as described below. Downloaded body, preset, physics, outfit and plugin files remain separately obtained dependencies. They are not included in Hex Appeal's player, source, runtime or SDK downloads. The release includes my integration code, explicitly credited openly licensed adaptations and game-derived vanilla garment refits. My 0BSD licence does not relicense anyone else's mod or Pearl Abyss assets.

## Mod dependencies

| Resource | Use during development | Use in the release |
| --- | --- | --- |
| **Kasprutz — [VENUS Body for Damiane](https://www.nexusmods.com/crimsondesert/mods/3318), Elf Body** | Inspected body topology and shape; used the separately obtained 2.6 body as the target for fitting vanilla garments | Required external actual body. Seven appearance patches reference its existing prefab. Runtime validates its served route. No VENUS model or body deltas are bundled. The optional vanilla garment outputs were fitted against it. |
| **Hex0x0 / Hex — [Hex's Hentai Preset](https://www.nexusmods.com/crimsondesert/mods/4234)** | Compared 1.2 and 1.3, including updated stomach dimensions; checked native regional composition | Required external preset for HEX. Read into memory and resolved through Instant Surgery. No complete preset file or baked copy of its parameters is distributed. |
| **AlexeySova — [Instant Surgery](https://www.nexusmods.com/crimsondesert/mods/3660)** | Inspected the installed 0.9.6 native functions, metadata and pose routes; tested isolated copies | Required loaded external plugin. My adapter calls/hooks its native resolver, deformation and pose interfaces in memory. Short matching signatures and layout offsets are included in my adapter; its ASI and implementation are not bundled or patched on disk. This is a private-interface integration, not an upstream SDK. |
| **AlexeySova — Two Reasons Body Physics** | Inspected the 1.0.7 pose route to preserve coordinated body/outfit updates | Optional. When loaded, my adapter uses its `TwoReasonsEditorPoseAppliedV1` acknowledgement interface. No simulation implementation, plugin or configuration is copied. The checked interface must be available for that route. |
| **Vincent1681 / VincentQuirin — [Sexy Witches of Pywel Body and Dress Reshaped](https://www.nexusmods.com/crimsondesert/mods/2891)** | Inspected FOMOD 1.1 body/dress and supplied colours as a weight/outfit reference. Private fitting, material and split-model experiments were tested and excluded | Optional external body reference gates native limb/waist Weight adjustments; these are fixed native adjustments inspired by the reference, not an exact mesh reproduction. Dancer uses the original external upper/lower models, properties and five supplied textures through local file references. The next combined package can prepare those references inside its installed folder, without a separate DMM entry. No original or modified Dancer file, split patch or recolour is bundled. Independent embedded-top removal is not supplied. |

The Dancer helper creates four local hard links to the player's original extracted dependency: its two original models, original properties and one selected supplied texture. The next combined player package includes the setup code and a launcher, not those resource files. Setup can detect a complete FOMOD beside the installed mod or accept an explicit source path. It runs **before the initial DMM mount**, so a fresh install needs one mount. Python 3.12+ with its launcher is required for this optional setup step.

In combined-package mode, the references are created inside the player's **installed** Hex Appeal entry. The published 1.0.0 helper instead creates a separate external resource entry. Both modes use original file contents without mesh, material or texture editing. Runtime colour changes replace only the generated texture directory entry. The player keeps the separately obtained FOMOD. No reference is created when its required files are missing; readable unverified versions warn rather than being rejected by hash alone.

Hard links expose the complete dependency file contents and can retain data if the original filename is removed. A configured local installation therefore must **not** be archived or redistributed as Hex Appeal. Release packaging uses the clean source checkout, rejects dependency hashes and bundles none of these links or dependency resources. This is a distribution boundary, not a claim that Dancer can render without reading its original assets.

## Compatibility and research references

| Resource | Scope |
| --- | --- |
| **IxAmxLegion — [Legion's Slider of Bodies / Auto-Refit](https://www.nexusmods.com/crimsondesert/mods/3951)** | Inspected as an available shaping/refitting reference. Not a runtime dependency; its app, presets and fitting implementation are not included or operated by Hex Appeal. |
| **junhx2 — [Spouse Overhaul](https://www.nexusmods.com/crimsondesert/mods/3999)** | Observed outfit rebuild/visibility behavior and compatibility. No implementation copied. Custom spouse outfits remain outside normal Wardrobe ownership. |
| **Khione95 — [Character Creator](https://www.nexusmods.com/crimsondesert/mods/837)** | Barber/character ownership compatibility research. No implementation or assets copied; no blanket licence permission is inferred. |
| **Desert Mod Manager** | External install/mount manager and loaded runtime/manifest. My own reader checks active resource routes and configured storage. DMM executables/parser code are not bundled. The current adapter does not guarantee other managers' formats. |

## Openly licensed code and tools

These are code/tool credits with explicit upstream licences. Their terms and required notices are retained where code is distributed; they are distinct from downloaded content-mod dependencies.

| Resource | Use | Terms |
| --- | --- | --- |
| [ModActivator / Alzucard](https://github.com/Alzucard/ModActivator), `abc673c6e7a4ebf44c3e4273088d6331f3ad9a43` | Adapted overlay/input infrastructure compiled into the runtime | MIT-0; retained notice |
| [Trinity / XeTrinityz](https://github.com/XeTrinityz/Trinity), Master Looter ancestry | Upstream DX12/controller overlay adaptations retained through that code | MIT; retained full notices |
| [CrimsonDesertTools / Quang Trinh (tkhquang)](https://github.com/tkhquang/CrimsonDesertTools/tree/db82c301af48cbe6d1595fc76a8139b85f85d788/CrimsonDesertLiveTransmog) | Native assembly/material/frame research adapted into checked Wardrobe switching and refresh | MIT; retained relevant LiveTransmog notice |
| [Dear ImGui](https://github.com/ocornut/imgui), 1.91.5 | Vendored menu sources and DX12 backend adaptation | MIT; retained notice |
| [MinHook / Tsuda Kageyu](https://github.com/TsudaKageyu/minhook), including HDE | Vendored hook/prologue handling | BSD 2-Clause and bundled HDE terms; retained notices |
| [CrimsonForge / hzeem](https://github.com/hzeem/CrimsonForge), `09f821638da4e1f04b26be43af408b4d59337ad3` | External development model/archive tools used to create/validate the default garment refits | MIT; not bundled or a gameplay requirement |
| [CDUMM / faisalkindi](https://github.com/faisalkindi/CDUMM), `4d5a0f8cc6ea841d246509d7b9f8c80954c2fe0d` | External development archive reader | MIT; not bundled or a gameplay requirement |

Full distributed code notices are in [runtime/licenses](runtime/licenses/UPSTREAM-NOTICES.md). My original code, SDK, helper and examples use [0BSD](LICENSE), copyright **TAS-69**. Licence notices are preserved verbatim.

## Distribution boundary

The optional Outfit Refits add-on contains **forty modified vanilla game garments**, not downloaded mod outfits. They preserve the original skin/UV/index structures and were fitted to the separately obtained VENUS baseline. The Bodies core contains **seven reference patches**, not a body model. No Dancer PAC/DDS/property file, source preset, external ASI, extracted rig, private permission message or research bundle belongs in the release.

Private permission correspondence is kept separately. It is not a blanket permission to redistribute other mods or an endorsement of every future integration. Outfit creators who want deeper support can use the external [Wardrobe patch guide](runtime/sdk/AUTHOR-PATCH-GUIDE.md); their own assets and any permissioned patches remain in their own packages.

Development 0.5.7 also compares the original VENUS 2.6 Human Body and recognizes its external hash. Its body/hand records and physics match Elf Body; the existing refit field is unchanged because it ignores target head displacement. No Human PAC/HKX or Elf cloak-rule patch is copied or distributed. This extends the supported external variant without changing the dependency-only asset boundary. See [the comparison](docs/VENUS-COMPATIBILITY.md).
