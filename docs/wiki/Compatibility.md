# Compatibility and known issues

## Checked references

The reference setup is Crimson Desert **2.05.00 / Steam build 26636972**, DMM **5.0.1**, VENUS Human/Elf Body **2.6**, Instant Surgery **0.9.6**, Hex **1.3 / 1.2**, optional Two Reasons Body Physics **1.0.7**, and optional Sexy Witches FOMOD **1.1**.

These are checked versions, not a promise that every past or future release works. Unknown readable versions receive a soft warning and remain usable. Native layouts, function signatures, ownership and transaction checks must still pass. The static default-outfit refits were built against VENUS 2.6 and need a new fit review if its geometry changes.

The refreshed **1.0.0**, based on development **0.5.8**, includes separate **Feature checks** under Dependencies & paths, specific errors for actual interface failures, named-export validation independent of an export's address, and tolerant preset formatting. See [dependency compatibility](../DEPENDENCY-COMPATIBILITY.md) for recovery and the remaining private-interface limits. Earlier development checkpoints and the initial split release files remain backed up.

The runtime also warns when an unverified Spouse Overhaul or Character Creator is loaded. Its appearance-hook adapter checks bounded jump chains, executable destinations and recognized module ownership separately. An unreadable, cyclic or unrecognized hook, changed native interface or unowned outfit can still stop an operation. A version warning alone does not disable Wardrobe. With Character Creator 9.1.41 loaded, the corrected adapter passed a sampled Elowen check for saved Dancer restoration and automatic Black → White → Black refresh. That does not verify every interaction in that Character Creator version.

Both Human Body and Elf Body are recognized. Their body/hand skinning and physics match, so the existing default refits use the same baseline. Live Human checks on Elowen covered all nine combinations on Dancer, Default ↔ Dancer switching and independent minimum Height / maximum Weight on the default robe. See [VENUS compatibility](../VENUS-COMPATIBILITY.md) for the tested build and limits. A DMM priority rule tied to the Elf variant may need to be chosen again after switching to Human.

## Conflicts

| Overlap | Action |
| --- | --- |
| Another complete body replacement, including NaiK's Original Damiane Body | Let the selected VENUS Human/Elf variant win the body route |
| VENUS Armor Refit on boot routes 0160 / 0161 / 0163 | Choose the intended winner in DMM |
| Another outfit using Dancer's shared 0191 resources or texture | Review the file winner; Dancer colours affect the shared texture |
| Spouse Overhaul / barber custom outfits | Wardrobe avoids custom outfit ownership; return to a recognized default before replacing it |
| Another modifier writing the same pose buffers | A checked transaction may reject the shape rather than overwrite foreign work |
| Another F9 binding | Change Hex Appeal's key in Settings |

## Known limits

- Minor hand/skirt clipping and extreme HEX/HEX or high-weight shoulder clipping remain.
- Dancer colours are global. Embedded Dancer bra/top removal is not supplied.
- Earlier Lyselia robe disappearance occurred with Spouse Overhaul/barber outfit changes. Its cause was not isolated.
- Pale Dancer blotches were intermittent. They were reproduced during comparisons with shaping disabled and original meshes; that did not establish a cause. The last clean original-resource sample showed none. A permanent fix is not claimed.
- Private split-mesh Dancer experiments produced transparent skin/material areas and were rejected. No split/refit Dancer derivative pack is distributed.
- Coordinated skeletal deformation is not arbitrary mesh regeneration, cloth/collider authoring or a universal garment-fit guarantee.
- Controller input, every detail level, long sessions, every witch in motion and third-party authored prefab matrices have not received exhaustive live acceptance.

## What was checked

The release binary is rebuilt from the tested development lineage. Offline checks cover shape composition, saves, input/menu lifecycle, dependency routes, Wardrobe ownership/restoration, palettes, SDK definitions, native matrices, Base underwear policy and ABI handling. The package audit matches that rebuilt binary. See the release's verification summary for its hash and results.

Live evidence from the preceding clean build covers Elowen startup restoration, all nine regional shapes on the original Dancer resources, automatic White → Black changes and Default → Dancer restoration in one close idle view. Earlier direct tests covered Lyselia/Elowen shaping; broader all-witch default fit and moving cloth were player-reported. A separate earlier provider plugin sampled native accessory visibility and Base underwear linkage on Elowen. It did not remove Dancer's embedded top. The release's version/configuration packaging changes have not had a fresh live game session.

## Troubleshooting

**F9 does nothing:** confirm the ASI loader and only one HexAppeal.asi, then check the log and key binding. A plugin update requires a full game restart.

**Saved / waiting:** bring the selected witch into view and inspect Dependencies & paths. Downloaded files and active mounted files are different checks.

**Outfit stays default:** confirm its resources are mounted and the actor is wearing a recognized default, rather than custom spouse/barber equipment. Let the world and assembly finish loading.

**Outfit unavailable:** hover the status for the initialization error. In the current adapter, an unverified appearance-mod version only warns; a changed or unrecognized native hook can still prevent switching. Saved choices are retained.

**Colour or materials look wrong:** check resource conflicts, test a fresh save load and full restart, and report whether it happens with original resources and neutral shaping. Include the selected colour/shape, witch, game/dependency versions and a screenshot. Do not assume every material issue comes from shaping.

**Crash:** close the game and disable Hex Appeal through DMM for a comparison. Keep a copy of the relevant logs before retrying. A log being active at the crash is timing evidence, not proof of its cause.
