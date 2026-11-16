# SDK examples and use cases

## One dress, removable accessories

Start with `runtime/sdk/examples/parts-outfit.json`. It declares a default prefab, a bra toggle and a briefs toggle, each mapped to a real engine visibility key. Replace every placeholder. `PartsOutfitProvider.cpp` retries connection/registration, updates availability and unregisters on normal shutdown. Wardrobe applies those effective visibility values after it owns the correct assembly.

## Four complete outfit versions

Start with `outfit.json` and `NativeOutfitProvider.cpp`. Supply all four bra/briefs states. The generator emits the registration and native matrix. Wardrobe chooses the correct authored prefab; it does not generate one.

## Your mod already handles rendering

Use `WardrobeProvider.cpp`. Register ordinary controls, poll values and apply them through your own checked renderer. For linked underwear use `Effective(...)`, not just the stored choice. Reapply to newly created instances even if the saved revision is unchanged.

## Shared colour with an automatic refresh

Register the colour as global when every instance uses the same texture route. Your provider saves/applies/verifies that external resource, then requests refresh once. Keep asset work in your own mod. Use separate prefab/material resources for true per-witch colour; a global file replacement cannot provide it.

<details>
<summary><b>Build the supplied examples</b></summary>

From the repository root, with Visual Studio C++ tools, CMake and Python installed:

```powershell
cmake -S runtime/sdk -B build/sdk -A x64 -DHEX_APPEAL_BUILD_EXAMPLE=ON
cmake --build build/sdk --config Release
python -m unittest discover -s runtime/sdk/tests
```

These targets compile example objects for integration. They do not install an ASI or supply outfit assets. In the SDK-only download, replace `runtime/sdk` with `.`.

</details>

<details>
<summary><b>Why a toggle appears but nothing changes</b></summary>

Check availability, actual mounted routes, target witch and recognized outfit ownership. Confirm the engine part key rather than a modelling-tool object name. For custom rendering, confirm the provider reads effective values and handles rebuilt instances. For matrices, check that all combinations and the default prefab are authored and registered. A successful registration alone cannot remove embedded geometry or provide missing skin.

</details>
