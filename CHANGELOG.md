# Changes

## 1.0.2

### Player download

- Fixed DMM import grouping: VENUS and Vanilla stay inside one Hex Appeal group.
- Set the package, body options and ASI version to 1.0.2. The plugin now has Windows file/product version metadata too.
- Clarified clean installation after removing the archived package, while preserving saved settings.
- Retained the tested body resources, shaping and Wardrobe functionality from 1.0.1.

### Wardrobe SDK download

- Updated package verification and installation guidance. The public API and examples are unchanged.

### Source download

- Corrected the archive builder, added manifest-layout assertions and recorded actual DMM import/mount results.
- Added plugin version resources and an offline check for the displayed version.
- Updated beginner installation and upgrade documentation.

## 1.0.1

### Player download

- Corrected the archive import layout so DMM keeps VENUS and Vanilla inside one Hex Appeal group. Runtime and game resources are unchanged by this packaging correction.
- Clarified the 1.0.0 upgrade cleanup: keep saved settings, remove obsolete package/plugin copies, then select one current body option and mount.
- Added [01] VENUS and [02] Vanilla in one download, sharing the F9 runtime. Enable one body choice.
- Vanilla midpoint preserves each witch's original body, scale and customization. VENUS can remain installed separately for Damiane.
- Isolated witch boots from Damiane Armor Refit Pack; isolated original Vanilla bodies and underwear from shared body/armour replacements.
- Added complete body prefab lookup and definition resources, fixing the world-load crash and invisible witches found during private testing.
- Expanded preset discovery through DMM's configured storage, active source paths, portable locations and Instant Surgery folders. Nested/renamed presets and manual file/folder paths are supported.
- Kept baseline/FLAT, independent Height/Weight and Wardrobe available when their requirements pass, even without the external Hex preset. Vanilla does not require VENUS.
- Corrected proportion-slider padding and full track backgrounds, visibility labels, centred witch names and warning glyphs.

### Wardrobe SDK download

- Added optional OutfitPolicy v1 and HA_REPLACE_BASE_UNDERWEAR. The original ABI tables remain unchanged.
- Added replace_base_underwear definitions, capability checks and rollback in the generator/helper.
- An owned custom outfit can suppress natural base underwear without changing the saved user preference. Hide outfit can keep custom underwear; Hide base bra & panties hides its linked controls.
- Expanded ownership, restoration and unsupported-runtime tests, beginner guidance and complete implementation examples. Custom assets still need their own rendered checks.

### Source download

- Corrected the player ZIP builder and added checks for the main manifest beside its single folder. Documented actual DMM import and upgrade results.
- Included the complete original runtime, SDK, tools and source tests for these changes.
- Added original-game resource builders/validators and dual-choice packaging with tested-resource allowlists.
- Updated body-choice, missing-dependency, SDK, installation and verification documentation. Downloaded dependencies, private research/permissions, compiled plugins and game model outputs remain excluded from source.
