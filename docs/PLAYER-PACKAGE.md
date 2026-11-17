# One player download

The version 1.0.0 package combines the runtime, seven body-routing patches and forty game-derived default outfit refits under one Hex Appeal folder and manifest. DMM uses one Mods entry plus its normal ASI switch. Both controls are part of installation and removal.

The combined layout was imported and mounted in DMM 5.0.1 on 17 November 2026. The seven appearance routes, forty garment routes and included ASI were checked. This packaging layout acceptance is retained for the core-only rebuild; the exact new binary receives separate offline checks. It has not had a fresh core-only game session.

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
  VERIFICATION.json
```

No Dancer setup launcher, dependency helper, palette implementation, source scanner, models, properties, textures or local file references belong in this release. Its old implementation is preserved privately for a possible separate patch after permission. Weight remains part of native shaping and has no outfit dependency.

The general Wardrobe SDK remains available, including providers, variants, toggles, complete matrices, visibility parts, Base underwear linkage and explicit refresh requests. SDK/source archives are optional creator downloads.

Packaging uses the clean committed Git checkout. It rejects dependency hashes, excluded integration filenames, unexpected binaries, duplicate/traversing paths and any model outside the forty approved default outputs. The runtime is checked for the removed integration's resource identifiers. Private permission messages, research inputs, saves and test executables remain excluded.

**Version 1 remains 1.0.0 until the Nexus release.** Each explicitly requested preparation replaces the dated private release with backups and matching checksums. `DEV_VERSION` remains independent. See [Versioning](wiki/Versioning.md).
