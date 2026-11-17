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
