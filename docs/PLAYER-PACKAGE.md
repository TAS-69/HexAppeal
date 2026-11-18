# One player download

Version 1.0.1 includes two alternative body folders and one shared runtime. DMM shows a Hex Appeal group on Mods and its normal ASI switch. Enable **[01] VENUS** or **[02] Vanilla**, and disable the other body choice before mounting. The checked DMM 5.0.1 build displays independent checkboxes rather than enforcing this choice automatically.

```text
Hex Appeal/
  mod.json
  HexAppeal.asi
  HexAppeal.ini
  [01] VENUS/
    mod.json
    files/0009/character/...
  [02] Vanilla/
    mod.json
    files/0009/character/...
  LICENSE
  NOTICE-...
  docs--...
  VERIFICATION.txt
```

Shared guides, artwork and notices are flat files so DMM does not mistake documentation folders for extra mod choices. Links in the packaged Markdown are repaired accordingly. Source and SDK archives retain their normal directory structures.

VENUS has seven appearance patches and forty default garment refits. Vanilla preserves the original witch bodies, scale, head/customization, clothing and underwear; its stock body/underwear resources use private paths. Shared boots are isolated in both options. No downloaded body, preset, outfit, third-party ASI or private experimental derivative is bundled.

Packaging builds both choices from external original game inputs, verifies all resource hashes against the live-tested set, requires the public runtime's startup record and runs content allowlists. See [verification](VERIFICATION-1.0.1.md) for acceptance and limits. Dancer remains excluded pending permission; Weight has no outfit dependency.
