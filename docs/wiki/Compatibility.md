# Compatibility and known issues

## Checked references

Last reviewed **17 November 2026**. These are locally checked versions, rather than a claim to cover the newest download on every mod page.

| Component | Checked version | Scope |
| --- | --- | --- |
| Crimson Desert | **2.05.00 / Steam 26636972** | Native functions, actor ownership, shaping and sampled outfit operations |
| Desert Mod Manager | **5.0.1** | Fresh core package import, mounting and all 47 file routes |
| VENUS | **2.6 Human / Elf Body** | Body format and matching skinning; Elf core session and earlier Human comparison |
| Instant Surgery | **0.9.6** | Checked native shaping interface and live coordinated deformation |
| Hex's Hentai Preset | **1.3**, with **1.2** retained | External preset parsing and regional shaping; 1.3 is preferred |
| Two Reasons Body Physics | **1.0.7** | Optional pose interface and coordinated update acknowledgement |
| Character Creator | **9.1.41** | Optional hook coexistence, startup, shaping, default Wardrobe and base visibility |
| Spouse Overhaul | **0.5.0** | Optional hook coexistence and custom outfit ownership boundary |

These are checked versions, not a promise that every past or future release works. Unknown readable versions receive a soft warning and remain usable. Native layouts, function signatures, ownership and transaction checks must still pass. The static default-outfit refits were built against VENUS 2.6 and need a new fit review if its geometry changes.

The **1.0.0** preparation includes separate **Feature checks** under Dependencies & paths, specific errors for actual interface failures, named-export validation independent of an export's address, and tolerant preset formatting. See [dependency compatibility](../DEPENDENCY-COMPATIBILITY.md) for recovery and the remaining private-interface limits.

The runtime also warns when an unverified Spouse Overhaul or Character Creator is loaded. Its appearance-hook adapter checks bounded jump chains, executable destinations and recognized module ownership separately. An unreadable, cyclic or unrecognized hook, changed native interface or unowned outfit can still stop an operation. A version warning alone does not disable Wardrobe. Character Creator 9.1.41 is now recognised after the scoped core session and hook checks. This does not certify every feature of Character Creator or Spouse Overhaul.

Both Human Body and Elf Body are recognized. Their body/hand skinning and physics match, so the existing default refits use the same baseline. Live Human checks on Elowen covered all nine combinations on Dancer, Default ↔ Dancer switching and independent minimum Height / maximum Weight on the default robe. See [VENUS compatibility](../VENUS-COMPATIBILITY.md) for the tested build and limits. A DMM priority rule tied to the Elf variant may need to be chosen again after switching to Human.

## Conflicts

| Overlap | Action |
| --- | --- |
| Another complete body replacement, including NaiK's Original Damiane Body | Let the selected VENUS Human/Elf variant win the body route |
| VENUS Armor Refit on boot routes 0160 / 0161 / 0163 | Choose the intended winner in DMM |
| Spouse Overhaul / barber custom outfits | Wardrobe avoids custom outfit ownership; return to a recognized default before replacing it |
| Another modifier writing the same pose buffers | A checked transaction may reject the shape rather than overwrite foreign work |
| Another F9 binding | Change Hex Appeal's key in Settings |

## Known limits

- Extreme proportions may clip. Coordinated pose shaping is not arbitrary mesh regeneration, cloth/collider authoring or a universal outfit-fit guarantee.
- Default garment refits use the VENUS 2.6 baseline; changed geometry needs a fit review.
- Controller input, every detail level, long sessions and every witch in motion have not received exhaustive live acceptance.
- Base underwear can briefly reappear while an outfit rebuild settles, before its separate visibility policy reapplies.
- The combined base bra/panties part is not exposed as independent controls. Providers may supply separate garment parts.
- The 500 ms refresh setting passed game-robe assembly tests; individual outfit textures may need a longer interval.
- Earlier private outfit material/visibility problems did not establish a universal cause. That outfit integration is not part of this release.
- Dancer dependencies, setup and colour controls are excluded pending permission. Generic SDK providers remain supported and need rendered checks with their own assets.

## What was checked

The current release binary passed **21 offline checks**, **33 SDK cases**, **23 helper tests** and **44 isolated native shaping cases**. All eleven reference poses matched exactly. The unchanged body/default-garment set retains its 47 checks. See [release verification](../VERIFICATION-1.0.0.md) for hashes and test boundaries.

Current live Elowen checks covered all four independent clothing/Base underwear combinations, repeated removal/restoration, exposed underwear restoration, the Hide outfit quick pin, unsupported-provider retention and automatic saved visibility after a full restart. Three public SDK refresh calls passed at 500 ms configured delay, with 625 ms observed waits. The temporary helper was then removed. External texture-cache behaviour was not tested by those refresh calls.

The earlier core layout was freshly imported/mounted, and its earlier runtime sampled all nine combinations, independent Weight/Height, partial blends, automatic saves, Minimise, pins and camera input. The current runtime was updated in that same entry; the final ZIP was not freshly imported again. Earlier Lyselia/Elowen sessions and player-reported all-witch/moving-cloth checks retain their original scope. Adopting outfit assets need their own rendered acceptance.

## Troubleshooting

**F9 does nothing:** confirm the ASI loader and only one HexAppeal.asi, then check the log and key binding. A plugin update requires a full game restart.

**Saved / waiting:** bring the selected witch into view and inspect Dependencies & paths. Downloaded files and active mounted files are different checks.

**Outfit stays default:** confirm its resources are mounted and the actor is wearing a recognized default, rather than custom spouse/barber equipment. Let the world and assembly finish loading.

**Outfit unavailable:** hover the status for the initialization error. In the current adapter, an unverified appearance-mod version only warns; a changed or unrecognized native hook can still prevent switching. Saved choices are retained.

**Provider colour or materials look wrong:** check resource conflicts, test a fresh save load and full restart, and report whether it happens with original resources and neutral shaping. Include the selected colour/shape, witch, game/dependency versions and a screenshot. Do not assume every material issue comes from shaping.

**Crash:** close the game and disable Hex Appeal through DMM for a comparison. Keep a copy of the relevant logs before retrying. A log being active at the crash is timing evidence, not proof of its cause.
