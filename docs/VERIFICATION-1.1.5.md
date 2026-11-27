# Verification: 1.1.5

**Released 27 November 2026.** 1.1.5 supports Crimson Desert 2.06.00 and keeps the 2.05.01 layouts. Shape, Height, Weight, HEX, Wardrobe, Hide outfit, base underwear and the native **[HA] Wardrobe** work on the updated game. Sylvia keeps Shape when another mod renames her body, and body warnings show only on the witch they apply to. The rendering, frame generation and NVIDIA presentation code is unchanged from 1.1.4. Body resources, outfit refits, the character packs and the Wardrobe SDK are unchanged from 1.1.4.

| File | SHA-256 |
| --- | --- |
| `HexAppeal.asi` | `38ae21d5875d3affcf33919df55dc729ebc675928b97d7f4afe81f57254d2b2b` |
| `HexAppeal-1.1.5.zip` | `31afb1ca0bd8e166d5383becef4bb2baa752dca0269e9fdcf940a905c126ba89` |

The character packs and `HexAppeal-Wardrobe-SDK-1.1.3.zip` are the 1.1.3 files. Their checksums are in the [1.1.3 record](VERIFICATION-1.1.3.md).

## Checked references

- **Game:** Crimson Desert 2.06.00, Steam build 26949954. The executable's stamp is `6B07E5E9` and its SHA-256 is `ea2d84d947521332b002359818960ffd97765f37b447503d8a1efad4040269c8`.
- **Mod manager:** DMM 5.3.2 with DMM Runtime 1.4.0.
- **Dependencies:** Venus Body for Damiane 2.6 (Elf Body), Instant Surgery 0.9.6 and Hex's Hentai Preset 1.3.

## What changed for 2.06

- **Game addresses.** The 2.06 lookups switch on only for the 2.06 executable (stamp, size and checksum). Each 2.06 signature must land on its checked address. All 43 game addresses were found in 0.5 s. No other game build changes behaviour.
- **Records and tables.** Outfit records use 2.06's 0x78-byte size, packed to the game's size for each call. The new character and item tables are read for add-on identities and for Wardrobe names and icons.
- **Shaping.** Instant Surgery 0.9.6 no longer receives uploads on 2.06. Hex Appeal hooks the game's checked animation upload directly, gated on 2.06 and on the function's bytes, and still uses Instant Surgery for the shaping engine.
- **Native Wardrobe.** It selects the game's own Interact row and lets the game commit it, which keeps the Interact camera, depth of field and character hold. Cancel uses the game's own close handler. Every new hook site was checked against the 2.06 executable on disk.
- **Sylvia and renamed bodies.** On 2.06, Sylvia's stock appearance uses Damiane's body. Legion's Slider "No Underwear - NPCs" renames it to `phw_00_nude_00_0001_damian_IAL`. Hex Appeal now compares prefab names and paths without regard to letter case and accepts that copy. A copy that draws the checked Venus model with the same rig adds no warning.
- **Warnings.** Each witch's body and appearance warnings are kept per witch. The global warning covers only issues that affect everyone, such as an unverified Venus body. Add-on characters show only global warnings.

## Offline checks

All **33 offline runtime checks** passed on the exact binary, including Windows FileVersion and ProductVersion 1.1.5, and **58 helper tests** passed. The checks include new tests for mixed-case body copies, Venus-model copies that add no warning, and warnings scoped to one witch. The player ZIP holds 140 files with both body choices. Its game files are identical to 1.1.4.

## Live checks

These ran on the exact 1.1.5 binary, on the full PD loadout: 15 ASIs, including Two Reasons Body Physics, Spouse Overhaul, Character Creator, Pywel Wardrobe, DesertLink, Live Transmog and Heroine of Pywel. The test PC is an AMD RX 7900 XTX with HDR and FSR Frame Generation.

- **Startup and features.** All features showed Ready: Shape, HEX preset, Weight, Wardrobe and base underwear. The F9 overlay drew through the swapchain wrapper, because DesertLink owned Present.
- **Warnings.** No warning marks showed on the Shape pages checked: Elowen, Sylvia, ALL CHARACTERS, and the Alida and Rucchia add-ons. Wardrobe and Settings showed none either. Sylvia's body is eligible.
- **Witches.** Saved Shape and outfits reapplied, and base underwear was hidden.
- **Native [HA] Wardrobe on Lyselia.** These ran on the preceding build (`d687666e`), whose Wardrobe code is identical. The camera and depth of field engaged, the HUD hid, a preview applied, and Cancel rolled it back and closed the native group. The regular Interact wheel afterwards showed its gold selector line again.
- **Character packs.** All six packs passed validation and showed in F9. Spawned NPCs:
  - Alida, Nevra Yailar and Selvia were recognised by Wardrobe, and Alida received the VENUS body.
  - Rucchia, her Stranger record, Maegu and Woosa had their pose buffers validated.
  - Maegu and Woosa had their base underwear hidden, and Maegu's saved outfit was applied and acknowledged.

## Limits

- **Naira.** DesertLink cannot spawn Naira's ally records (1004236, 412) on 2.06. Spawning her 444 record crashed the game even with Hex Appeal removed, so Naira was checked against the 2.06 game data only. Her records, names, appearance and default parts all exist.
- **Two Reasons Body Physics.** The handoff is unavailable on 2.06 until Instant Surgery and Two Reasons Body Physics update.
- **Not retested here.** The [02] Vanilla option was not live-tested on 2.06, and no NVIDIA RTX hardware was available.
- **Earlier fitting results** keep their own scope in the [1.0.2](VERIFICATION-1.0.2.md), [1.0.1](VERIFICATION-1.0.1.md) and [1.0.0](VERIFICATION-1.0.0.md) records.
