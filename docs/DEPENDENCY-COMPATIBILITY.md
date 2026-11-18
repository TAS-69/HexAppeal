# Dependency compatibility

The dependency checks separate the installed version from the checks needed to use each feature. A new hash produces an **Unverified** warning. Readable files and a passing native interface remain usable. Failed requirements produce a **Blocked** state with the specific reason. Version 1.0.1 also checks the selected witch body independently of a VENUS installation used for Damiane.

Open **F9 → Settings → Dependencies & paths**. File rows show which downloads were found and verified. **Feature checks** show whether the active integration is ready. Hover a blocked row or its error icon for the cause.

| Result | What happens |
| --- | --- |
| Known file, checks pass | Feature is ready |
| Unverified file, checks pass | Feature remains usable with the version warning |
| File missing, unreadable or wrong basic format | Affected feature is blocked |
| Native function, export, metadata or ownership check fails | Affected operation is blocked with its actual interface error |
| HEX contains an unsupported setting | HEX is held; VENUS/FLAT and independent controls remain available when their own requirements pass |
| A required plugin is installed but has not loaded | Startup waits, then explains that the ASI must be enabled and the game restarted |

Shape choices remain saved while their requirements are unavailable. Fix a preset/path and use **Detect again**, or wait for the next scan. Compatible saved choices resume through the existing application queue. Updating a loaded ASI requires a full game restart.

## Independent Weight

Weight uses the native shaping adapter and needs no outfit dependency. Dancer detection and setup are excluded from this release. Generic Wardrobe providers retain their own dependencies and availability reasons. Legacy outfit path fields are preserved only as inert preferences. See [Weight controls](WEIGHT-CONTROLS.md).

## What the adapter checks

- The selected mounted witch appearance/body routes must exist and be readable. VENUS is required only for its body choice; Vanilla validates its own original resources. Unverified readable bodies retain a warning, including the limits of their untested visual fit.
- Instant Surgery's private shaping adapter checks the expected function signatures and metadata when its file is unverified. It does not guess relocated private structures or call arbitrary matching bytes.
- Versioned public exports, including `InstantSurgeryDeformPoseV1` and `TwoReasonsEditorPoseAppliedV1`, resolve by name. Their executable address must belong to the loaded plugin. Moving a valid export within that plugin no longer fails a fixed-address comparison.
- Appearance hooks use bounded chain and executable ownership checks. Actor identity, native outfit records and final assembly acknowledgement still govern writes.
- External HEX presets accept a UTF-8 BOM, comments and surrounding whitespace. Unknown shape fields, invalid numbers, missing markers and unreadable/oversized files return parsing errors.
- Base underwear, Wardrobe and Weight report separate checks. Uninitialized features explain a blocked shaping adapter; an independent feature error keeps its own reason.

The native adapter still relies on private interfaces. A future release that changes their layout needs an adapter update. Passing a basic format or native check cannot prove every animation, material, garment fit or future dependency release. The warning remains until that upstream version receives its own verification.

## Verification

The offline suite covers unknown-version controls, actual interface blocks, recovery, feature isolation, settings retention, atomic status snapshots and named executable exports. Private native comparisons exercise inspected dependencies on isolated copies and rig buffers, including compatible hash/format changes and deliberately incompatible inputs. These fixtures do not certify an unreleased upstream version.

The **0.5.8** build passed **23/23** offline runtime checks and **34** helper tests. An isolated native comparison accepted an unverified engine hash and an unverified BOM/comment/whitespace-formatted preset, producing identical outputs for all nine combinations. Three changed function interfaces and two incompatible preset cases were rejected. Original dependency files were preserved.

On 17 November 2026, a live Elowen check showed all six feature rows ready while Character Creator retained its unverified-version warning. A temporary external preset with an unsupported field blocked HEX and displayed that field in the tooltip; the other feature rows stayed ready. Clearing the override restored automatic detection, HEX readiness and the saved shape. The saved Dancer / Arcane Pink selection remained active and unchanged. This was one close idle view, not exhaustive motion or all-witch coverage.

The tested runtime was `25640380fe36d4e511a3924c5f62a32bd3ceecabe17f5e85002db267b0114a6a`. The verified **0.5.7** restore point remains preserved. The refreshed **1.0.0** includes these changes; its exact rebuilt binary has a separate [release verification summary](VERIFICATION-1.0.0.md).
