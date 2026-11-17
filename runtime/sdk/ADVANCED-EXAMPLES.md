# Advanced Wardrobe integrations

These examples use placeholder assets and neutral outfit names. Replace them with resources you own or have permission to distribute. The definitions and provider code compile against this SDK; they do not include a playable outfit or invent an engine asset API.

Use the [quick start](README.md) first. Run providers from a normal update after loader initialization, outside `DllMain`, and stop them before unloading. One serialized update every 100–250 ms is enough for these examples; renderer writes still need your game's correct thread.

## Choose an integration

| Resources you already have | Integration | Who applies changes? |
| --- | --- | --- |
| Independently addressable engine parts | `part_visibility` | Wardrobe, after it owns the matching replacement |
| Authored complete variants | `native_controls` plus every `states` row | Wardrobe selects your supplied prefab |
| Your own renderer/material system | Ordinary controls, polled by your provider | Your checked renderer bridge |

Keep stable outfit, control and option IDs. Display the actual mod/outfit title. A display-name correction does not require changing its ID. Registrations start unavailable; mark them ready only after the real mounted routes and required interfaces pass your checks. An unverified version alone should produce a warning through your own status/log; the v1 provider definition does not have a separate version-warning property.

## 1. Two colours, two garment switches and a belt

This example combines a native prefab matrix with one independent engine visibility part. Colour, bra and briefs choose an authored prefab. The belt keeps its own engine key and is unaffected by Base underwear.

Two colours × two bra values × two briefs values require **eight** states. `native_controls` replaces the single `prefab_variant` flag. Each mapped control is per-witch. The default row must match `prefab`. Every colour must supply correct skin coverage, materials, rigging, detail levels and cloth behavior for all four garment combinations.

### Definition

Save as `advanced-outfit.json`. These prefab names have no file extension or directory. `ExampleStudioBelt` must be an actual engine visibility key in each corresponding assembly, not a Blender object or material name.

```json
{
  "schema": 1,
  "id": "example/studio-outfit",
  "label": "Example Collection / Studio Outfit",
  "prefab": "example_studio_black_full",
  "credit": "Your outfit credits",
  "requirements": "Supply and mount your own eight outfit prefabs and their resources.",
  "controls": [
    {"id": "colour", "label": "Colour", "type": "choice", "default": "black", "options": [{"id": "black", "label": "Black"}, {"id": "white", "label": "White"}]},
    {"id": "bra", "label": "Outfit bra", "type": "toggle", "default": true, "follow_base_underwear": true},
    {"id": "briefs", "label": "Outfit briefs", "type": "toggle", "default": true, "follow_base_underwear": true},
    {"id": "belt", "label": "Belt", "type": "toggle", "default": true}
  ],
  "native_controls": ["colour", "bra", "briefs"],
  "states": [
    {"values": {"colour": "black", "bra": false, "briefs": false}, "prefab": "example_studio_black_bare"},
    {"values": {"colour": "black", "bra": false, "briefs": true}, "prefab": "example_studio_black_briefs"},
    {"values": {"colour": "black", "bra": true, "briefs": false}, "prefab": "example_studio_black_bra"},
    {"values": {"colour": "black", "bra": true, "briefs": true}, "prefab": "example_studio_black_full"},
    {"values": {"colour": "white", "bra": false, "briefs": false}, "prefab": "example_studio_white_bare"},
    {"values": {"colour": "white", "bra": false, "briefs": true}, "prefab": "example_studio_white_briefs"},
    {"values": {"colour": "white", "bra": true, "briefs": false}, "prefab": "example_studio_white_bra"},
    {"values": {"colour": "white", "bra": true, "briefs": true}, "prefab": "example_studio_white_full"}
  ],
  "part_visibility": [{"control": "belt", "part": "ExampleStudioBelt"}]
}
```

The generator validates every combination and emits a native definition. It does not create models, split meshes or mount resources.

```powershell
python tools/generate_provider.py examples/advanced-outfit.json --output build/AdvancedDefinition.hpp
```

### Provider lifecycle

Compile [AdvancedOutfitProvider.cpp](examples/AdvancedOutfitProvider.cpp) with the generated header and SDK include directory. The registration function name follows the outfit ID, so changing `example/studio-outfit` also changes `Register_example_studio_outfit`.

```cpp
#include "AdvancedDefinition.hpp"
#include <string>

namespace studio_example {
static hexappeal::WardrobeClient client;
static uint64_t registration=0;
static bool availabilityKnown=false,lastAvailability=false;
static std::string error;

void Update(bool mountedResourcesReady){
    if(!client.Connect()){error=client.LastError();return;}
    if(!client.HasNativeOutfits()||!client.HasBaseUnderwear()){
        error="This outfit needs native outfit and Base underwear API v1.";
        return;
    }
    if(!registration){
        if(Register_example_studio_outfit(client,registration)!=HA_OK){error=client.LastError();return;}
        availabilityKnown=false;
    }
    if(!availabilityKnown||lastAvailability!=mountedResourcesReady){
        if(client.Available(registration,mountedResourcesReady,mountedResourcesReady?"":"The outfit's mounted resources are unavailable.")!=HA_OK){
            error=client.LastError();return;
        }
        lastAvailability=mountedResourcesReady;
        availabilityKnown=true;
    }
    error.clear();
}
std::string Status(){return error;}
bool Shutdown(){
    if(client.Unregister(registration)!=HA_OK){error=client.LastError();return false;}
    client.Reset();availabilityKnown=false;error.clear();return true;
}
}
```

Your host calls `studio_example::Update(mountedResourcesReady)` after checking the actual installed routes and their compatibility. Retry normal updates while Hex Appeal is still loading. Read `Status()` for a failure. Call `Shutdown()` before unloading and handle a false result through your host's normal error path. The example does not force a selection or rewrite player preferences during registration.

Wardrobe applies native matrix and part values itself. Turning on **Hide base bra & panties** makes the effective `bra` and `briefs` values false. The selected colour and belt choice remain unchanged. Stored garment preferences are preserved and restored when that override is removed, including edits made while overridden.

## 2. A custom renderer and one shared colour resource

Use this route when your mod already owns rendering or shared material changes. The colour is global because every instance uses the same resource. Bra and briefs are per-witch toggles. This definition deliberately has no `native_controls`, `part_visibility` or `prefab_variant`: your provider applies its ordinary options.

```json
{
  "schema": 1,
  "id": "example/renderer-outfit",
  "label": "Example Collection / Runtime Outfit",
  "prefab": "example_runtime_outfit_u",
  "credit": "Your outfit credits",
  "requirements": "Supply your own outfit assets and implement the renderer bridge functions.",
  "controls": [
    {"id": "colour", "label": "Shared colour", "type": "choice", "scope": "global", "default": "black", "options": [{"id": "black", "label": "Black"}, {"id": "white", "label": "White"}]},
    {"id": "bra", "label": "Outfit bra", "type": "toggle", "default": true, "follow_base_underwear": true},
    {"id": "briefs", "label": "Outfit briefs", "type": "toggle", "default": true, "follow_base_underwear": true}
  ]
}
```

Generate its header before compiling the provider:

```powershell
python tools/generate_provider.py examples/renderer-outfit.json --output build/RendererDefinition.hpp
```

### The renderer bridge you implement

The three declarations at the top of [CustomRendererProvider.cpp](examples/CustomRendererProvider.cpp) are integration points for **your own code**, not functions exported by Hex Appeal:

| Function | Required behavior |
| --- | --- |
| `ApplyAndVerifySharedColour(value)` | Validate the option index, apply/save the corresponding resource owned by your mod, verify it, then return true. Keep originals intact. Return false on failure. |
| `OwnedInstanceGeneration(witch)` | Return zero unless an actual live instance belongs to this exact outfit. Return a nonzero generation that changes whenever the instance/assembly is recreated. Resolve ownership through your own renderer/resource identifiers. |
| `ApplyOwnedVisibility(witch, bra, briefs)` | On the correct engine thread, recheck ownership and set only your own garment parts. Return false if the instance changed or a write could not be verified. |

`GetSelection` reports saved/requested state; it is not proof that a replacement is already rendered. That is why the provider checks its owned instance separately.

```cpp
#include "RendererDefinition.hpp"
#include <array>
#include <cstring>
#include <string>

namespace renderer_example {
bool ApplyAndVerifySharedColour(uint32_t value);
uint64_t OwnedInstanceGeneration(int witch);
bool ApplyOwnedVisibility(int witch,bool bra,bool briefs);

static constexpr const char* outfitId="example/renderer-outfit";
static hexappeal::WardrobeClient client;
static uint64_t registration=0;
static bool availabilityKnown=false,lastAvailability=false,colourKnown=false,pendingRefresh=false;
static uint32_t appliedColour=0;
static std::string error;
struct Receipt {
    bool valid=false;
    uint64_t instance=0,selection=0,bra=0,braBase=0,briefs=0,briefsBase=0;
};
static std::array<Receipt,6> receipts{};

void Update(bool mountedResourcesReady){
    if(!client.Connect()){error=client.LastError();return;}
    if(!client.HasBaseUnderwear()||!client.HasRefresh()){
        error="This provider needs Base underwear v1 and the refresh extension.";return;
    }
    if(!registration){
        if(Register_example_renderer_outfit(client,registration)!=HA_OK){error=client.LastError();return;}
        availabilityKnown=false;colourKnown=false;receipts={};
    }
    if(!availabilityKnown||lastAvailability!=mountedResourcesReady){
        if(client.Available(registration,mountedResourcesReady,mountedResourcesReady?"":"The outfit's mounted resources are unavailable.")!=HA_OK){error=client.LastError();return;}
        availabilityKnown=true;lastAvailability=mountedResourcesReady;
        if(!mountedResourcesReady){colourKnown=false;receipts={};}
    }
    if(!mountedResourcesReady)return;
    HA_ControlValue colour{};
    if(client.Stored(-1,outfitId,"colour",colour)!=HA_OK){error=client.LastError();return;}
    if(!colourKnown||colour.value!=appliedColour){
        if(!ApplyAndVerifySharedColour(colour.value)){error="The provider could not save and verify its shared colour.";return;}
        appliedColour=colour.value;colourKnown=true;pendingRefresh=true;
    }
    if(pendingRefresh){
        const auto result=client.Refresh(-1,outfitId);
        if(result==HA_OK||result==HA_UNAVAILABLE)pendingRefresh=false;
        else{error=client.LastError();return;}
    }
    for(int witch=0;witch<6;++witch){
        HA_Selection selection{};
        if(client.Selection(witch,selection)!=HA_OK){error=client.LastError();return;}
        auto& receipt=receipts[size_t(witch)];
        if(!selection.available||std::strcmp(selection.outfit,outfitId)){receipt.valid=false;continue;}
        const auto instance=OwnedInstanceGeneration(witch);
        if(!instance){receipt.valid=false;continue;}
        HA_EffectiveControlValue bra{},briefs{};
        if(client.Effective(witch,outfitId,"bra",bra)!=HA_OK||client.Effective(witch,outfitId,"briefs",briefs)!=HA_OK){error=client.LastError();return;}
        if(receipt.valid&&receipt.instance==instance&&receipt.selection==selection.revision&&receipt.bra==bra.revision&&receipt.braBase==bra.baseUnderwearRevision&&receipt.briefs==briefs.revision&&receipt.briefsBase==briefs.baseUnderwearRevision)continue;
        if(!ApplyOwnedVisibility(witch,bra.value!=0,briefs.value!=0)){error="The provider could not update its owned outfit instance.";return;}
        receipt={true,instance,selection.revision,bra.revision,bra.baseUnderwearRevision,briefs.revision,briefs.baseUnderwearRevision};
    }
    error.clear();
}
std::string Status(){return error;}
bool Shutdown(){
    if(client.Unregister(registration)!=HA_OK){error=client.LastError();return false;}
    client.Reset();availabilityKnown=false;colourKnown=false;pendingRefresh=false;receipts={};error.clear();return true;
}
}
```

The renderer reads **effective** values, including Base underwear overrides. It watches the outfit revision, Base underwear revision, selection revision and instance generation. Checking only the saved preference misses rebuilt actors and overrides.

A shared colour change follows **apply → save/verify → request refresh**. Refresh is requested once per applied change and retried only until accepted. `HA_UNAVAILABLE` can mean no owned matching replacement is visible; its next assembly uses the current resource. Other errors remain visible through `Status()`. A successful queue request does not prove correct rendering: inspect your materials after the rebuild.

Do not implement per-witch colour by replacing a globally shared file. Use independent authored prefabs/material resources and the first example if each witch needs its own colour.

## Targets, results and persistence

| Value | Target |
| --- | --- |
| 0 / 1 / 2 | Lyselia / Bari / Areciel |
| 3 / 4 / 5 | Elowen / Sylvia / human White Crow |
| -1 | Set All for per-witch controls, or access a global control |

Query per-witch controls for one witch at a time. `Stored(...)` returns the preference; `Effective(...)` returns the value to render. `BaseState(-1, ...)` also reports mixed state and the hidden mask. A missing Base underwear extension must not be mistaken for an effective value of zero.

| Result | Typical handling |
| --- | --- |
| `HA_OK` | Continue; native assembly may still be asynchronous |
| `HA_NOT_READY` | Wait for normal initialization or the required interface |
| `HA_UNAVAILABLE` | Show the reason, hold the affected feature and retain the saved choice |
| `HA_INVALID`, `HA_LIMIT`, `HA_DUPLICATE` | Fix the provider definition or lifecycle; do not retry unchanged bad data every frame |
| `HA_IO_ERROR` | Report persistence failure and keep the previous accepted preference |
| `HA_NOT_FOUND` | Recheck the outfit/control/token lifecycle before writing |

Read `LastError()` immediately after the failed API call on the same serialized client/thread. API tables are valid only while Hex Appeal stays loaded; hot-unloading Hex Appeal itself is unsupported. Definitions are copied during registration, so the temporary arrays emitted by the generator are safe to release.

## Build and validate

From this SDK directory, with Visual Studio C++ tools, CMake and Python:

```powershell
cmake -S . -B build/sdk -A x64 -DHEX_APPEAL_BUILD_EXAMPLE=ON
cmake --build build/sdk --config Release
python -m unittest discover -s tests
```

These targets compile object libraries for integration into your own x64 C++17 plugin. They do not provide a loader, link your renderer bridge or install an ASI. In the full source repository, use `runtime/sdk` as the CMake source directory and `runtime/sdk/tests` for the test path.

## Rendered acceptance

1. Verify every authored state, all supported witches, ordinary blends, shape extremes, every detail level and moving cloth.
2. Check exposed skin, material slots, occlusion, rigging and cloth after each toggle. A hidden garment cannot recreate skin omitted from your assembly.
3. Test Base underwear on/off and edits made during the override. Confirm unrelated accessories stay unchanged.
4. Test Default → outfit → Default, rapid changes, area streaming, save reload and full restart. Let native assemblies finish before judging the result.
5. Remove/restore a required resource in a disposable test install. Confirm availability changes, a useful reason and retained choices. Do not overwrite a player's active installation to run that check.
6. Test custom spouse/barber clothing. Keep renderer writes limited to your own proven instances.

The SDK checks definitions and ownership; rendered correctness belongs to the adopting outfit. These examples are 0BSD and grant no rights to someone else's assets.
