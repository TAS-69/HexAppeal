# Verification: 1.0.0

**Core-only Nexus preparation, 17 November 2026.** Version 1 remains 1.0.0 until the Nexus release. This explicitly requested replacement excludes Dancer integration; prior packages and tags are backed up before replacement.

The packaged Windows x64 runtime SHA-256 is:

`e11bc8df1fe41ea7722f4c55ec51a8e287abc0eddf1190709bf4b6c8ed8784b3`

| Check | Result |
| --- | --- |
| Full offline suite against this exact public binary | **21/21 passed** |
| SDK definition/example cases included within that suite | **29/29 passed** |
| Packaging, body-variant and shading helper tests | **22/22 passed** |
| Appearance/default-garment validation | **47/47 passed** |
| Isolated native Weight/preset/Height cases | **44/44 passed** |
| Existing native pose output comparisons | **11/11 byte-identical** |
| Standalone Windows x64 Release SDK build | **All four provider object targets compiled** |

The two removed runtime checks and fifteen removed helper cases covered the excluded outfit integration. The remaining suites include its replacement boundary tests: unavailable legacy selections, Default restoration, inert saved fields, forbidden setup/resource filenames and binary-marker rejection. The test total reduction is scope removal, not skipped failing tests.

## Included and excluded

The seven appearance patches and forty game-derived default-garment outputs retain their checked hashes, skin/UV/index structures and stored detail-level layouts. Downloaded body, preset, external ASI and outfit files are not packaged.

No Dancer palette implementation, source scanner, model routing, setup launcher/helper or local reference files are included in the player, source or SDK archives. The runtime is checked for its excluded resource identifiers. Old outfit/colour/path settings keys remain inert compatibility data; they cannot enable that outfit. Weight is independent of outfit files.

Wardrobe retains the generic provider API, variants, toggles, visibility parts, complete prefab matrices, Base underwear policy and explicit refresh extension. New assets need their own rendered checks. The custom renderer example compiles as an object and intentionally requires its adopting mod to implement the three renderer bridge functions.

## Native and live scope

The isolated native probe used a test copy of Instant Surgery and rig buffers, without supplying an outfit source. It exercised all nine regional combinations, native transaction/physics acknowledgement, repeat-upload stability, modifier conflict handling, 36 weighted regional/Height extreme cases and eight weighted VENUS/FLAT cases without a Hex preset. Original dependency inputs were unchanged. All eleven previous pose files matched exactly.

Earlier development sessions sampled Lyselia/Elowen shaping and default outfits. Wider all-witch fit and moving-cloth checks were player-reported. Generic native accessory visibility/Base underwear linkage has sampled Elowen coverage. The combined one-manifest layout was previously imported and mounted in DMM 5.0.1 with seven appearance and forty garment routes verified; that unchanged layout acceptance is retained.

**This exact core-only public rebuild has not had a fresh in-game session or manager import.** Its binary, package contents and native-buffer behavior have the checks above. These results do not certify every witch in motion, every detail level, controller behavior, arbitrary clothing or long-term stability. See [Compatibility](wiki/Compatibility.md).

The release includes matching `VERIFICATION.json`, `CONTENTS.json` and `SHA256SUMS.txt`.
