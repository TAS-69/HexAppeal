# Weight without an outfit dependency

Development **0.5.9** makes Weight available whenever Hex Appeal's native shaping adapter and active body are ready. **Sexy Witches is optional for Dancer Dress and its five supplied colours only.** Removing that outfit, clearing its path or setting an invalid outfit path no longer disables Weight or holds a saved weighted shape.

Weight still adjusts the same native limb/waist parameters and range. Height stays independent. Breast/butt presets, blends and saved values are preserved. This change removes a file-presence gate; it does not replace the existing shaping calculation or bake any upstream body data into Hex Appeal.

The earlier body inspection remains credited as a development reference. No original or modified outfit/body file is distributed. The dependency reader identifies the optional source solely for Dancer and its palette.

The Settings path is now **Dancer source file or mod folder**. Old `WeightReference` keys remain readable as Dancer paths; new saves write `DancerSource`. If both keys exist, the explicit new field wins, including an empty value selecting automatic detection. Profiles need no migration or reset.

Missing VENUS/Instant Surgery, an unmounted body or an actual incompatible native interface still stops shaping. An unavailable Dancer source disables only the associated outfit/colours. Weight cannot repair an arbitrary outfit's skinning, cloth or geometry.

The published **1.0.0** files predate this change. Development numbering is separate; that release is not replaced or advanced automatically.

## Verification

The 0.5.9 development build passed all **23 runtime checks** and **34 helper tests**. Menu fixtures exercise editing and saving Weight with no outfit source, automatic application retains weighted profiles without that source, and path fixtures cover legacy-key migration and precedence.

An isolated native test used the installed Instant Surgery interface and rig buffers, without supplying Dancer resources. All **36** weighted preset/height extreme cases and **8** weighted VENUS/FLAT cases without a Hex preset passed. The eleven existing pose comparisons matched the earlier outputs byte for byte, including both weighted cases. Breast/butt parameter exclusions and independent Height were retained. Original dependency files were unchanged.

The checked ASI SHA-256 is `35b07cf24383cf44e57ddeed0ad9e927ca245ea83ee5c031a9a3ae514e567d34`. This change has offline/native-buffer verification, with no new in-game visual session.
