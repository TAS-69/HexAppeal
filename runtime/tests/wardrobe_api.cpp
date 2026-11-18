#include "wardrobe_api.h"
#include "underwear.h"
#include "../sdk/HexAppealWardrobeClient.hpp"
#include <Windows.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <atomic>
#include <thread>
extern "C" const HA_WardrobeAPI* HA_CALL HexAppeal_GetWardrobeAPI(uint32_t);
extern "C" const HA_BaseUnderwearAPI* HA_CALL HexAppeal_GetBaseUnderwearAPI(uint32_t);
extern "C" const HA_NativeWardrobeAPI* HA_CALL HexAppeal_GetNativeWardrobeAPI(uint32_t);
extern "C" const HA_OutfitVisibilityAPI* HA_CALL HexAppeal_GetOutfitVisibilityAPI(uint32_t);
extern "C" const HA_OutfitPolicyAPI* HA_CALL HexAppeal_GetOutfitPolicyAPI(uint32_t);
extern "C" int HA_CALL HexAppeal_RequestWardrobeRefreshV1(int32_t,const char*);
namespace hex {
static std::string selection="hexappeal/default";static std::atomic<unsigned> reapplied{0};
static UnderwearPreference preference{};static bool rejectUnderwearSave=false;
static UnderwearPreference nudePreference{};
static bool baseNativeReady=false;
UnderwearStatus underwearStatus(int){return {baseNativeReady,false,false,true,"Native fixture not loaded"};}
UnderwearPreference outfitHiddenPreference(){return nudePreference;}
UnderwearStatus outfitHiddenStatus(int){return {true,false,false,false,"Fixture"};}
bool setOutfitHidden(int target,bool hidden,std::string& error){if(rejectUnderwearSave){error="Fixture save denied";return false;}const auto mask=target<0?63u:1u<<target;const auto next=hidden?(nudePreference.hiddenMask|mask):(nudePreference.hiddenMask&~mask);if(next!=nudePreference.hiddenMask){nudePreference.hiddenMask=next;++nudePreference.revision;}return true;}
UnderwearPreference baseUnderwearPreference(){return preference;}
bool setUnderwearHidden(int target,bool hidden,std::string& error){if(rejectUnderwearSave){error="Fixture save denied";return false;}const auto mask=target<0?63u:1u<<target;const auto next=hidden?(preference.hiddenMask|mask):(preference.hiddenMask&~mask);if(next!=preference.hiddenMask){preference.hiddenMask=next;++preference.revision;}return true;}
bool requestWardrobeOutfit(int witch,const std::string& id,std::string& error){WardrobeProviderOutfit outfit;if(witch< -1||witch>5||!resolveWardrobeProvider(id,witch<0?0:witch,outfit,error))return false;selection=id;return true;}
bool nativeWardrobeSelection(int witch,std::string& id,std::string& prefab,bool& available,uint64_t& stamp){if(witch<0||witch>5)return false;id=selection;WardrobeProviderOutfit outfit;std::string error;available=resolveWardrobeProvider(id,witch,outfit,error);prefab=outfit.prefab;stamp=1;return true;}
void reapplyWardrobeProviderControls(int,const std::string&){++reapplied;}
bool requestSelectedWardrobeRefresh(int witch,const std::string& id,std::string& error){if(witch< -1||witch>5||id!=selection){error="No selected replacement";return false;}return true;}
}
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static HA_WardrobeAPI invalidClientAPI{};
static const HA_WardrobeAPI* HA_CALL invalidClientFactory(uint32_t){return &invalidClientAPI;}
int wmain(int argc,wchar_t** argv){try{
    check(argc==2,"fixture path required");const auto root=std::filesystem::path(argv[1]);std::filesystem::create_directories(root);const auto file=root/L"controls.ini";std::error_code ec;std::filesystem::remove(file,ec);
    const auto* base=HexAppeal_GetBaseUnderwearAPI(1);HA_BaseUnderwearState state{};state.size=sizeof state;
    const auto* nude=HexAppeal_GetOutfitVisibilityAPI(1);HA_OutfitVisibilityState nudeState=hexappeal::Sized<HA_OutfitVisibilityState>();
    check(nude&&nude->size==sizeof(HA_OutfitVisibilityAPI)&&!HexAppeal_GetOutfitVisibilityAPI(2),"Hide outfit is an optional versioned extension");
    check(nude->GetState(3,&nudeState)==HA_NOT_READY&&nude->SetHidden(3,1)==HA_NOT_READY,"Hide outfit never publishes before initialization");
    check(base&&base->size==sizeof(HA_BaseUnderwearAPI)&&!HexAppeal_GetBaseUnderwearAPI(2),"optional versioned underwear ABI");
    check(base->GetState(3,&state)==HA_NOT_READY&&base->SetHidden(3,1)==HA_NOT_READY,"pre-initialization calls fail before publishing preferences");
    std::string error;check(hex::initializeWardrobeAPI(file,error),"initialize API settings");const auto* api=HexAppeal_GetWardrobeAPI(1);check(api&&api->size==72&&!HexAppeal_GetWardrobeAPI(2),"original versioned C ABI layout unchanged");
    HA_Option variants[2]{};strcpy_s(variants[0].id,"red");strcpy_s(variants[0].label,"Red");strcpy_s(variants[0].prefab,"test_dress_red");strcpy_s(variants[1].id,"blue");strcpy_s(variants[1].label,"Blue");strcpy_s(variants[1].prefab,"test_dress_blue");
    HA_Control controls[4]{};for(auto& control:controls)control.size=sizeof control;
    strcpy_s(controls[0].id,"colour");strcpy_s(controls[0].label,"Colour");controls[0].flags=HA_PREFAB_VARIANT;controls[0].optionCount=2;controls[0].options=variants;
    strcpy_s(controls[1].id,"panties");strcpy_s(controls[1].label,"Outfit panties");controls[1].type=HA_TOGGLE;controls[1].defaultValue=1;
    strcpy_s(controls[2].id,"sparkles");strcpy_s(controls[2].label,"Sparkles");controls[2].type=HA_TOGGLE;controls[2].scope=HA_GLOBAL;
    controls[1].flags=HA_FOLLOW_BASE_UNDERWEAR;controls[3]=controls[1];strcpy_s(controls[3].id,"bra");strcpy_s(controls[3].label,"Outfit bra");
    HA_Outfit outfit{};outfit.size=sizeof outfit;strcpy_s(outfit.id,"example/dress");strcpy_s(outfit.label,"Example dress");strcpy_s(outfit.prefab,"test_dress_red");outfit.controlCount=4;outfit.controls=controls;uint64_t handle=0;
    check(api->RegisterOutfit(&outfit,&handle)==HA_OK&&handle,"register choices and toggles");check(api->RegisterOutfit(&outfit,&handle)==HA_DUPLICATE,"duplicate stable ID rejected");
    const auto* policy=HexAppeal_GetOutfitPolicyAPI(1);
    check(policy&&policy->size==sizeof(HA_OutfitPolicyAPI)&&!HexAppeal_GetOutfitPolicyAPI(2),"optional outfit policy leaves existing ABI unchanged");
    check(policy->SetPolicy(handle,2)==HA_INVALID&&policy->SetPolicy(0,HA_REPLACE_BASE_UNDERWEAR)==HA_NOT_FOUND,"policy rejects unknown flags and stale tokens");
    check(policy->SetPolicy(handle,HA_REPLACE_BASE_UNDERWEAR)==HA_OK,"register outfit-scoped underwear replacement before availability");
    uint64_t other=0;auto invalid=outfit;strcpy_s(invalid.id,"hexappeal/reserved");check(api->RegisterOutfit(&invalid,&other)==HA_INVALID,"reserved namespace rejected");invalid=outfit;strcpy_s(invalid.id,"example/bad");invalid.controls=reinterpret_cast<HA_Control*>(1);check(api->RegisterOutfit(&invalid,&other)==HA_INVALID,"unreadable schema rejected before publication");
    invalid=outfit;strcpy_s(invalid.id,"example/bad-flags");controls[1].scope=HA_GLOBAL;check(api->RegisterOutfit(&invalid,&other)==HA_INVALID,"linked underwear cannot silently hide a global shared mesh");controls[1].scope=HA_PER_WITCH;
    controls[0].flags=HA_FOLLOW_BASE_UNDERWEAR;check(api->RegisterOutfit(&invalid,&other)==HA_INVALID,"choices cannot use visibility semantics");controls[0].flags=HA_PREFAB_VARIANT;
    controls[1].flags=HA_FOLLOW_BASE_UNDERWEAR|HA_PREFAB_VARIANT;check(api->RegisterOutfit(&invalid,&other)==HA_INVALID,"linked visibility rejects incompatible flags");controls[1].flags=HA_FOLLOW_BASE_UNDERWEAR;
    controls[1].flags=8;check(api->RegisterOutfit(&invalid,&other)==HA_INVALID,"unknown flags rejected");controls[1].flags=HA_FOLLOW_BASE_UNDERWEAR;
    std::memset(controls,0,sizeof controls);std::memset(variants,0,sizeof variants);std::memset(&outfit,0,sizeof outfit);
    check(api->SelectOutfit(3,"example/dress")==HA_UNAVAILABLE,"unready dependency gate");
    check(api->SetAvailable(handle,1,"")==HA_OK&&api->SelectOutfit(3,"example/dress")==HA_UNAVAILABLE,"scoped underwear requires a working native base route");
    hex::baseNativeReady=true;check(api->SelectOutfit(3,"example/dress")==HA_OK,"provider can select its copied definition once native visibility is ready");
    HA_ControlValue value{};value.size=sizeof value;check(api->GetControl(3,"example/dress","panties",&value)==HA_OK&&value.value==1,"default toggle supplied");
    HA_EffectiveControlValue effective{};effective.size=sizeof effective;
    hex::WardrobeProviderOutfit scoped;
    check(hex::resolveWardrobeProvider("example/dress",3,scoped,error)&&scoped.policyFlags==HA_REPLACE_BASE_UNDERWEAR,"resolved provider retains scoped underwear policy");
    check(policy->SetPolicy(handle,0)==HA_UNAVAILABLE&&hex::baseUnderwearPreference().hiddenMask==0,"available provider policy cannot change or rewrite player visibility");
    check(base->GetEffectiveControl(3,"example/dress","panties",&effective)==HA_OK&&effective.value==1&&effective.storedValue==1,"visible default effective preference");
    const auto originalRevision=effective.revision,originalBaseRevision=effective.baseUnderwearRevision;
    hex::baseNativeReady=false;
    check(base->SetHidden(3,1)==HA_OK&&base->GetState(3,&state)==HA_OK&&state.hidden&&!state.mixed&&state.hiddenMask==8&&!state.nativeReady&&state.nativePending,"provider preference observable independently of native hook readiness");
    hex::baseNativeReady=true;
    for(const char* control:{"bra","panties"})check(base->GetEffectiveControl(3,"example/dress",control,&effective)==HA_OK&&!effective.value&&effective.storedValue==1&&effective.baseUnderwearHidden&&effective.revision==originalRevision&&effective.baseUnderwearRevision!=originalBaseRevision,"base hide overrides both opted-in parts without rewriting saved outfit controls");
    check(api->GetControl(3,"example/dress","panties",&value)==HA_OK&&value.value==1,"original GetControl preserves stored values");
    check(base->GetEffectiveControl(2,"example/dress","panties",&effective)==HA_OK&&effective.value==1&&!effective.baseUnderwearHidden,"another witch remains independent");
    check(base->GetState(-1,&state)==HA_OK&&state.mixed&&!state.hidden,"All exposes mixed preference");
    check(api->SetControl(-1,"example/dress","sparkles",1)==HA_OK&&base->GetEffectiveControl(3,"example/dress","sparkles",&effective)==HA_OK&&effective.value==1,"unrelated controls unaffected by base hide");
    check(api->SetControl(3,"example/dress","bra",0)==HA_OK&&base->SetHidden(3,0)==HA_OK,"edit saved preference while overridden and show again");
    check(base->GetEffectiveControl(3,"example/dress","panties",&effective)==HA_OK&&effective.value==1&&base->GetEffectiveControl(3,"example/dress","bra",&effective)==HA_OK&&!effective.value,"show restores each outfit's own choice");
    check(base->SetHidden(-1,1)==HA_OK&&base->GetState(-1,&state)==HA_OK&&state.hidden&&!state.mixed&&state.hiddenMask==63,"All hides linked underwear on all witches");
    hex::rejectUnderwearSave=true;const auto savedRevision=state.revision;check(base->SetHidden(3,0)==HA_UNAVAILABLE&&base->GetState(3,&state)==HA_OK&&state.hidden&&state.revision==savedRevision,"failed preference save preserves visibility and revision");hex::rejectUnderwearSave=false;
    check(base->SetHidden(-1,0)==HA_OK,"restore default visibility after All");
    check(base->SetHidden(6,1)==HA_INVALID&&base->SetHidden(3,2)==HA_INVALID&&base->GetState(-2,&state)==HA_INVALID&&base->GetEffectiveControl(-1,"example/dress","panties",&effective)==HA_INVALID,"invalid visibility inputs rejected");
    check(base->GetState(3,reinterpret_cast<HA_BaseUnderwearState*>(1))==HA_INVALID&&base->GetEffectiveControl(3,"example/dress","panties",reinterpret_cast<HA_EffectiveControlValue*>(1))==HA_INVALID,"unreadable outputs fail safely");
    check(api->SetControl(3,"example/dress","colour",1)==HA_OK&&hex::reapplied==1,"prefab variant queues only native refresh");
    HA_Selection selected{};selected.size=sizeof selected;check(api->GetSelection(3,&selected)==HA_OK&&!strcmp(selected.prefab,"test_dress_blue"),"variant resolves to registered prefab");
    check(api->SetControl(3,"example/dress","panties",0)==HA_OK&&api->GetControl(3,"example/dress","panties",&value)==HA_OK&&value.value==0,"provider-owned toggle saved");check(api->GetControl(2,"example/dress","panties",&value)==HA_OK&&value.value==1,"per-witch option independent");
    check(api->SetControl(-1,"example/dress","sparkles",1)==HA_OK&&api->GetControl(2,"example/dress","sparkles",&value)==HA_OK&&value.value==1&&value.scope==HA_GLOBAL,"global option shared across witches");
    check(api->SetControl(-1,"example/dress","panties",0)==HA_OK,"All applies per-witch controls");for(int i=0;i<6;++i)check(api->GetControl(i,"example/dress","panties",&value)==HA_OK&&value.value==0,"all six states updated");
    check(hex::initializeWardrobeAPI(file,error)&&api->GetControl(3,"example/dress","colour",&value)==HA_OK&&value.value==1,"provider options survive reload");
    check(hex::pinWardrobeControl("example/dress","panties",true,error)&&hex::initializeWardrobeAPI(file,error)&&hex::wardrobeControlPinned("example/dress","panties"),"individual provider pin persists independently of its value");
    const auto locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(locked!=INVALID_HANDLE_VALUE,"lock fixture");
    check(api->SetControl(3,"example/dress","colour",0)==HA_UNAVAILABLE&&api->GetControl(3,"example/dress","colour",&value)==HA_OK&&value.value==1,"failed save leaves live option unchanged");CloseHandle(locked);
    check(api->SetControl(3,"example/dress","colour",99)==HA_UNAVAILABLE,"invalid option rejected");char tiny[2]{'X','X'};check(api->GetLastError(tiny,2)==HA_LIMIT&&tiny[1]==0,"truncated errors remain terminated");
    check(api->UnregisterOutfit(handle)==HA_OK&&api->SelectOutfit(3,"example/dress")==HA_UNAVAILABLE,"removed provider unavailable without dangling callbacks");
    check(policy->SetPolicy(handle,0)==HA_NOT_FOUND,"unregistered provider cannot leave a mutable suppression policy");
    check(base->GetEffectiveControl(3,"example/dress","panties",&effective)==HA_UNAVAILABLE,"unloaded providers never return effective renderer instructions");
    HA_Outfit replacement{};replacement.size=sizeof replacement;strcpy_s(replacement.id,"example/dress");strcpy_s(replacement.label,"Updated dress");strcpy_s(replacement.prefab,"updated_dress");const auto oldHandle=handle;check(api->RegisterOutfit(&replacement,&handle)==HA_OK&&handle!=oldHandle&&api->SetAvailable(oldHandle,1,"")==HA_NOT_FOUND,"re-registration replaces copied schema and invalidates old token");
    const auto* native=HexAppeal_GetNativeWardrobeAPI(1);check(native&&native->size==sizeof(HA_NativeWardrobeAPI)&&!HexAppeal_GetNativeWardrobeAPI(2),"optional native combinations ABI");
    hexappeal::WardrobeClient client;
    check(!client.Attach(nullptr)&&!client.Connected(),"missing module is recoverable");
    invalidClientAPI.size=sizeof invalidClientAPI;invalidClientAPI.version=1;
    check(!client.Attach(invalidClientFactory),"helper rejects incomplete function tables before calling them");
    check(client.Attach(HexAppeal_GetWardrobeAPI)&&!client.HasBaseUnderwear()&&!client.HasNativeCombinations(),"older API retains independent capabilities");
    check(!client.HasOutfitPolicy()&&client.Policy(handle,HA_REPLACE_BASE_UNDERWEAR)==HA_UNAVAILABLE,"older runtimes reject required scoped policy without partial activation");
    check(client.Effective(3,"example/native","bra",effective)==HA_UNAVAILABLE&&!client.LastError().empty(),"missing extension reports its own clear error");
    check(client.Attach(HexAppeal_GetWardrobeAPI,HexAppeal_GetBaseUnderwearAPI,HexAppeal_GetNativeWardrobeAPI,HexAppeal_RequestWardrobeRefreshV1)&&client.HasBaseUnderwear()&&client.HasNativeCombinations()&&client.HasRefresh(),"helper retries complete API discovery");
    check(client.Attach(HexAppeal_GetWardrobeAPI,HexAppeal_GetBaseUnderwearAPI,HexAppeal_GetNativeWardrobeAPI,HexAppeal_RequestWardrobeRefreshV1,HexAppeal_GetOutfitVisibilityAPI,HexAppeal_GetOutfitPolicyAPI)&&client.HasOutfitPolicy()&&client.Policy(handle,HA_REPLACE_BASE_UNDERWEAR)==HA_OK,"client discovers and sets scoped policy on a new unavailable registration");
    check(hex::baseUnderwearPreference().hiddenMask==0,"client policy never toggles saved base visibility");
    char bounded[8]="keep";check(!hexappeal::Text(bounded,"far-too-long")&&!strcmp(bounded,"keep")&&hexappeal::Text(bounded,"")&&!bounded[0],"helper rejects truncation and handles empty text");
    HA_Control nativeControls[2]{};
    for(auto& c:nativeControls){c.size=sizeof c;c.type=HA_TOGGLE;c.defaultValue=1;c.flags=HA_FOLLOW_BASE_UNDERWEAR;}
    strcpy_s(nativeControls[0].id,"bra");strcpy_s(nativeControls[0].label,"Bra");strcpy_s(nativeControls[1].id,"panties");strcpy_s(nativeControls[1].label,"Panties");
    HA_PrefabState states[4]{};for(unsigned i=0;i<4;++i){states[i].size=sizeof(HA_PrefabState);states[i].values[0]=i/2;states[i].values[1]=i%2;sprintf_s(states[i].prefab,"test_ub_combination_%u",i);}
    HA_Outfit definition{};definition.size=sizeof definition;definition.controlCount=2;definition.controls=nativeControls;strcpy_s(definition.id,"example/native");strcpy_s(definition.label,"Native test outfit");strcpy_s(definition.prefab,states[3].prefab);
    HA_NativeOutfit nativeDefinition{};nativeDefinition.size=sizeof nativeDefinition;nativeDefinition.outfit=&definition;nativeDefinition.controlMask=3;nativeDefinition.stateCount=4;nativeDefinition.states=states;
    uint64_t nativeHandle=0;
    auto badNative=nativeDefinition;badNative.stateCount=3;check(native->RegisterOutfit(&badNative,&nativeHandle)==HA_INVALID,"incomplete native combinations rejected");
    states[2].values[0]=0;check(native->RegisterOutfit(&nativeDefinition,&nativeHandle)==HA_INVALID,"duplicate combinations rejected");states[2].values[0]=1;
    states[2].values[7]=1;check(native->RegisterOutfit(&nativeDefinition,&nativeHandle)==HA_INVALID,"unmapped native state values rejected");states[2].values[7]=0;
    nativeControls[0].scope=HA_GLOBAL;check(native->RegisterOutfit(&nativeDefinition,&nativeHandle)==HA_INVALID,"shared visibility cannot masquerade as per-witch prefab state");nativeControls[0].scope=HA_PER_WITCH;
    strcpy_s(definition.prefab,states[0].prefab);check(native->RegisterOutfit(&nativeDefinition,&nativeHandle)==HA_INVALID,"default prefab must match defaults");strcpy_s(definition.prefab,states[3].prefab);
    badNative=nativeDefinition;badNative.states=reinterpret_cast<HA_PrefabState*>(1);check(native->RegisterOutfit(&badNative,&nativeHandle)==HA_INVALID,"unreadable native states fail before publication");
    check(native->RegisterOutfit(&nativeDefinition,&nativeHandle)==HA_OK&&api->SetAvailable(nativeHandle,1,"")==HA_OK,"register complete native visibility combinations");
    memset(nativeControls,0,sizeof nativeControls);memset(states,0,sizeof states);memset(&definition,0,sizeof definition);
    check(api->SelectOutfit(3,"example/native")==HA_OK&&api->GetSelection(3,&selected)==HA_OK&&!strcmp(selected.prefab,"test_ub_combination_3"),"native defaults resolve after all source arrays are released");
    check(api->SetControl(3,"example/native","bra",0)==HA_OK&&api->GetSelection(3,&selected)==HA_OK&&!strcmp(selected.prefab,"test_ub_combination_1"),"native bra toggle selects its declared prefab");
    check(api->SetControl(3,"example/native","panties",0)==HA_OK&&api->GetSelection(3,&selected)==HA_OK&&!strcmp(selected.prefab,"test_ub_combination_0"),"native toggles compose");
    check(api->SetControl(3,"example/native","bra",1)==HA_OK&&api->GetSelection(3,&selected)==HA_OK&&!strcmp(selected.prefab,"test_ub_combination_2"),"all four combinations reachable");
    check(base->SetHidden(3,1)==HA_OK&&api->GetSelection(3,&selected)==HA_OK&&!strcmp(selected.prefab,"test_ub_combination_0"),"base hide chooses authored hidden native combination");
    check(api->GetControl(3,"example/native","bra",&value)==HA_OK&&value.value==1&&base->SetHidden(3,0)==HA_OK&&api->GetSelection(3,&selected)==HA_OK&&!strcmp(selected.prefab,"test_ub_combination_2"),"native base override preserves and restores saved choices");
    check(api->GetSelection(2,&selected)==HA_OK&&!strcmp(selected.prefab,"test_ub_combination_3"),"native combinations are independent per witch");
    const auto beforeNoop=hex::reapplied.load();check(api->SetControl(3,"example/native","bra",1)==HA_OK&&hex::reapplied==beforeNoop,"unchanged values do not queue rebuilds");
    check(client.Stored(3,"example/native","bra",value)==HA_OK,"helper initializes output structure sizes");const auto beforeAvailable=value.revision;
    check(client.Available(nativeHandle,true)==HA_OK&&client.Stored(3,"example/native","bra",value)==HA_OK&&value.revision==beforeAvailable,"repeated availability polls do not invalidate option revisions");
    std::atomic<bool> threadFailure{false};std::thread first([&]{for(int i=0;i<24;++i)if(api->SetControl(0,"example/native","bra",i%2)!=HA_OK)threadFailure=true;});
    std::thread second([&]{for(int i=0;i<24;++i)if(api->SetControl(1,"example/native","panties",i%2)!=HA_OK)threadFailure=true;});
    std::thread reader([&]{for(int i=0;i<100;++i){HA_ControlValue v{};v.size=sizeof v;if(api->GetControl(i%2,"example/native",i%2?"panties":"bra",&v)!=HA_OK||v.value>1)threadFailure=true;}});
    first.join();second.join();reader.join();check(!threadFailure,"concurrent provider queries and saved-option edits remain valid");
    check(hex::initializeWardrobeAPI(file,error)&&api->GetSelection(3,&selected)==HA_OK&&!strcmp(selected.prefab,"test_ub_combination_2"),"native choices survive settings reload");
    check(client.Unregister(nativeHandle)==HA_OK&&!nativeHandle&&client.Unregister(nativeHandle)==HA_OK&&api->GetSelection(3,&selected)==HA_OK&&!selected.available,"helper shutdown is idempotent and preserves unavailable saved selection");
    HA_Control partControl=hexappeal::Sized<HA_Control>();partControl.type=HA_TOGGLE;partControl.defaultValue=1;partControl.flags=HA_FOLLOW_BASE_UNDERWEAR;
    hexappeal::Text(partControl.id,"bra");hexappeal::Text(partControl.label,"Own bra");
    HA_Outfit partOutfit=hexappeal::Sized<HA_Outfit>();partOutfit.controlCount=1;partOutfit.controls=&partControl;
    hexappeal::Text(partOutfit.id,"example/parts");hexappeal::Text(partOutfit.label,"Parts outfit");hexappeal::Text(partOutfit.prefab,"test_parts_dress");
    HA_VisibilityPart part=hexappeal::Sized<HA_VisibilityPart>();hexappeal::Text(part.control,"bra");hexappeal::Text(part.part,"OwnBra");
    HA_NativeOutfit partDefinition=hexappeal::Sized<HA_NativeOutfit>();partDefinition.outfit=&partOutfit;partDefinition.partCount=1;partDefinition.parts=&part;uint64_t partHandle=0;
    hexappeal::Text(part.part,"Underwear");check(client.RegisterNative(partDefinition,partHandle)==HA_INVALID&&!partHandle,"outfit bindings cannot target base underwear");hexappeal::Text(part.part,"OwnBra");
    partControl.scope=HA_GLOBAL;partControl.flags=0;check(client.RegisterNative(partDefinition,partHandle)==HA_INVALID,"native visibility cannot alter shared parts across witches");partControl.scope=HA_PER_WITCH;partControl.flags=HA_FOLLOW_BASE_UNDERWEAR;
    auto duplicateParts=std::array<HA_VisibilityPart,2>{part,part};partDefinition.partCount=2;partDefinition.parts=duplicateParts.data();
    check(client.RegisterNative(partDefinition,partHandle)==HA_INVALID,"duplicate native part bindings rejected");partDefinition.partCount=1;partDefinition.parts=&part;
    check(client.RegisterNative(partDefinition,partHandle)==HA_OK&&client.Available(partHandle,true)==HA_OK,"register visibility-only outfit without prefab combinations");
    hex::WardrobeProviderOutfit resolved;check(hex::resolveWardrobeProvider("example/parts",3,resolved,error)&&resolved.visibilityParts.size()==1&&resolved.visibilityParts[0].visible,"native parts default visible");
    check(base->SetHidden(3,1)==HA_OK&&hex::resolveWardrobeProvider("example/parts",3,resolved,error)&&!resolved.visibilityParts[0].visible,"base hide resolves native outfit visibility");
    check(base->SetHidden(3,0)==HA_OK&&api->SetControl(3,"example/parts","bra",0)==HA_OK&&hex::resolveWardrobeProvider("example/parts",3,resolved,error)&&!resolved.visibilityParts[0].visible,"independent part toggle remains off after base show");
    check(hex::resolveWardrobeProvider("example/parts",2,resolved,error)&&resolved.visibilityParts[0].visible,"native part toggles remain per witch");
    check(client.Unregister(partHandle)==HA_OK&&!hex::resolveWardrobeProvider("example/parts",3,resolved,error),"removed provider no longer supplies native part policy");
    HA_Control nudeControl=hexappeal::Sized<HA_Control>();nudeControl.type=HA_TOGGLE;nudeControl.defaultValue=1;nudeControl.flags=HA_FOLLOW_OUTFIT_HIDE;
    hexappeal::Text(nudeControl.id,"clothes");hexappeal::Text(nudeControl.label,"Clothes");
    partOutfit.controls=&nudeControl;hexappeal::Text(partOutfit.id,"example/nude");hexappeal::Text(part.control,"clothes");hexappeal::Text(part.part,"OwnClothes");uint64_t nudeHandle=0;
    nudeControl.scope=HA_GLOBAL;check(native->RegisterOutfit(&partDefinition,&nudeHandle)==HA_INVALID,"Hide outfit cannot hide globally shared parts");nudeControl.scope=HA_PER_WITCH;
    nudeControl.flags=HA_FOLLOW_BASE_UNDERWEAR|HA_FOLLOW_OUTFIT_HIDE;check(native->RegisterOutfit(&partDefinition,&nudeHandle)==HA_INVALID,"clothing and underwear cannot share a follow control");nudeControl.flags=HA_FOLLOW_OUTFIT_HIDE;
    check(native->RegisterOutfit(&partDefinition,&nudeHandle)==HA_OK&&api->SetAvailable(nudeHandle,1,"")==HA_OK,"explicit per-outfit Hide outfit opt-in");
    check(nude->SetHidden(3,1)==HA_OK&&nude->GetState(3,&nudeState)==HA_OK&&nudeState.hiddenMask==8&&nudeState.hidden,"Hide outfit preference is per witch");
    check(base->GetState(3,&state)==HA_OK&&!state.hidden,"Hide outfit does not rewrite Base underwear preference");
    check(hex::resolveWardrobeProvider("example/nude",3,resolved,error)&&!resolved.visibilityParts[0].visible,"Hide outfit masks opted-in native parts");
    check(base->GetEffectiveControl(3,"example/nude","clothes",&effective)==HA_OK&&!effective.value&&effective.storedValue==1,"effective renderer controls preserve original values");
    check(base->GetEffectiveControl(2,"example/nude","clothes",&effective)==HA_OK&&effective.value==1,"another witch is unchanged");
    check(base->GetEffectiveControl(3,"example/dress","panties",&effective)==HA_UNAVAILABLE,"unavailable provider stays unavailable under Hide outfit");
    HA_Control pairControls[2]={nudeControl,partControl};HA_VisibilityPart pairParts[2]={part,part};
    hexappeal::Text(pairParts[1].control,"bra");hexappeal::Text(pairParts[1].part,"OwnBra");
    auto pairOutfit=partOutfit;hexappeal::Text(pairOutfit.id,"example/independent-visibility");pairOutfit.controlCount=2;pairOutfit.controls=pairControls;
    auto pairDefinition=partDefinition;pairDefinition.outfit=&pairOutfit;pairDefinition.partCount=2;pairDefinition.parts=pairParts;uint64_t pairHandle=0;
    check(native->RegisterOutfit(&pairDefinition,&pairHandle)==HA_OK&&api->SetAvailable(pairHandle,1,"")==HA_OK,"register clothing and underwear as independent native bindings");
    for(int clothes=0;clothes<2;++clothes)for(int bra=0;bra<2;++bra){
        check(nude->SetHidden(3,clothes)==HA_OK&&base->SetHidden(3,bra)==HA_OK&&hex::resolveWardrobeProvider(pairOutfit.id,3,resolved,error),"select the independent visibility combination");
        check(resolved.visibilityParts[0].visible==!clothes&&resolved.visibilityParts[1].visible==!bra,"native clothing and underwear retain all four independent combinations");
        check(base->GetEffectiveControl(3,pairOutfit.id,"bra",&effective)==HA_OK&&effective.value==!bra&&effective.baseUnderwearHidden==unsigned(bra)&&effective.storedValue==1,"custom renderer receives only its matching underwear override");
    }
    check(base->SetHidden(3,0)==HA_OK&&nude->GetState(3,&nudeState)==HA_OK&&client.Unregister(pairHandle)==HA_OK,"restore the matrix fixture without changing saved garment values");
    auto ordinary=partOutfit;hexappeal::Text(ordinary.id,"example/ordinary");partDefinition.outfit=&ordinary;nudeControl.flags=HA_FOLLOW_BASE_UNDERWEAR;uint64_t ordinaryHandle=0;
    check(native->RegisterOutfit(&partDefinition,&ordinaryHandle)==HA_OK&&api->SetAvailable(ordinaryHandle,1,"")==HA_OK&&hex::resolveWardrobeProvider("example/ordinary",3,resolved,error)&&resolved.visibilityParts[0].visible,"an outfit without Hide outfit opt-in is unchanged");
    nudeControl.flags=HA_FOLLOW_OUTFIT_HIDE;partDefinition.outfit=&partOutfit;
    const auto nudeRevision=nudeState.revision;hex::rejectUnderwearSave=true;
    check(nude->SetHidden(3,0)==HA_UNAVAILABLE&&nude->GetState(3,&nudeState)==HA_OK&&nudeState.hidden&&nudeState.revision==nudeRevision,"failed Hide outfit save keeps state and revision");hex::rejectUnderwearSave=false;
    check(nude->SetHidden(3,0)==HA_OK&&hex::resolveWardrobeProvider("example/nude",3,resolved,error)&&resolved.visibilityParts[0].visible,"Hide outfit off restores the outfit's stored visibility");
    check(nude->SetHidden(-1,1)==HA_OK&&nude->GetState(-1,&nudeState)==HA_OK&&nudeState.hidden&&nudeState.hiddenMask==63,"All has six independent Hide outfit selections");
    check(nude->SetHidden(3,0)==HA_OK&&nude->GetState(-1,&nudeState)==HA_OK&&nudeState.mixed&&!nudeState.hidden,"mixed All visibility is observable");
    check(nude->SetHidden(6,1)==HA_INVALID&&nude->SetHidden(3,2)==HA_INVALID&&nude->GetState(-2,&nudeState)==HA_INVALID&&nude->GetState(3,reinterpret_cast<HA_OutfitVisibilityState*>(1))==HA_INVALID,"invalid Hide outfit calls fail safely");
    check(client.Attach(HexAppeal_GetWardrobeAPI,HexAppeal_GetBaseUnderwearAPI,HexAppeal_GetNativeWardrobeAPI,HexAppeal_RequestWardrobeRefreshV1,HexAppeal_GetOutfitVisibilityAPI)&&client.HasOutfitVisibility()&&client.HideOutfit(-1,false)==HA_OK&&client.OutfitVisibilityState(3,nudeState)==HA_OK&&!nudeState.hidden,"client discovers and controls optional Hide outfit extension");
    client.Reset();check(!client.Connected()&&!client.HasNativeCombinations()&&!client.HasOutfitVisibility(),"explicit helper reset clears capabilities");
    std::cout<<"PASS: original C ABI, native prefab combinations, complete-schema validation, linked bra/panties override/restoration, per-witch/All persistence, provider lifecycle, client capability discovery, concurrency, idempotency and failed-save preservation.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
