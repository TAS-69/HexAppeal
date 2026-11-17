# Verification: 1.0.0

**Refreshed 17 November 2026.** This replaces the initial split 1.0.0 downloads with the latest checked source and one combined player archive. The previous release files and tag were backed up before replacement. The public number remains 1.0.0; development numbering remains independent.

The packaged Windows x64 runtime SHA-256 is:

`b7cbb634c41d5970d9e60d76a361f1d15fb9f7a98e51979989fb375815a0ab9a`

| Check | Result |
| --- | --- |
| Full offline runtime suite against the exact public binary | **23/23 passed** |
| SDK definition/example tests, included within that suite | **29/29 passed** |
| Setup, packaging, body-variant and shading helper tests | **34/34 passed** |
| Appearance/default-garment asset validation | **47/47 passed** |
| Standalone CMake SDK build, Windows x64 Release | **All four provider object targets compiled** |

The runtime build also compiles the new generated-header examples. The advanced guide's JSON validates and its full C++ blocks match the compiled examples. The custom-renderer example deliberately declares three bridge functions for the adopting mod to implement; it is an object integration example, not a playable standalone ASI.

The seven appearance patches and forty game-derived garment outputs retain their checked hashes, original skin/UV/index structures and detail-level layouts. Source inputs and installed dependencies were read without changing their contents. No downloaded body, preset, external ASI, Dancer model, property file or texture is packaged. The player archive contains only Hex Appeal's plugin, routing/refit outputs, setup code, guides and licences.

Runtime coverage includes pad composition, automatic persistence/application, independent proportions, frame ABI, input/menu lifecycle, pins, native ownership/restoration, Wardrobe transport/providers, effective Base underwear values, complete prefab matrices, external palettes, served dependency discovery, appearance-hook checks and per-feature compatibility. Native checks use isolated copies of the inspected plugins. Their original files are verified unchanged.

## In-game evidence and limits

The release includes the tested 0.5.8 source changes. That development binary, `25640380fe36d4e511a3924c5f62a32bd3ceecabe17f5e85002db267b0114a6a`, passed a sampled Elowen session: all six feature checks ready with an unverified Character Creator warning; an incompatible external preset blocked only HEX; clearing the override recovered detection and the saved shape. Saved Dancer / Arcane Pink remained active.

Earlier sampled builds cover all nine Human Body/Dancer combinations, independent minimum Height with maximum Weight on the default robe, Default/Dancer restoration, and automatic White/Black colour refresh using original external resources. The combined layout was imported and mounted in DMM 5.0.1 with seven appearance routes and forty garment routes verified. Its optional Dancer references were tested inside the main entry with the old separate entry disabled. See [VENUS compatibility](VENUS-COMPATIBILITY.md), [dependency compatibility](DEPENDENCY-COMPATIBILITY.md) and [player packaging](PLAYER-PACKAGE.md) for individual hashes and scope.

**This public version-stamped rebuild has not had a separate fresh game session.** Its exact binary passed the offline suite above. Earlier direct Lyselia/Elowen checks, player-reported broader witch/default-fit/moving-cloth checks and sampled native accessory visibility tests retain their original scope. Generic prefab matrices require rendered acceptance with an adopting outfit's own assets.

These results do not establish every witch in motion, every detail level, controller behavior, arbitrary outfit fitting or long-term stability. Minor clipping and unresolved intermittent material behavior remain documented in [Compatibility](wiki/Compatibility.md). Matching `VERIFICATION.json`, `CONTENTS.json` and `SHA256SUMS.txt` accompany the downloads.
