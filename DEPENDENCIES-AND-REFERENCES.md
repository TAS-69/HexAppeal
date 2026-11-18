# Dependencies, references and credits

I use the following resources as described below. Downloaded body, preset, physics, outfit and plugin files remain separately obtained dependencies. They are not included in Hex Appeal's player, source, runtime or SDK downloads. The release includes my integration code, explicitly credited openly licensed adaptations and game-derived original body/underwear resources and vanilla garment refits. My 0BSD licence does not relicense anyone else's mod or Pearl Abyss assets.

## Mod dependencies

| Resource | Use during development | Use in the release |
| --- | --- | --- |
| **Kasprutz — [VENUS Body for Damiane](https://www.nexusmods.com/crimsondesert/mods/3318), Human/Elf Body** | Inspected body topology and shape; used the separately obtained 2.6 body as the target for fitting vanilla garments | Required external actual body only for the VENUS option. Seven appearance patches reference its existing prefab; Vanilla does not use VENUS. Runtime validates its served route. No VENUS model or body deltas are bundled. The included vanilla garment outputs were fitted against its shared body baseline. |
| **Hex0x0 / Hex — [Hex's Hentai Preset](https://www.nexusmods.com/crimsondesert/mods/4234)** | Compared 1.2 and 1.3, including updated stomach dimensions; checked native regional composition | Required external preset for HEX. Read into memory and resolved through Instant Surgery. No complete preset file or baked copy of its parameters is distributed. |
| **AlexeySova — [Instant Surgery](https://www.nexusmods.com/crimsondesert/mods/3660)** | Inspected the installed 0.9.6 native functions, metadata and pose routes; tested isolated copies | Required loaded external plugin. My adapter calls/hooks its native resolver, deformation and pose interfaces in memory. Short matching signatures and layout offsets are included in my adapter; its ASI and implementation are not bundled or patched on disk. This is a private-interface integration, not an upstream SDK. |
| **AlexeySova — [Two Reasons Body Physics](https://www.nexusmods.com/crimsondesert/mods/3636)** | Inspected the 1.0.7 pose route to preserve coordinated body/outfit updates | Optional. When loaded, my adapter uses its `TwoReasonsEditorPoseAppliedV1` acknowledgement interface. No simulation implementation, plugin or configuration is copied. The checked interface must be available for that route. |

AlexeySova approved the described Instant Surgery / Two Reasons integration on the condition that I do not copy his mods and that players download and install them separately. Instant Surgery remains required for native shaping; Two Reasons remains optional as disclosed in my request. My adapter uses the installed plugins in memory. The approval does not grant redistribution or relicensing of either mod. Original correspondence stays private.

## Historical outfit reference

**[Sexy Witches of Pywel Body and Dress Reshaped](https://www.nexusmods.com/crimsondesert/mods/2891)** by **[Vincent1681](https://www.nexusmods.com/profile/Vincent1681)** was inspected as a visual Weight/outfit reference during development. Native Weight uses fixed limb/waist adjustments, with no requirement to install or read that mod and no geometry or preset data embedded from it.

The Dancer outfit integration, supplied-colour bindings, dependency discovery and setup helper are excluded from this core release pending permission. Earlier private material, fitting and split-model experiments are excluded too. No current outfit dependency or installation step is implied by this historical credit. Any approved future integration will be a separate Wardrobe provider patch using the original external download.

## Compatibility and research references

| Resource | Scope |
| --- | --- |
| **IxAmxLegion — [Legion's Slider of Bodies / Auto-Refit](https://www.nexusmods.com/crimsondesert/mods/3951)** | Inspected as an available shaping/refitting reference. Not a runtime dependency; its app, presets and fitting implementation are not included or operated by Hex Appeal. |
| **junhx2 — [Spouse Overhaul](https://www.nexusmods.com/crimsondesert/mods/3999)** | Observed outfit rebuild/visibility behavior and compatibility. No implementation copied. Custom spouse outfits remain outside normal Wardrobe ownership. |
| **Khione95 — [Character Creator](https://www.nexusmods.com/crimsondesert/mods/837)** | Barber/character ownership compatibility research. No implementation or assets copied; no blanket licence permission is inferred. |
| **Kasprutz — Damiane Armor Refit Pack for VENUS BODY, No Panties 2.5** | Inspected the mounted shared boot and underwear routes to isolate conflicts. Witch-only original/refitted resources now use private paths. Its files remain unchanged and are neither copied into the release nor required. The inspected base underwear still exposes one combined part. |
| **Desert Mod Manager** | External install/mount manager and loaded runtime/manifest. My own reader checks active resource routes and configured storage. DMM executables/parser code are not bundled. The current adapter does not guarantee other managers' formats. |

For Spouse Overhaul and Character Creator, my adapter inspects loaded module identity and appearance-hook destinations. An unverified file version produces a compatibility warning; native signatures, bounded executable hook chains, live actor ownership and assembly acknowledgement remain separate operation checks. It neither edits their files nor copies their implementation.

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

The included default Outfit Refits contain **forty modified vanilla game garments**, not downloaded mod outfits. They preserve the original skin/UV/index structures and were fitted to the separately obtained VENUS baseline. Each body option contains **seven appearance patches**. VENUS references the external body. Vanilla includes three original game body resource sets and two original underwear sets at private paths; compiled prefab references alone are adjusted to those paths. Neither option includes a downloaded mod body. No Dancer PAC/DDS/property file, source preset, external ASI, extracted rig, private permission message or research bundle belongs in the release.

Private permission correspondence is kept separately. It is not a blanket permission to redistribute other mods or an endorsement of every future integration. Outfit creators who want deeper support can use the external [Wardrobe patch guide](runtime/sdk/AUTHOR-PATCH-GUIDE.md); their own assets and any permissioned patches remain in their own packages.

The current release also compares the original VENUS 2.6 Human Body and recognizes its external hash. Its body/hand records and physics match Elf Body; the existing refit field is unchanged because it ignores target head displacement. No Human PAC/HKX or Elf cloak-rule patch is copied or distributed. This extends the supported external variant without changing the dependency-only asset boundary. See [the comparison](docs/VENUS-COMPATIBILITY.md).

## Hide outfit extension

Hide outfit uses my checked adapter to remove recognised default armour records from a live witch and restore their retained metadata when disabled. It uses the game's outfit assembly path. The separate Base underwear control uses the existing visibility hook; hiding clothing does not hide underwear. The selected VENUS or original Vanilla body remains the rendered body. No additional model file is generated or cached by this visibility feature. The transient records are game-process metadata, not copied model files.

External providers opt in with `HA_FOLLOW_OUTFIT_HIDE` on declared per-witch visibility controls, authored complete prefab states, or their own checked renderer bridge. Their own package supplies every required asset and skin-coverage fix. Unregistered/custom spouse and barber outfits are retained. The original Wardrobe ABI remains unchanged; a separate optional preference API exposes the Hide outfit state. See the [provider guide](runtime/sdk/OUTFIT-VISIBILITY.md).

Character Creator 9.1.41 is now a recognised coexistence version after the scoped core session and hook tests. Its original files and code remain external. This does not certify every feature of that mod or future versions; individual hook, actor and operation checks remain active.

## Custom outfit underwear

The optional OutfitPolicy extension lets a separately installed provider declare that its owned replacement supplies its own underwear. My runtime then suppresses the normal base underwear only while that native outfit instance is active. It does not change the saved visibility preference or replace shared Damiane assets. Providers declare their own garment visibility and supply all required skin coverage. Hide outfit can retain their underwear; Hide base bra & panties hides the active linked pair. Default restores natural base visibility. This API is fixture-tested; the separate custom outfit still needs rendered acceptance.
