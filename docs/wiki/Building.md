# Building Hex Appeal

## Runtime

Use Windows x64, **Visual Studio 2022 C++ x64 tools with Windows SDK and assembler**, and **Python 3.12+**. The runtime vendors its openly licensed ImGui/MinHook sources and notices. No downloaded body, preset, outfit or plugin binary is required to compile it.

```powershell
python runtime/tools/build.py
python runtime/tools/verify.py
```

The default build uses `DEV_VERSION` plus `-development`. For an explicitly requested public package:

```powershell
python runtime/tools/build.py --release
python runtime/tools/verify.py --instant "D:/Games/Crimson Desert/bin64/InstantSurgery.asi" --game "D:/Games/Crimson Desert"
python tools/verify_helpers.py
python tools/package_release.py
```

The optional paths enable the checked installed-dependency fixture tests. They read original files and use isolated copies for the native export check. The native export fixture accepts the inspected Instant Surgery 0.9.6 binary only; runtime unknown-version policy remains separate. Tests do not launch or mount the game. Outputs and reports are ignored by Git.

For the Human Body compatibility regression, also pass `--venus-human "path/to/the/original/Human Body/phw_00_nude_00_0001_damian.pac"`. It must remain outside this checkout; the test serves it through an isolated manifest and never changes your real selection.

Packaging requires a **clean, committed Git checkout**, a matching successful full offline report, a public build and the unchanged requested `VERSION`. Clone the repository to package; the Source ZIP is sufficient to compile, but has no Git history. Packaging uses tracked files only, writes the combined player, SDK and source archives, checksums and a content inventory, and does not upload, change the version or alter a game install. See [Versioning](Versioning.md).

If Windows quarantines the preferences test, leave it quarantined. The verifier can record that specific check as blocked with `--skip-quarantined-interface-test`; `package_release.py --draft` can then prepare an explicitly incomplete draft if every other required check passed. It does not mark the package release-ready. Do not disable protection to obtain a passing report.

## Default body routes and garment refits

The repository includes my seven appearance patches and forty game-derived vanilla garment refits, with the source/validation metadata needed to trace them. No VENUS geometry, Hex preset, Dancer assets or extracted vanilla originals are stored as build inputs.

The Source archive omits the compiled plugin and garment PAC outputs. Obtain the matching player archive for those checked outputs, or rebuild them from your external inputs.

Rebuilding the garment assets additionally requires the separately obtained reference VENUS Human/Elf Body, the matching installed game, NumPy/SciPy from `requirements.txt`, and external tools:

- [CrimsonForge](https://github.com/hzeem/CrimsonForge), checked commit `09f821638da4e1f04b26be43af408b4d59337ad3`.
- [CDUMM](https://github.com/faisalkindi/CDUMM), checked commit `4d5a0f8cc6ea841d246509d7b9f8c80954c2fe0d`.

Put external inputs/toolchains outside this checkout. Configure a local, ignored `development-paths.json` with `game`, `researchRoot` and, if needed, `dependencyFiles.venus`, `dependencyFiles.hex`, `dependencyFiles.instant_surgery`. Under that external root the scripts expect `toolchain/crimsonforge`, `toolchain/cdumm`, `toolchain/python-deps` and `sources/`. `HEX_APPEAL_GAME` and `HEX_APPEAL_RESEARCH` can override the first two paths.

`extract_sources.py --game "..."` reads only the known vanilla archive paths and verifies source hashes. `build_bodies.py` creates the appearance patches. `refit.py --output "fresh-output" --report "fresh-report.json"` refuses existing output, fits every stored detail level and preserves skin/UV/index data. `validate.py --game "..."` checks the shipped forty outputs against the retained validation metadata. The shipped baseline used the default `recalculate` shading mode; alternative modes are build options, not verified fixes for intermittent Dancer materials.

These asset rebuild steps are version-specific. Changed hashes need an asset review and fresh live fit checks, not a forced rebuild. Normal players should use the release packages.
