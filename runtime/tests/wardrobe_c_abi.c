#include "../sdk/HexAppealWardrobe.h"

const HA_WardrobeAPI* CheckWardrobeCHeader(HA_GetWardrobeAPI getAPI){
    HA_ControlValue value={0};value.size=sizeof value;
    const HA_WardrobeAPI* api=getAPI(HA_WARDROBE_ABI);
    if(api&&api->size==sizeof(HA_WardrobeAPI))api->GetControl(0,"example/dress","panties",&value);
    return api;
}
typedef char OriginalWardrobeTableUnchanged[sizeof(HA_WardrobeAPI)==72?1:-1];
typedef char UnderwearStateLayout[sizeof(HA_BaseUnderwearState)==32?1:-1];
typedef char EffectiveControlLayout[sizeof(HA_EffectiveControlValue)==40?1:-1];
typedef char PrefabStateLayout[sizeof(HA_PrefabState)==164?1:-1];
typedef char VisibilityPartLayout[sizeof(HA_VisibilityPart)==100?1:-1];
typedef char NativeOutfitLayout[sizeof(HA_NativeOutfit)==40?1:-1];
typedef char NativeTableLayout[sizeof(HA_NativeWardrobeAPI)==16?1:-1];
typedef char OutfitPolicyTableLayout[sizeof(HA_OutfitPolicyAPI)==16?1:-1];
typedef char OriginalOutfitLayout[sizeof(HA_Outfit)==688?1:-1];
const HA_OutfitPolicyAPI* CheckOutfitPolicyCHeader(HA_GetOutfitPolicyAPI getAPI){
    const HA_OutfitPolicyAPI* api=getAPI(HA_OUTFIT_POLICY_ABI);
    if(api&&api->size==sizeof(HA_OutfitPolicyAPI))api->SetPolicy(0,HA_REPLACE_BASE_UNDERWEAR);
    return api;
}
const HA_BaseUnderwearAPI* CheckBaseUnderwearCHeader(HA_GetBaseUnderwearAPI getAPI){
    HA_BaseUnderwearState state={0};HA_EffectiveControlValue value={0};
    const HA_BaseUnderwearAPI* api=getAPI(HA_BASE_UNDERWEAR_ABI);
    state.size=sizeof state;value.size=sizeof value;
    if(api&&api->size==sizeof(HA_BaseUnderwearAPI)){
        api->GetState(-1,&state);api->GetEffectiveControl(0,"example/dress","panties",&value);
    }
    return api;
}
