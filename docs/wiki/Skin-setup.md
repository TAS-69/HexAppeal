# Witch skin setup

## Match Damiane’s VENUS skin

With **[01] VENUS** selected for the witches, open **F9 → Settings → Skin tone**. Tick **Match Damiane’s VENUS skin**, then restart Crimson Desert. The setting starts off and requires VENUS Human or Elf Body for Damiane and an active custom skin texture or skin tone export. Untick it and restart to restore the mounted appearance.

Hex Appeal checks the face, torso, collar and hands. When a witch uses a different colour texture for a skin region, Hex Appeal points that material at Damiane’s texture using a private prefab. This preserves the mounted geometry, normal maps, skeleton and outfit. Any makeup baked into a replacement colour comes with it. The **Log** tab reports when a region cannot be checked.

The skin option was tested with DMM Runtime 1.3.1. It uses the current DMM mount, so changing the option requires only a game restart.

The shared VENUS body already uses mounted replacements for the torso, collar and hands. If a skin pack recolours the torso consistently and leaves another region on a stock texture, Hex Appeal can use that colour change to prepare a private texture for the missing region. This works with supported block-compressed DDS textures and gives priority to explicit replacements. Private textures are generated on the player’s PC.

A mounted Skin Texture Maker customization export can also supply brightness and skin colour values. Hex Appeal applies those values through each witch’s own customization so the face and body can receive the same adjustment. Changes stored only in a save or made live at the barber need an equivalent mounted export.

## Local setup tools

The Source download includes tools for brightness, body and hand texture isolation, and Damiane-only underwear removers. The tools read DMM’s mounted file list and report appearance routes they cannot use.

| Mode | Use it when |
| --- | --- |
| `brightness` | Skin Texture Maker exported a character customization file or you want to set brightness directly. Works with both body options. |
| `stock-textures` | A mounted player skin changes the witches too. This isolates stock game body and hand colour for VENUS witches. |
| `custom-textures` | You want VENUS witches to use the locally installed body or hand DDS. A custom albedo can still need colour correction at the neck. |
| `body-only` | A Damiane-only underwear remover also removes the witch underwear. This gives the witches a separate prefab that still points at the installed VENUS model. |

## Setup

1. Mount your normal Hex Appeal setup in DMM and close Crimson Desert.
2. Run the tool from the Hex Appeal source folder. Supply an original game export folder with `--game-assets` or a local archive reader setup with `--archive-toolchain`. An archive setup contains `cdumm/paz_parse.py` and `crimsonforge/core` and can read the originals directly from your game install.
3. Import the generated ZIP into DMM. Put a **Witch Body Isolation** or **Witch Skin Isolation** entry immediately above **Hex Appeal [01] VENUS**. Use one isolation entry. Mount, launch, and check a witch and Damiane in the same lighting.

### Examples

Read the active Damiane brightness export automatically. Supply a full DMM manifest path with `--manifest` if the game uses a different one.

~~~powershell
python tools/setup_witch_skin.py brightness --game-assets "C:\Game exports" --output "C:\Mod work\Witch Brightness (local).zip"
~~~

Add `--preset "C:\Exports\cd_phw_damian_customization.paccd_xml"` to choose an export manually, or add `--brightness 65` to enter the value yourself. For brightness, export the witches’ original `character/descriptors/customization/…_customization.paccd_xml` files when an active override is unavailable.

Use the original material and a custom local albedo. Add `--witch Elowen` to test one witch first.

~~~powershell
python tools/setup_witch_skin.py custom-textures --archive-toolchain "C:\Archive tools" --body-texture "C:\Exports\body.dds" --output "C:\Mod work\Witch Skin (local).zip"
~~~

Use `--hand-texture` for a matching hand DDS. Without an active or manually selected hand texture, setup uses the original game hand texture. Keep generated ZIPs local. Regenerate a texture setup after updating VENUS or its textures. To revert, disable the generated entry and mount again.

The new brightness and custom-colour routes passed local file checks. A rendered comparison is still needed for a specific lighting or texture setup.
