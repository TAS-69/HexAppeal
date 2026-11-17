# One player download

The version 1.0.0 package combines the runtime, seven body-routing patches and forty game-derived default outfit refits under one Hex Appeal folder and manifest. DMM uses one Mods entry plus its normal ASI switch. Both controls are part of installation and removal.

The core layout was freshly imported and mounted in DMM 5.0.1 on 17 November 2026. All 80 files matched that earlier ZIP, with seven appearance routes and forty own garment routes. The current runtime/config was updated in the same entry and passed Elowen visibility, startup and SDK refresh checks. The final rebuilt ZIP has not been imported again; its accepted layout and asset hashes are checked during packaging. See [Verification](VERIFICATION-1.0.0.md) for the tested scope.

## Contents

```text
Hex Appeal/
  mod.json
  HexAppeal.asi
  HexAppeal.ini
  files/0009/character/...
  docs/...
  licenses/...
  LICENSE
  DEPENDENCIES-AND-REFERENCES.md
  VERIFICATION.txt
```

No Dancer setup launcher, dependency helper, palette implementation, source scanner, models, properties, textures or local file references belong in this release. Its old implementation is preserved privately for a possible separate patch after permission. Weight remains part of native shaping and has no outfit dependency.

The general Wardrobe SDK remains available, including providers, variants, toggles, complete matrices, visibility parts, independent Hide outfit/Base underwear linkage and explicit refresh requests. SDK/source archives are optional creator downloads.

Packaging uses the clean committed Git checkout. It rejects dependency hashes, excluded integration filenames, unexpected binaries, duplicate/traversing paths and any model outside the forty approved default outputs. The runtime is checked for the removed integration's resource identifiers. Private permission messages, research inputs, saves and test executables remain excluded.

**Version 1 remains 1.0.0 until the Nexus release.** Each explicitly requested preparation replaces the dated private release with backups and matching checksums. `DEV_VERSION` remains independent. See [Versioning](wiki/Versioning.md).
