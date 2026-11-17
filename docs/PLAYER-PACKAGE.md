# One player download

The 1.0.0 player package, refreshed on 17 November 2026, combines **Bodies, Runtime and default Outfit Refits**. Players download one ZIP and enable **Hex Appeal** on Mods and its **HexAppeal** switch on ASI Mods, then mount. The development source keeps its separate modules. SDK and source downloads are optional creator resources, not extra installation steps for players.

The combined layout was imported and mounted with **DMM 5.0.1** on 17 November 2026. All seven appearance routes and forty default garment routes matched. DMM keeps data and plugin switches separate; turning off only Mods leaves the ASI enabled. Both normal switches are part of installation and removal. Other managers need their own acceptance check.

The first fresh launch exposed an outdated Character Creator whole-file gate. After correcting it to warn on an unverified version while separately checking executable hook ownership and native interfaces, the combined installation passed a live check on Elowen: saved Dancer restored, and Midnight Black → Original White → Midnight Black refreshed automatically. The old external resource entry stayed disabled. Her saved shape remained active; no new pale blotches or transparency appeared in the close oblique view. This is sampled acceptance, not every witch, motion or detail level. The checked runtime was `de392658026ea59427df2adc0998f51fceb12904f14648caed0bb4f04dbd33c2`.

## Contents

```text
Hex Appeal/
  mod.json
  HexAppeal.asi
  HexAppeal.ini
  Setup Dancer.cmd
  tools/prepare_dancer_dependencies.py
  files/0009/character/...
  docs/...
  licenses/...
  LICENSE
  DEPENDENCIES-AND-REFERENCES.md
  VERIFICATION.json
```

There is one manifest titled **Hex Appeal**, seven appearance-routing patches and forty existing game-derived default garment refits. The only plugin is Hex Appeal. Dependencies, dependency models/textures/presets, private Dancer experiments, saves, permission messages and test executables remain outside the download. Licensed compiled overlay adaptations retain their notices.

The packager checks path safety, case-insensitive duplicates, the complete approved garment set and external dependency hashes, including both checked VENUS variants. It produces one player archive plus separate creator SDK/source archives. `VERSION` is not incremented automatically. The earlier split 1.0.0 files and tag were backed up before this explicitly requested replacement.

## Manager acceptance

With the game closed and the previous development setup backed up:

1. Import the private combined development archive. Confirm its name, version and all seven patches / forty refits are recognized.
2. Confirm DMM recognizes the included ASI and configuration on its ASI Mods page.
3. Disable the previous separate Hex Appeal modules to avoid duplicate patches or plugins.
4. Enable the combined Mods entry and its ASI switch. Mount and verify the intended appearance/model routes and the exact deployed HexAppeal.asi hash.
5. To remove it, disable both controls and remount. DMM's Mods checkbox alone does not disable its plugin.
6. Preserve the saved choices and record the manager version, mounted-route hashes, deployed plugin hash and live result in `build-data/player-package.json`. The current private test installation uses the combined entry; the old separate entries remain disabled.

Separate Mods and ASI controls are accepted. Hex Appeal does not deploy plugins from its runtime or alter the manager's settings to synchronize those controls.

## Dancer before the first mount

1. Keep the complete separately downloaded Sexy Witches FOMOD, with all five colour folders, in DMM's mods storage. It must be on the same drive as Hex Appeal.
2. Import the combined Hex Appeal ZIP. Install Python 3.12+ with its launcher if needed for this optional step.
3. Before mounting, open the installed Hex Appeal folder and run **Setup Dancer.cmd**. It detects the original dependency and prepares four local references. If detection is ambiguous, use `--source` with the helper to select the original FOMOD.
4. Enable Hex Appeal and its ASI switch, review conflicts, then mount once. Start the game normally.

There is no separate Dancer resource entry in this mode. If upgrading from the older setup, disable that old entry before mounting. The setup preserves the main manifest, runtime, default refits and player guide. Existing Dancer files are never overwritten; a partial link failure removes only the links created by that attempt.

The actual main-entry mount passed native checks for Dancer readiness, managed colour switching and all five supplied palette sources on 17 November 2026. The later live check above confirmed startup restoration and automatic White/Black refresh using those references. Original dependency contents remained unchanged.

The original models and textures are never inside the public ZIP. The configured local installation contains links to their complete contents, so do not repackage that installed folder. Keep the original FOMOD available. Updating the main package may require preparing the references again **before** the update's mount. This setup does not run after a first mount or generate assets on game startup.

`package_release.py` requires recorded manager acceptance. `--draft` can prepare an explicitly incomplete public-build package when that check is pending, subject to the other release checks. A private combined development archive is only a packaging fixture and is never uploaded as a public release.
