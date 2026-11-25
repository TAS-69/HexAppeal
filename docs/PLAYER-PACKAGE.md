# One player download

The player download has two alternative body folders and one shared runtime. DMM shows a Hex Appeal group on Mods and its normal ASI switch. Enable **PICK ONE: [01] VENUS** or **PICK ONE: [02] Vanilla**, not both, before mounting. Since 1.0.4 both choices sit in one DMM group with one shared ASI.

```text
mod.json
Hex Appeal/
  HexAppeal.asi
  HexAppeal.ini
  HEX-APPEAL-PACKAGE.txt
  PICK ONE - [01] VENUS/
    mod.json
    ha_wardrobe.field.json
    0009/character/...
  PICK ONE - [02] Vanilla/
    mod.json
    ha_wardrobe.field.json
    0009/character/...
  LICENSE
  NOTICE-...
  docs--...
  VERIFICATION.txt
```

Extract the 1.1.4 Release Bundle first. Its inner player ZIP and optional character ZIPs in `packs/` are the files DMM imports.

Shared guides, artwork and notices are flat files so DMM does not mistake documentation folders for extra mod choices. Links in the packaged Markdown are repaired accordingly. Source and SDK archives retain their normal directory structures.

VENUS has seven appearance patches and forty default garment refits. Vanilla preserves the original witch bodies, scale, head/customization, clothing and underwear; its stock body/underwear resources use private paths. Shared boots are isolated in both options. No downloaded body, preset, outfit, third-party ASI or private experimental derivative is bundled.

Packaging builds both choices from external original game inputs, verifies all resource hashes against the live-tested set, requires the public runtime's startup record and runs content allowlists. See the [1.1.4](VERIFICATION-1.1.4.md) and [1.1.3](VERIFICATION-1.1.3.md) verification records and the earlier [1.0.1 record](VERIFICATION-1.0.1.md) for acceptance and limits. Dancer remains excluded pending permission; Weight has no outfit dependency.
