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
