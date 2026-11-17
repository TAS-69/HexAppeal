# VENUS Human Body compatibility

Development build **0.5.7** recognizes both **VENUS 2.6 Human Body** and **Elf Body**. Enable one variant in DMM. Public release **1.0.0** remains unchanged; its runtime treats the Human file as an unverified readable body rather than a hard block, while its older build/preflight tools require Elf Body.

## What matches

Both original downloads contain 13,740 primary vertices and 25,158 faces. Their body and hand vertex records, skin weights, palette and stored detail-level layouts match. Their physics files are byte-identical. Only the head geometry differs; the Elf option also supplies a separate cloak shrink-rule patch.

The refitter deliberately ignores target head displacement. Both variants therefore supply identical refit control data. The existing forty default-garment outputs and seven appearance patches remain unchanged. No new body or outfit asset is generated or bundled.

The development adapter recognizes both original hashes and names the detected variant. Manual paths accept either. The source/preflight tools allow exactly one selected Human or Elf variant and validate it against the shared refit baseline. Other readable versions retain the existing soft-warning policy; an unknown body's static refit compatibility still needs a geometry review.

The game prefab reads `phw_00_nude_00_0001_damian.pac`. Earlier guards also required a `cd_phw_...` alias, which VENUS itself does not install. Development 0.5.7 requires the actual primary route and checks the alias only when present. A clean Human or Elf installation no longer needs an unrelated mod to supply that filename. Release 1.0.0 retains the earlier gate.

## Checks

- Original Human/Elf resources compared without modifying either download.
- Human Body served through both aliases in isolated DMM-manifest fixtures; automatic and manual detection passed.
- Five variant/preflight regression cases passed, alongside the existing helper suite.
- Actual Instant Surgery native checks passed all nine combinations, zero VENUS identity error and zero HEX/HEX equivalence error, plus independent proportions.
- 121 CPU skinning comparisons across eleven native poses and all populated stored detail levels passed. Body/hand positions were identical between Human and Elf; results were finite.
- All 21 runtime checks passed on the corrected development build with Human Body mounted, including primary-only and optional-alias fixtures. The separate preferences test also completed in that run.

On 17 November 2026, Human Body was mounted without the Elf cloak patch and checked live on Elowen in her cabin. All nine exact top/bottom combinations applied visibly on the original external Dancer outfit. The dress followed the shape changes, and no new transparent areas or pale blotches were visible in the close front/oblique samples. Default → Dancer switching worked in both directions. Her refitted default robe also followed FLAT/FLAT and HEX/HEX, including minimum Height (-10%) with maximum Weight (+100%) selected independently.

These are close idle-view checks on Elowen, not acceptance of every witch in motion, every camera angle, every detail level or every outfit. The existing minor clipping and intermittent-material limitations still apply. The live test used runtime `9767b1c04624d9225e03b0db74a702a13936eb382b582283bef5e222674ee6a1`. The final development rebuild changes the remaining Settings label from “VENUS Elf Body” to “VENUS Body”; the detected variant remains available in its tooltip. That label-only rebuild passed all 21 offline runtime checks but has not had another game session.

After testing, Elf Body, the ear-mod selection and the exact saved Hex Appeal settings were restored. No game save was restored or edited. Public release 1.0.0 and its archives were left unchanged.

## Installation conflict

**NaiK Elf Ears V3 — Original Damiane Body** replaces a complete body. It can win the served route even while VENUS is enabled. Selecting Human also invalidates a file-priority choice tied specifically to the Elf variant. Check the active file winner after switching, or disable the competing original-body replacement for a Human setup. Merely enabling VENUS is insufficient.

Both variants remain separately installed dependencies. Their PAC/HKX files and the Elf cloak patch are not copied into Hex Appeal. Geometry comparison does not change the licence or permission scope described in [dependency use and credits](../DEPENDENCIES-AND-REFERENCES.md).
