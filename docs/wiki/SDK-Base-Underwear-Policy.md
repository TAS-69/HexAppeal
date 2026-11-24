# SDK: An outfit with its own underwear

Use this optional policy when your outfit supplies its own bra or briefs and the normal game/body pair would overlap them. Your assets stay in your outfit package. Nothing replaces Damiane's shared underwear files.

## Definition

Add this top-level field to your outfit JSON:

```json
"replace_base_underwear": true
```

Keep the main clothing and custom underwear on separate controls:

```json
"controls": [
  {"id": "clothing", "label": "Clothing", "type": "toggle", "default": true, "follow_outfit_hide": true},
  {"id": "bra", "label": "Outfit bra", "type": "toggle", "default": true, "follow_base_underwear": true},
  {"id": "briefs", "label": "Outfit briefs", "type": "toggle", "default": true, "follow_base_underwear": true}
],
"part_visibility": [
  {"control": "clothing", "part": "MyOutfitClothing"},
  {"control": "bra", "part": "MyOutfitBra"},
  {"control": "briefs", "part": "MyOutfitBriefs"}
]
```

These are fragments, not a complete definition. Supply your normal schema, namespaced outfit ID, label, registered prefab, credits and requirements. Replace the part keys with actual engine visibility parts in your assets. Mesh object names alone do not create these parts. Alternatively supply every authored prefab combination, including the states with clothing hidden and underwear retained. Keep complete skin coverage in every state.

Generate the header as usual. The generated registration checks the optional capability before registering, sets the policy while the registration is unavailable, and unregisters if setup fails. Your provider may mark it available only after its own resources are ready.

## Result

| Selected outfit | Hide outfit | Hide base bra & panties | Result |
| --- | --- | --- | --- |
| Default | On | Off | Normal base underwear follows its original visibility rules. |
| Your outfit | Off | Off | Your clothing and saved custom underwear choices show; normal base underwear is suppressed. |
| Your outfit | On | Off | Main clothing hides; your saved custom underwear choices remain. |
| Your outfit | Either | On | Normal base underwear and linked custom underwear hide. |
| Back to Default | Either | Off | The normal pair returns through its original visibility rules. |

The suppression is per live instance, after Wardrobe confirms ownership of the selected replacement. It ends when that provider is unavailable, removed, or no longer owns the replacement. It does not edit the player's checkbox, saved mask, revision or individual garment preferences. Hiding one custom garment does not reveal the original pair underneath.

Both custom pieces follow the combined **Hide base bra & panties** override. Your individual bra/briefs controls remain separate saved choices. **Hide outfit** never hides these linked underwear controls.

The global **Allow external control (all witches)** setting lets detected external mods and outfit rules control their affected witches. The SDK reports Hex Appeal's override as inactive for each delegated witch. Turning the setting off restores the saved per-witch choices. A selected outfit's own `replace_base_underwear` policy still applies while that outfit is active.

## Direct C or C++ registration

The optional export is `HexAppeal_GetOutfitPolicyAPI(1)`. Its `HA_OutfitPolicyAPI` table has `SetPolicy(token, flags)`. The only current flag is `HA_REPLACE_BASE_UNDERWEAR`; zero leaves normal base handling unchanged. Existing Wardrobe, Native Wardrobe and Base underwear v1 layouts are unchanged.

With the C++ helper, check and set the policy before availability:

```cpp
if (!wardrobe.HasOutfitPolicy()) return HA_UNAVAILABLE;
const auto result = wardrobe.RegisterNative(definition, registration);
if (result != HA_OK) return result;
const auto policy = wardrobe.Policy(registration, HA_REPLACE_BASE_UNDERWEAR);
if (policy != HA_OK) {
    wardrobe.Unregister(registration);
    return policy;
}
return HA_OK;
```

This assumes `definition` describes your authored native outfit and `registration` starts at zero. After success, check mounted resources and call `Available` from your normal update. For C, resolve the export, check table size/version and the function pointer, then follow the same order. Unknown flags and stale tokens fail. Changing policy while the registration is available is refused.

Do not call `HideBase` or Base underwear's `SetHidden` automatically on selection. Those functions change the player's persistent preference and would also hide your linked custom underwear.

## Compatibility and checks

Older runtimes do not expose this extension. An outfit requiring it must remain unavailable with a clear message; do not approximate it by changing the player's checkbox or replacing shared body files. Outfits that omit the field retain their previous behavior.

The policy needs the checked native base-visibility route and a replacement Wardrobe can prove it owns. It cannot remove underwear merged into unrelated geometry, infer material masks, or fix missing skin. Custom renderers must still apply effective garment values on their own verified instances.

Test all four clothing/underwear combinations, individual garment choices, Default restoration, provider removal, NPC streaming, reloads and every supplied body variant. The development checks cover registration, preference separation and simulated native visibility. Actual asset rendering must be accepted separately before claiming compatibility.
