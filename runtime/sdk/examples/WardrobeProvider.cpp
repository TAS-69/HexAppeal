#include <Windows.h>
#include <cstring>
#include "../HexAppealWardrobe.h"



static const HA_WardrobeAPI* wardrobe=nullptr;
static const HA_BaseUnderwearAPI* baseUnderwear=nullptr;
static uint64_t registration=0;

bool RegisterMyWardrobeOutfit(bool myAssetsReady){
    const auto module=GetModuleHandleW(L"HexAppeal.asi");if(!module)return false;
    const auto getAPI=reinterpret_cast<HA_GetWardrobeAPI>(GetProcAddress(module,"HexAppeal_GetWardrobeAPI"));
    wardrobe=getAPI?getAPI(HA_WARDROBE_ABI):nullptr;if(!wardrobe)return false;
    const auto getBase=reinterpret_cast<HA_GetBaseUnderwearAPI>(GetProcAddress(module,"HexAppeal_GetBaseUnderwearAPI"));
    baseUnderwear=getBase?getBase(HA_BASE_UNDERWEAR_ABI):nullptr;
    HA_Option variants[2]{};
    strcpy_s(variants[0].id,"red");strcpy_s(variants[0].label,"Red");strcpy_s(variants[0].prefab,"my_dress_red");
    strcpy_s(variants[1].id,"blue");strcpy_s(variants[1].label,"Blue");strcpy_s(variants[1].prefab,"my_dress_blue");
    HA_Control controls[3]{};
    controls[0].size=sizeof(HA_Control);controls[0].type=HA_CHOICE;controls[0].scope=HA_PER_WITCH;controls[0].flags=HA_PREFAB_VARIANT;
    strcpy_s(controls[0].id,"colour");strcpy_s(controls[0].label,"Colour");controls[0].optionCount=2;controls[0].options=variants;
    controls[1].size=sizeof(HA_Control);controls[1].type=HA_TOGGLE;controls[1].scope=HA_PER_WITCH;controls[1].defaultValue=1;
    strcpy_s(controls[1].id,"panties");strcpy_s(controls[1].label,"Outfit panties");
    strcpy_s(controls[1].help,"Panties supplied by this outfit.");
    controls[2]=controls[1];strcpy_s(controls[2].id,"bra");strcpy_s(controls[2].label,"Outfit bra");strcpy_s(controls[2].help,"Bra supplied by this outfit.");
    if(baseUnderwear){controls[1].flags=HA_FOLLOW_BASE_UNDERWEAR;controls[2].flags=HA_FOLLOW_BASE_UNDERWEAR;}
    HA_Outfit outfit{};outfit.size=sizeof outfit;outfit.controlCount=3;outfit.controls=controls;
    strcpy_s(outfit.id,"yourname/my-dress");strcpy_s(outfit.label,"My dress");strcpy_s(outfit.prefab,"my_dress_red");
    strcpy_s(outfit.credit,"Your mod name and credits");strcpy_s(outfit.requirements,"Install and mount My Dress through DMM.");
    if(wardrobe->RegisterOutfit(&outfit,&registration)!=HA_OK)return false;
    return wardrobe->SetAvailable(registration,myAssetsReady,myAssetsReady?"":"My Dress resources are not mounted.")==HA_OK;
}


extern void SetMyOutfitPantiesVisible(int witch,bool visible);
extern void SetMyOutfitBraVisible(int witch,bool visible);
static bool effectiveVisibility(int witch,const char* control,bool& visible){
    if(baseUnderwear){HA_EffectiveControlValue value{};value.size=sizeof value;
        if(baseUnderwear->GetEffectiveControl(witch,"yourname/my-dress",control,&value)!=HA_OK)return false;
        visible=value.value!=0;return true;}
    HA_ControlValue value{};value.size=sizeof value;
    if(wardrobe->GetControl(witch,"yourname/my-dress",control,&value)!=HA_OK)return false;
    visible=value.value!=0;return true;
}
void UpdateMyWardrobeOutfit(){
    if(!wardrobe||!registration)return;
    for(int witch=0;witch<6;++witch){
        HA_Selection selected{};selected.size=sizeof selected;
        if(wardrobe->GetSelection(witch,&selected)!=HA_OK||!selected.available||strcmp(selected.outfit,"yourname/my-dress"))continue;

        bool visible=false;
        if(effectiveVisibility(witch,"panties",visible))SetMyOutfitPantiesVisible(witch,visible);
        if(effectiveVisibility(witch,"bra",visible))SetMyOutfitBraVisible(witch,visible);
    }
}
void UnregisterMyWardrobeOutfit(){if(wardrobe&&registration)wardrobe->UnregisterOutfit(registration);registration=0;wardrobe=nullptr;baseUnderwear=nullptr;}
