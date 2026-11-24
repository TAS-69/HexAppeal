# Verification: 1.0.0

**Core-only Nexus preparation, 17 November 2026.** Version 1 remains 1.0.0 until the Nexus release. This replacement adds independent outfit visibility, its optional SDK extension and a shorter refresh interval. Dancer integration remains excluded pending permission.

The packaged Windows x64 runtime SHA-256 is:

`b98c327a7d177bce679036d6cc720cc81e5e1addfae3cd443360f3221e8e099b`

| Check | Result |
| --- | --- |
| Full offline suite against this exact public binary | **21/21 passed** |
| SDK definition/example cases within that suite | **33/33 passed** |
| Packaging, body-variant and shading helper tests | **23/23 passed** |
| Unchanged appearance/default-garment validation | **47/47 passed** |
| Isolated native Weight/preset/Height cases with current source | **44/44 passed** |
| Existing native pose output comparisons | **11/11 byte-identical** |
| Windows x64 SDK example build | **All four provider object targets compiled** |

## Live checks on this binary

Two sessions near Elowen checked the default robe and hat with all four combinations of **Hide outfit** and **Hide base bra & panties**. Clothing removal retained visible underwear; enabling both removed both; restoring clothing kept the underwear preference independent. Removal/restoration was repeated. Base underwear was also visibly restored while clothing remained hidden.

The Hide outfit quick-panel pin, Minimise, quick toggle, unpin and Restore worked. A non-participating SDK outfit stayed visible when Hide outfit was enabled. After a full game restart, the saved Hide outfit selection applied automatically without touching either control.

Three explicit public SDK refresh calls used a private test provider referencing an already-loaded original game robe. Each Default → selected cycle received assembly acknowledgements and visibly returned active. **RefreshDelayMs=500** produced **625 ms** observed waits in all three cycles. This tests native refresh cadence, not an external outfit's texture cache or colour correctness. The previous 1500 ms delay was introduced for stale colours; it was not a verified cure for blotches. Providers still need rendered resource-change checks and may require a longer configured interval.

The temporary helper and its saved test control were removed after the game closed. Saved body/profile files and original outfit choices matched their backups exactly. Elowen was left on Default with both visibility controls off. No save was restored or edited. See [live results](../build-data/live-visibility-20261117.json).

This was a close front/oblique idle Elowen test. It does not certify every witch, detail level, controller, moving-cloth case or long session. Base underwear can briefly reappear during an outfit rebuild before its independent policy reapplies. Outfit creators must verify skin coverage and every supplied visibility state.

## Native and earlier core checks

The isolated native probe used a test copy of Instant Surgery and rig buffers. It exercised all nine regional combinations, transaction/physics acknowledgement, repeat-upload stability, modifier conflicts, 36 weighted regional/Height extremes and eight weighted VENUS/FLAT cases without Hex's Hentai Preset. Original dependency inputs were unchanged. All eleven previous pose files matched exactly. See [native results](../build-data/native-visibility-20261117.json).

The earlier core session on 17 November sampled all nine combinations on Elowen's default robe, independent Weight/Height extremes, a partial blend with axis preservation, automatic save/application, legacy-choice recovery, Minimise, pins and camera input blocking/passing. That session used runtime `e11bc8df1fe41ea7722f4c55ec51a8e287abc0eddf1190709bf4b6c8ed8784b3`; it is retained as earlier evidence rather than relabelled as a fresh test of this binary. Wider all-witch and moving-cloth fit checks were player-reported. See [earlier core results](../build-data/live-core-20261117.json).

## Package and dependency boundary

The core layout was freshly imported and mounted in DMM 5.0.1: all 80 files matched that ZIP, with seven appearance routes and forty own garment routes. This binary/config was updated in the same staged and deployed entry. The final rebuilt ZIP has not received another fresh import; packaging checks enforce the accepted one-manifest layout and unchanged asset hashes.

Seven appearance patches and forty game-derived default-garment outputs are included. Downloaded bodies, presets, external ASIs and outfit files remain external. No Dancer palette, scanner, setup helper, resource reference or private derivative belongs in the player, source or SDK archive. Weight needs no outfit dependency.

The SDK retains its original tables and adds an optional Outfit visibility table. Tests cover independent clothing/underwear flags, effective values, complete prefab states, ownership, transaction cleanup, exception restoration and failed saves. The four SDK example targets compile; their placeholder resources still need the adopting outfit's rendered checks.

The player report is `VERIFICATION.txt`; external release metadata includes `VERIFICATION.json`, `CONTENTS.json` and `SHA256SUMS.txt`. The `.txt` extension prevents DMM from treating the player report as another patch.
