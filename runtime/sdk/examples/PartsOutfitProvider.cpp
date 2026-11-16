#include "PartsDefinition.hpp"

namespace example_parts {
hexappeal::WardrobeClient wardrobe;
uint64_t registration=0;
bool lastAvailable=false;

void Update(bool ownMountedAssetsReady){
    if(!wardrobe.Connect())return;
    if(!registration){
        if(Register_example_parts_dress(wardrobe,registration)!=HA_OK)return;
        lastAvailable=false;
    }
    if(lastAvailable!=ownMountedAssetsReady&&wardrobe.Available(registration,ownMountedAssetsReady,"Mount the provider's outfit resources first.")==HA_OK)
        lastAvailable=ownMountedAssetsReady;
}
void Shutdown(){wardrobe.Unregister(registration);wardrobe.Reset();}
}
