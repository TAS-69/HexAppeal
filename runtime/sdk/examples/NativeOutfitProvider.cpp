#include "ProviderDefinition.hpp"

namespace outfit_example {
static hexappeal::WardrobeClient client;
static uint64_t registration=0;
static bool availabilityKnown=false,lastAvailability=false;
static std::string error;

void Update(bool myMountedAssetsReady){
    if(!client.Connect()){error=client.LastError();return;}
    if(!registration){
        if(Register_yourname_my_dress(client,registration)!=HA_OK){error=client.LastError();return;}
        availabilityKnown=false;
    }
    if(!availabilityKnown||lastAvailability!=myMountedAssetsReady){
        if(client.Available(registration,myMountedAssetsReady,myMountedAssetsReady?"":"Install and mount the outfit assets.")!=HA_OK){error=client.LastError();return;}
        availabilityKnown=true;lastAvailability=myMountedAssetsReady;
    }
    error.clear();
}
std::string Status(){return error;}
void Shutdown(){client.Unregister(registration);client.Reset();availabilityKnown=false;}
}
