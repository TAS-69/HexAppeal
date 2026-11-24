#include "wardrobe_api.h"
#include "wardrobe_selection.h"
#include "wardrobe_native.h"
#include "underwear.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>

namespace hex {
namespace {
std::mutex registryMutex;
std::vector<WardrobeProviderOutfit> outfits;
std::map<std::string,uint32_t> values;
std::filesystem::path settingsFile;
uint64_t revision=1,nextToken=1;
thread_local std::string lastError;
int fail(int code,const std::string& message){lastError=message;return code;}
bool copy(void* output,const void* source,size_t size){__try{if(!output||!source)return false;memcpy(output,source,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool cstring(const char* source,char* output,size_t capacity){__try{if(!source)return false;for(size_t i=0;i<capacity;++i){output[i]=source[i];if(!output[i])return true;}return false;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<size_t N>bool fixed(const char(&source)[N],std::string& out,bool required=true){const auto end=static_cast<const char*>(memchr(source,0,N));if(!end)return false;out.assign(source,end);if(required&&out.empty())return false;for(unsigned char c:out)if(c<32||c==127)return false;return true;}
bool token(const std::string& value){if(value.empty()||value.size()>31)return false;for(unsigned char c:value)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;return true;}
bool prefab(const std::string& value){if(value.empty()||value.size()>127)return false;for(unsigned char c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
std::string key(const std::string& outfit,int witch,const std::string& control){return outfit+'|'+std::to_string(witch)+'|'+control;}
uint32_t valueFor(const WardrobeProviderOutfit& outfit,const WardrobeControl& control,int witch){const auto found=values.find(key(outfit.id,control.scope==HA_GLOBAL?-1:witch,control.id));const auto value=found==values.end()?control.defaultValue:found->second;return value<(control.type==HA_TOGGLE?2:control.options.size())?value:control.defaultValue;}
bool save(const std::map<std::string,uint32_t>& pending,std::string& error){
    if(settingsFile.empty()){error="Wardrobe options are not initialized yet.";return false;}
    const auto temporary=std::filesystem::path(settingsFile.wstring()+L".tmp");std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
    if(!out){error="Wardrobe options could not be saved.";return false;}
    out<<"HexAppealWardrobeControls=1\n";for(const auto& row:pending)out<<row.first<<'='<<row.second<<'\n';out.flush();const bool ok=bool(out);out.close();
    if(!ok||!MoveFileExW(temporary.c_str(),settingsFile.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="Wardrobe options could not be saved atomically.";return false;}return true;
}
bool parseDefinition(const HA_Outfit* pointer,WardrobeProviderOutfit& output){
    HA_Outfit input{};if(!copy(&input,pointer,sizeof input)||input.size!=sizeof input||input.controlCount>HA_WARDROBE_MAX_CONTROLS)return false;
    if(!fixed(input.id,output.id)||!validWardrobeId(output.id)||output.id.rfind("hexappeal/",0)==0||!fixed(input.label,output.label)||!fixed(input.prefab,output.prefab)||!prefab(output.prefab)||!fixed(input.credit,output.credit,false)||!fixed(input.requirements,output.requirements,false))return false;
    std::set<std::string> ids;unsigned nativeVariants=0;
    for(uint32_t i=0;i<input.controlCount;++i){
        HA_Control raw{};if(!input.controls||!copy(&raw,input.controls+i,sizeof raw)||raw.size!=sizeof raw||raw.type>HA_TOGGLE||raw.scope>HA_GLOBAL||(raw.flags&~(HA_PREFAB_VARIANT|HA_FOLLOW_BASE_UNDERWEAR|HA_FOLLOW_OUTFIT_HIDE)))return false;
        WardrobeControl control;control.type=raw.type;control.scope=raw.scope;control.flags=raw.flags;control.defaultValue=raw.defaultValue;
        if(!fixed(raw.id,control.id)||!token(control.id)||!ids.insert(control.id).second||!fixed(raw.label,control.label)||!fixed(raw.help,control.help,false))return false;
        if(raw.type==HA_TOGGLE){if(raw.optionCount||(raw.flags&HA_PREFAB_VARIANT)||raw.defaultValue>1||raw.flags==(HA_FOLLOW_BASE_UNDERWEAR|HA_FOLLOW_OUTFIT_HIDE)||((raw.flags&(HA_FOLLOW_BASE_UNDERWEAR|HA_FOLLOW_OUTFIT_HIDE))&&raw.scope!=HA_PER_WITCH))return false;}
        else{
            if(raw.flags&(HA_FOLLOW_BASE_UNDERWEAR|HA_FOLLOW_OUTFIT_HIDE))return false;
            if(!raw.optionCount||raw.optionCount>HA_WARDROBE_MAX_OPTIONS||raw.defaultValue>=raw.optionCount||!raw.options)return false;
            if(raw.flags==HA_PREFAB_VARIANT&&(raw.scope!=HA_PER_WITCH||++nativeVariants>1))return false;
            std::set<std::string> optionIds;
            for(uint32_t j=0;j<raw.optionCount;++j){HA_Option item{};WardrobeOption option;if(!copy(&item,raw.options+j,sizeof item)||!fixed(item.id,option.id)||!token(option.id)||!optionIds.insert(option.id).second||!fixed(item.label,option.label)||!fixed(item.prefab,option.prefab,false))return false;
                if(raw.flags==HA_PREFAB_VARIANT?!prefab(option.prefab):!option.prefab.empty())return false;control.options.push_back(std::move(option));}
        }output.controls.push_back(std::move(control));
    }return true;
}
int publishDefinition(WardrobeProviderOutfit next,uint64_t* result){
    std::lock_guard<std::mutex> lock(registryMutex);
    const auto previous=std::find_if(outfits.begin(),outfits.end(),[&](const auto& e){return e.id==next.id;});
    if(previous!=outfits.end()&&!previous->removed)return fail(HA_DUPLICATE,"This outfit ID is already registered.");
    if(previous==outfits.end()&&outfits.size()>=32)return fail(HA_LIMIT,"Wardrobe supports up to 32 provider outfits per session.");
    next.token=nextToken++;if(!copy(result,&next.token,sizeof next.token))return fail(HA_INVALID,"A writable registration token is required.");
    next.reason="Provider dependencies are not ready.";if(previous==outfits.end())outfits.push_back(std::move(next));else *previous=std::move(next);++revision;lastError.clear();return HA_OK;
}
int HA_CALL registerOutfit(const HA_Outfit* input,uint64_t* result){try{
    WardrobeProviderOutfit next;if(!parseDefinition(input,next))return fail(HA_INVALID,"Invalid outfit definition or unsupported control schema.");
    return publishDefinition(std::move(next),result);
}catch(...){return fail(HA_INVALID,"The outfit could not be registered.");}}
int HA_CALL registerNativeOutfit(const HA_NativeOutfit* pointer,uint64_t* result){try{
    HA_NativeOutfit input{};WardrobeProviderOutfit next;
    if(!copy(&input,pointer,sizeof input)||input.size!=sizeof input||!parseDefinition(input.outfit,next)||input.controlMask>=(1u<<next.controls.size())||input.stateCount>HA_WARDROBE_MAX_PREFAB_STATES||input.partCount>HA_WARDROBE_MAX_VISIBILITY_PARTS||(!input.controlMask&&!input.partCount)||(input.controlMask?(!input.stateCount||!input.states):(input.stateCount||input.states))||(input.partCount&&!input.parts))return fail(HA_INVALID,"Invalid native outfit definition.");
    uint32_t expected=1;
    for(size_t i=0;i<next.controls.size();++i){const auto& control=next.controls[i];
        if(input.controlMask&&(control.flags&HA_PREFAB_VARIANT))return fail(HA_INVALID,"Native combinations replace the single prefab-variant flag.");
        if(input.controlMask&(1u<<i)){
            if(control.scope!=HA_PER_WITCH)return fail(HA_INVALID,"Prefab combinations require per-witch controls.");
            expected*=control.type==HA_TOGGLE?2u:uint32_t(control.options.size());
            if(expected>HA_WARDROBE_MAX_PREFAB_STATES)return fail(HA_LIMIT,"Reduce the number of native combinations to 256 or fewer.");
        }
    }
    if(input.controlMask&&input.stateCount!=expected)return fail(HA_INVALID,"Supply every native control combination exactly once.");
    next.nativeMask=input.controlMask;std::set<std::array<uint32_t,HA_WARDROBE_MAX_CONTROLS>> keys;bool defaultMatches=!input.controlMask;
    for(uint32_t i=0;i<input.stateCount;++i){
        HA_PrefabState raw{};WardrobePrefabState state;
        if(!copy(&raw,input.states+i,sizeof raw)||raw.size!=sizeof raw||!fixed(raw.prefab,state.prefab)||!prefab(state.prefab))return fail(HA_INVALID,"Invalid native prefab state.");
        bool isDefault=true;
        for(size_t j=0;j<HA_WARDROBE_MAX_CONTROLS;++j){
            state.values[j]=raw.values[j];
            if(input.controlMask&(1u<<j)){
                const auto& c=next.controls[j];if(raw.values[j]>=(c.type==HA_TOGGLE?2:c.options.size()))return fail(HA_INVALID,"Native state value is outside its control range.");
                isDefault&=raw.values[j]==c.defaultValue;
            }else if(raw.values[j])return fail(HA_INVALID,"Unmapped native state values must be zero.");
        }
        if(!keys.insert(state.values).second)return fail(HA_INVALID,"Duplicate native control combination.");
        if(isDefault)defaultMatches=state.prefab==next.prefab;
        next.prefabStates.push_back(std::move(state));
    }
    if(!defaultMatches)return fail(HA_INVALID,"The outfit prefab must match its default control combination.");
    std::set<std::string> parts;
    for(uint32_t i=0;i<input.partCount;++i){HA_VisibilityPart raw{};std::string control,part;
        if(!copy(&raw,input.parts+i,sizeof raw)||raw.size!=sizeof raw||!fixed(raw.control,control)||!fixed(raw.part,part)||!prefab(part)||!parts.insert(part).second)return fail(HA_INVALID,"Invalid or duplicate native visibility part.");
        for(const char* reserved:{"Underwear","Nude","Body","Head","Face"})if(!_stricmp(part.c_str(),reserved))return fail(HA_INVALID,"Use an outfit-owned visibility part; base body parts are reserved.");
        const auto found=std::find_if(next.controls.begin(),next.controls.end(),[&](const auto& c){return c.id==control;});
        if(found==next.controls.end()||found->type!=HA_TOGGLE||found->scope!=HA_PER_WITCH)return fail(HA_INVALID,"Visibility parts require declared per-witch toggles.");
        const auto index=uint32_t(std::distance(next.controls.begin(),found));if(next.nativeMask&(1u<<index))return fail(HA_INVALID,"A toggle cannot drive both a prefab combination and a visibility part.");
        next.visibilityParts.push_back({index,part,true});
    }
    return publishDefinition(std::move(next),result);
}catch(...){return fail(HA_INVALID,"The native outfit could not be registered.");}}
int HA_CALL setAvailable(uint64_t handle,uint32_t available,const char* reason){try{
    char text[256]{};if(available>1||(reason&&!cstring(reason,text,sizeof text)))return fail(HA_INVALID,"Invalid availability update.");
    const std::string message=available?"":text[0]?text:"Provider dependencies are not ready.";
    std::lock_guard<std::mutex> lock(registryMutex);for(auto& entry:outfits)if(entry.token==handle&&!entry.removed){if(entry.available==(available!=0)&&entry.reason==message)return HA_OK;entry.available=available!=0;entry.reason=message;++revision;return HA_OK;}return fail(HA_NOT_FOUND,"The registration token is unknown.");
}catch(...){return fail(HA_INVALID,"Availability update failed.");}}
int HA_CALL bindItem(uint64_t handle,const char* visibilityControl){try{
    char visibility[32]{};
    if(!cstring(visibilityControl,visibility,sizeof visibility))return fail(HA_INVALID,"Provide a toggle ID or an empty string.");
    std::lock_guard<std::mutex> lock(registryMutex);
    const auto found=std::find_if(outfits.begin(),outfits.end(),[=](const auto& row){return row.token==handle&&!row.removed;});
    if(found==outfits.end())return fail(HA_NOT_FOUND,"The registration token is unknown.");
    auto& entry=*found;
    if(entry.available||!entry.itemSlot.empty())return fail(HA_UNAVAILABLE,"Bind an item before advertising availability.");
    const auto slot=outfitSlotForPart(entry.prefab);
    if(slot.empty()||protectedOutfitPart(entry.prefab)||entry.controls.empty()||entry.policyFlags||!entry.visibilityParts.empty())
        return fail(HA_INVALID,"Use an equipable prefab with item controls and no full-outfit policy or visibility bindings.");
    const auto fits=[&](const auto& name){return !protectedOutfitPart(name)&&outfitSlotForPart(name)==slot;};
    bool visibilityFound=!visibility[0];
    for(size_t index=0;index<entry.controls.size();++index){
        const auto& control=entry.controls[index];
        bool applied=(entry.nativeMask&(1u<<index))!=0;
        if(control.flags&HA_PREFAB_VARIANT){
            applied=true;
            if(control.options[control.defaultValue].prefab!=entry.prefab||
               std::any_of(control.options.begin(),control.options.end(),[&](const auto& option){return !fits(option.prefab);}))
                return fail(HA_INVALID,"Item variants must use the same slot and match the default prefab.");
        }
        if(control.id==visibility){
            if(applied||control.type!=HA_TOGGLE)return fail(HA_INVALID,"Item visibility needs its own toggle.");
            applied=visibilityFound=true;
        }
        if(!applied||control.scope!=HA_PER_WITCH)return fail(HA_INVALID,"Each item control must drive per-witch variants or item visibility.");
    }
    if(!visibilityFound||std::any_of(entry.prefabStates.begin(),entry.prefabStates.end(),[&](const auto& state){return !fits(state.prefab);}))
        return fail(HA_INVALID,"Check the visibility toggle ID and the slot of each item state.");
    if(std::any_of(outfits.begin(),outfits.end(),[&](const auto& row){return !row.removed&&!row.itemSlot.empty()&&row.prefab==entry.prefab;}))
        return fail(HA_DUPLICATE,"Another provider already binds this item prefab.");
    entry.itemSlot=slot;
    entry.itemVisibility=visibility;
    ++revision;
    lastError.clear();
    return HA_OK;
}catch(...){return fail(HA_INVALID,"The item controls could not be bound.");}}
int HA_CALL unregisterOutfit(uint64_t handle){std::lock_guard<std::mutex> lock(registryMutex);for(auto& entry:outfits)if(entry.token==handle&&!entry.removed){entry.removed=true;entry.available=false;entry.reason="Outfit provider is unloaded. Select Default to restore.";++revision;return HA_OK;}return fail(HA_NOT_FOUND,"The registration token is unknown.");}
int HA_CALL selectOutfit(int32_t witch,const char* source){try{char id[96]{};if(!cstring(source,id,sizeof id))return fail(HA_INVALID,"An outfit ID is required.");std::string error;if(!requestWardrobeOutfit(witch,id,error))return fail(HA_UNAVAILABLE,error);return HA_OK;}catch(...){return fail(HA_INVALID,"Outfit selection failed.");}}
int HA_CALL getSelection(int32_t witch,HA_Selection* output){try{
    uint32_t size=0;if(!copy(&size,output,4)||size!=sizeof(HA_Selection))return fail(HA_INVALID,"Set selection.size to sizeof(HA_Selection).");
    HA_Selection result{};result.size=sizeof result;result.witch=witch;std::string id,prefabName;bool available=false;uint64_t generation=0;
    if(!nativeWardrobeSelection(witch,id,prefabName,available,generation))return fail(HA_NOT_READY,"Choose witch 0 through 5 after Wardrobe initializes.");
    result.available=available;{std::lock_guard<std::mutex> lock(registryMutex);result.revision=(generation<<32)^revision;}
    strcpy_s(result.outfit,id.c_str());strcpy_s(result.prefab,prefabName.c_str());return copy(output,&result,sizeof result)?HA_OK:fail(HA_INVALID,"Selection output is not writable.");
}catch(...){return fail(HA_INVALID,"Selection query failed.");}}
int HA_CALL setControl(int32_t witch,const char* outfit,const char* control,uint32_t value){try{char id[96]{},name[32]{};if(!cstring(outfit,id,sizeof id)||!cstring(control,name,sizeof name))return fail(HA_INVALID,"Outfit and control IDs are required.");std::string error;if(!writeWardrobeControl(witch,id,name,value,error))return fail(HA_UNAVAILABLE,error);return HA_OK;}catch(...){return fail(HA_INVALID,"Control update failed.");}}
int HA_CALL getControl(int32_t witch,const char* outfit,const char* control,HA_ControlValue* output){try{
    uint32_t size=0;char id[96]{},name[32]{};if(!copy(&size,output,4)||size!=sizeof(HA_ControlValue)||!cstring(outfit,id,sizeof id)||!cstring(control,name,sizeof name))return fail(HA_INVALID,"Invalid control query.");
    HA_ControlValue result{};result.size=sizeof result;std::lock_guard<std::mutex> lock(registryMutex);
    for(const auto& entry:outfits)if(entry.id==id&&entry.itemSlot.empty())for(const auto& c:entry.controls)if(c.id==name){if(witch< -1||witch>5||(c.scope==HA_PER_WITCH&&witch<0))return fail(HA_INVALID,"Choose one witch for a per-witch query.");result.value=valueFor(entry,c,witch);result.scope=c.scope;result.revision=revision;return copy(output,&result,sizeof result)?HA_OK:HA_INVALID;}
    return fail(HA_NOT_FOUND,"The outfit control is not registered.");
}catch(...){return fail(HA_INVALID,"Control query failed.");}}
int HA_CALL getLastError(char* output,uint32_t capacity){if(!output||!capacity||capacity>65536)return HA_INVALID;const auto count=std::min<size_t>(lastError.size(),capacity-1);const char zero=0;if(!copy(output,lastError.c_str(),count)||!copy(output+count,&zero,1))return HA_INVALID;return count==lastError.size()?HA_OK:HA_LIMIT;}
int HA_CALL requestRefresh(int32_t witch,const char* source){try{char id[96]{};if(!cstring(source,id,sizeof id)||!validWardrobeId(id))return fail(HA_INVALID,"A valid outfit ID is required.");std::string error;if(!requestSelectedWardrobeRefresh(witch,id,error))return fail(HA_UNAVAILABLE,error);return HA_OK;}catch(...){return fail(HA_INVALID,"Outfit refresh failed.");}}
const HA_WardrobeAPI api{sizeof(HA_WardrobeAPI),HA_WARDROBE_ABI,registerOutfit,setAvailable,unregisterOutfit,selectOutfit,getSelection,setControl,getControl,getLastError};
int HA_CALL getBaseUnderwearState(int32_t witch,HA_BaseUnderwearState* output){try{
    uint32_t size=0;if(witch< -1||witch>5||!copy(&size,output,4)||size!=sizeof(HA_BaseUnderwearState))return fail(HA_INVALID,"Choose witch 0 through 5, or All, and set state.size.");
    {std::lock_guard<std::mutex> lock(registryMutex);if(settingsFile.empty())return fail(HA_NOT_READY,"Wardrobe is not initialized yet.");}
    const auto preference=baseUnderwearPreference();const auto native=underwearStatus(witch);const uint32_t targets=witch<0?63u:1u<<witch;
    HA_BaseUnderwearState result{};result.size=sizeof result;result.hiddenMask=preference.hiddenMask&63u;result.revision=preference.revision;
    result.hidden=(result.hiddenMask&targets)==targets;result.mixed=(result.hiddenMask&targets)&&!result.hidden;result.nativeReady=native.ready;result.nativePending=native.pending;
    return copy(output,&result,sizeof result)?HA_OK:fail(HA_INVALID,"Underwear state output is not writable.");
}catch(...){return fail(HA_INVALID,"Underwear state query failed.");}}
int HA_CALL setBaseUnderwearHidden(int32_t witch,uint32_t hidden){try{
    if(witch< -1||witch>5||hidden>1)return fail(HA_INVALID,"Choose a supported witch and boolean visibility.");
    {std::lock_guard<std::mutex> lock(registryMutex);if(settingsFile.empty())return fail(HA_NOT_READY,"Wardrobe is not initialized yet.");}
    std::string error;if(!setUnderwearHidden(witch,hidden!=0,error))return fail(HA_UNAVAILABLE,error);return HA_OK;
}catch(...){return fail(HA_INVALID,"Underwear preference could not be saved.");}}
int HA_CALL getEffectiveControl(int32_t witch,const char* outfit,const char* control,HA_EffectiveControlValue* output){try{
    uint32_t size=0;char id[96]{},name[32]{};
    if(witch<0||witch>5||!copy(&size,output,4)||size!=sizeof(HA_EffectiveControlValue)||!cstring(outfit,id,sizeof id)||!cstring(control,name,sizeof name))return fail(HA_INVALID,"Query one witch and set effective value.size.");
    HA_EffectiveControlValue result{};result.size=sizeof result;bool found=false;
    {std::lock_guard<std::mutex> lock(registryMutex);if(settingsFile.empty())return fail(HA_NOT_READY,"Wardrobe is not initialized yet.");
        for(const auto& entry:outfits)if(entry.id==id&&entry.itemSlot.empty()){
            if(!entry.available||entry.removed)return fail(HA_UNAVAILABLE,"The outfit provider is unavailable.");
            for(const auto& c:entry.controls)if(c.id==name){result.storedValue=valueFor(entry,c,witch);result.value=result.storedValue;result.scope=c.scope;result.flags=c.flags;result.revision=revision;found=true;break;}break;}}
    if(!found)return fail(HA_NOT_FOUND,"The outfit control is not registered.");
    const auto preference=baseUnderwearPreference();result.baseUnderwearRevision=preference.revision;result.baseUnderwearHidden=(preference.hiddenMask&(1u<<witch))!=0;
    const bool nude=(outfitHiddenPreference().hiddenMask&(1u<<witch))!=0;
    if((result.flags&HA_FOLLOW_BASE_UNDERWEAR)&&result.baseUnderwearHidden)result.value=0;
    if((result.flags&HA_FOLLOW_OUTFIT_HIDE)&&nude)result.value=0;
    return copy(output,&result,sizeof result)?HA_OK:fail(HA_INVALID,"Effective value output is not writable.");
}catch(...){return fail(HA_INVALID,"Effective visibility query failed.");}}
const HA_BaseUnderwearAPI baseUnderwearAPI{sizeof(HA_BaseUnderwearAPI),HA_BASE_UNDERWEAR_ABI,getBaseUnderwearState,setBaseUnderwearHidden,getEffectiveControl};
int HA_CALL getOutfitVisibilityState(int32_t witch,HA_OutfitVisibilityState* output){try{
    HA_OutfitVisibilityState result{};result.size=sizeof result;uint32_t size=0;
    if(!copy(&size,output,4)||size!=sizeof result)return fail(HA_INVALID,"Set state.size and provide a writable output.");
    const auto checked=getBaseUnderwearState(witch,&result);if(checked!=HA_OK)return checked;
    const auto preference=outfitHiddenPreference();const auto native=outfitHiddenStatus(witch);const uint32_t targets=witch<0?63u:1u<<witch;
    result.hiddenMask=preference.hiddenMask;result.revision=preference.revision;result.hidden=(result.hiddenMask&targets)==targets;result.mixed=(result.hiddenMask&targets)&&!result.hidden;result.nativeReady=native.ready;result.nativePending=native.pending;
    return copy(output,&result,sizeof result)?HA_OK:fail(HA_INVALID,"Hide outfit state output is not writable.");
}catch(...){return fail(HA_INVALID,"Hide outfit state query failed.");}}
int HA_CALL setOutfitVisibilityState(int32_t witch,uint32_t hidden){try{
    if(witch< -1||witch>5||hidden>1)return fail(HA_INVALID,"Choose a supported witch and boolean visibility.");
    {std::lock_guard<std::mutex> lock(registryMutex);if(settingsFile.empty())return fail(HA_NOT_READY,"Wardrobe is not initialized yet.");}
    std::string error;return setOutfitHidden(witch,hidden!=0,error)?HA_OK:fail(HA_UNAVAILABLE,error);
}catch(...){return fail(HA_INVALID,"Hide outfit preference could not be saved.");}}
const HA_OutfitVisibilityAPI outfitVisibilityAPI{sizeof(HA_OutfitVisibilityAPI),HA_OUTFIT_VISIBILITY_ABI,getOutfitVisibilityState,setOutfitVisibilityState};
int HA_CALL setOutfitPolicy(uint64_t token,uint32_t flags){try{
    if(flags&~HA_REPLACE_BASE_UNDERWEAR)return fail(HA_INVALID,"Unsupported outfit policy flags.");
    std::lock_guard<std::mutex> lock(registryMutex);
    for(auto& entry:outfits)if(entry.token==token&&!entry.removed){
        if(entry.available)return fail(HA_UNAVAILABLE,"Set the outfit policy before marking the provider available.");
        if(flags&&!entry.itemSlot.empty())return fail(HA_INVALID,"Full-outfit policy requires a full outfit registration.");
        if(entry.policyFlags!=flags){entry.policyFlags=flags;++revision;}lastError.clear();return HA_OK;
    }return fail(HA_NOT_FOUND,"The registration token is unknown.");
}catch(...){return fail(HA_INVALID,"The outfit policy could not be set.");}}
const HA_OutfitPolicyAPI outfitPolicyAPI{sizeof(HA_OutfitPolicyAPI),HA_OUTFIT_POLICY_ABI,setOutfitPolicy};
const HA_NativeWardrobeAPI nativeWardrobeAPI{sizeof(HA_NativeWardrobeAPI),HA_NATIVE_WARDROBE_ABI,registerNativeOutfit};
}
bool initializeWardrobeAPI(const std::filesystem::path& path,std::string& error){
    std::lock_guard<std::mutex> lock(registryMutex);std::map<std::string,uint32_t> loaded;std::error_code ec;
    if(std::filesystem::exists(path,ec)){
        if(std::filesystem::file_size(path,ec)>524288||ec){error="Wardrobe provider settings are too large or unreadable.";return false;}
        std::ifstream in(path,std::ios::binary);std::string line;if(!std::getline(in,line)||line!="HexAppealWardrobeControls=1"){error="Wardrobe provider settings have an unsupported header.";return false;}
        while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;const auto equal=line.find('='),first=line.find('|'),second=line.find('|',first==std::string::npos?first:first+1);
            if(equal==std::string::npos||first==std::string::npos||second==std::string::npos||!validWardrobeId(line.substr(0,first))||!token(line.substr(second+1,equal-second-1))||loaded.size()>=2048){error="Wardrobe provider settings contain an invalid row.";return false;}
            const auto scope=line.substr(first+1,second-first-1),number=line.substr(equal+1);if((scope!="-1"&&scope!="-2"&&(scope.size()!=1||scope[0]<'0'||scope[0]>'5'))||number.empty()||number.size()>2||number.find_first_not_of("0123456789")!=std::string::npos){error="Wardrobe provider settings contain an invalid value.";return false;}
            const auto value=uint32_t(std::stoul(number));if(value>=HA_WARDROBE_MAX_OPTIONS||!loaded.emplace(line.substr(0,equal),value).second){error="Wardrobe provider settings contain a duplicate or out-of-range choice.";return false;}
        }if(in.bad()){error="Wardrobe provider settings could not be read.";return false;}
    }else if(ec){error="Wardrobe provider settings are unavailable.";return false;}
    settingsFile=path;values=std::move(loaded);++revision;return true;
}
std::vector<WardrobeProviderOutfit> wardrobeProviderOutfits(){std::lock_guard<std::mutex> lock(registryMutex);return outfits;}
namespace {
bool matchesItem(const WardrobeProviderOutfit& entry,const OutfitSlots& slots){
    if(entry.itemSlot.empty()||entry.removed)return false;
    const auto found=slots.find(entry.itemSlot);
    return found!=slots.end()&&found->second.size()==1&&*found->second.begin()==entry.prefab;
}
uint32_t itemValue(const WardrobeProviderOutfit& entry,const WardrobeControl& control,const WardrobeItemOptions& options){
    const auto found=options.find({entry.id,control.id});
    if(found==options.end())return control.defaultValue;
    if(control.type==HA_TOGGLE)return found->second=="1"?1:found->second=="0"?0:control.defaultValue;
    const auto option=std::find_if(control.options.begin(),control.options.end(),[&](const auto& row){return row.id==found->second;});
    return option==control.options.end()?control.defaultValue:uint32_t(std::distance(control.options.begin(),option));
}
bool suppressedControl(const WardrobeControl& control,bool outfitHidden,bool underwearHidden){
    return (outfitHidden&&(control.flags&HA_FOLLOW_OUTFIT_HIDE))||
           (underwearHidden&&(control.flags&HA_FOLLOW_BASE_UNDERWEAR));
}
}
std::vector<WardrobeItemControl> wardrobeItemControls(const OutfitSlots& slots,const WardrobeItemOptions& options){
    std::vector<WardrobeItemControl> controls;
    if(slots.empty())return controls;
    std::lock_guard<std::mutex> lock(registryMutex);
    for(const auto& entry:outfits)if(matchesItem(entry,slots)){
        for(const auto& control:entry.controls)
            controls.push_back({entry.id,entry.label,entry.reason,entry.itemSlot,control,itemValue(entry,control,options),entry.available});
    }
    return controls;
}
WardrobeItemLook resolveWardrobeItems(const OutfitSlots& slots,const WardrobeItemOptions& options,bool outfitHidden,bool underwearHidden){
    WardrobeItemLook look{slots,{},{}};
    if(slots.empty())return look;
    std::lock_guard<std::mutex> lock(registryMutex);
    for(const auto& entry:outfits)if(entry.available&&matchesItem(entry,slots)){
        std::string_view prefabName=entry.prefab;
        bool visible=true, underwear=false;
        std::array<uint32_t,HA_WARDROBE_MAX_CONTROLS> state{};
        for(size_t index=0;index<entry.controls.size();++index){
            const auto& control=entry.controls[index];
            const auto value=suppressedControl(control,outfitHidden,underwearHidden)?0:itemValue(entry,control,options);
            if(entry.nativeMask&(1u<<index))state[index]=value;
            if(control.flags&HA_PREFAB_VARIANT)prefabName=control.options[value].prefab;
            if(control.id==entry.itemVisibility){
                visible=value!=0;
                underwear=(control.flags&HA_FOLLOW_BASE_UNDERWEAR)!=0;
            }
        }
        if(entry.nativeMask){
            const auto row=std::find_if(entry.prefabStates.begin(),entry.prefabStates.end(),[&](const auto& candidate){return candidate.values==state;});
            if(row==entry.prefabStates.end())continue;
            prefabName=row->prefab;
        }
        auto name=std::string(prefabName);
        if(!visible)look.hidden.insert(name);
        if(underwear)look.underwear.insert(name);
        look.slots.at(entry.itemSlot)={std::move(name)};
    }
    return look;
}
bool resolveWardrobeProvider(const std::string& id,int witch,WardrobeProviderOutfit& output,std::string& error){
    if(witch<0||witch>5){error="Choose one supported witch.";return false;}
    std::lock_guard<std::mutex> lock(registryMutex);for(const auto& entry:outfits)if(entry.id==id&&entry.itemSlot.empty()){output=entry;if(!entry.available||entry.removed){error=entry.reason;return false;}
        if((entry.policyFlags&HA_REPLACE_BASE_UNDERWEAR)&&!underwearStatus(witch).ready){error="This outfit requires the native Base underwear visibility route.";return false;}
        const bool nude=(outfitHiddenPreference().hiddenMask&(1u<<witch))!=0;
        const bool hidden=(baseUnderwearPreference().hiddenMask&(1u<<witch))!=0;
        if(entry.nativeMask){
            std::array<uint32_t,HA_WARDROBE_MAX_CONTROLS> state{};
            for(size_t i=0;i<entry.controls.size();++i)if(entry.nativeMask&(1u<<i)){const auto& control=entry.controls[i];state[i]=valueFor(entry,control,witch);if((hidden&&(control.flags&HA_FOLLOW_BASE_UNDERWEAR))||(nude&&(control.flags&HA_FOLLOW_OUTFIT_HIDE)))state[i]=0;}
            const auto found=std::find_if(entry.prefabStates.begin(),entry.prefabStates.end(),[&](const auto& item){return item.values==state;});
            if(found==entry.prefabStates.end()){error="The selected native outfit combination is unavailable.";return false;}
            output.prefab=found->prefab;
        }else for(const auto& control:entry.controls)if(control.flags==HA_PREFAB_VARIANT)output.prefab=control.options[valueFor(entry,control,witch)].prefab;
        for(auto& part:output.visibilityParts){const auto& control=entry.controls[part.control];part.visible=valueFor(entry,control,witch)!=0&&!(hidden&&(control.flags&HA_FOLLOW_BASE_UNDERWEAR))&&!(nude&&(control.flags&HA_FOLLOW_OUTFIT_HIDE));}
        return true;}error="Outfit provider is not installed or registered.";return false;
}
bool readWardrobeControl(int witch,const std::string& outfit,const std::string& control,uint32_t& value,uint64_t& stamp){HA_ControlValue out{};out.size=sizeof out;if(getControl(witch,outfit.c_str(),control.c_str(),&out)!=HA_OK)return false;value=out.value;stamp=out.revision;return true;}
bool writeWardrobeControl(int witch,const std::string& outfit,const std::string& control,uint32_t value,std::string& error){
    bool native=false;int affected=witch;{
        std::lock_guard<std::mutex> lock(registryMutex);if(witch< -1||witch>5){error="Choose a supported witch.";return false;}
        const auto entry=std::find_if(outfits.begin(),outfits.end(),[&](const auto& e){return e.id==outfit&&e.itemSlot.empty();});if(entry==outfits.end()||!entry->available||entry->removed){error="The outfit provider is unavailable.";return false;}
        const auto item=std::find_if(entry->controls.begin(),entry->controls.end(),[&](const auto& c){return c.id==control;});if(item==entry->controls.end()||value>=(item->type==HA_TOGGLE?2:item->options.size())){error="Choose a supported outfit option.";return false;}
        auto pending=values;if(item->scope==HA_GLOBAL){pending[key(outfit,-1,control)]=value;affected=-1;}else for(int i=0;i<6;++i)if(witch<0||witch==i)pending[key(outfit,i,control)]=value;
        if(pending.size()>2048){error="Wardrobe saved-option limit reached.";return false;}if(pending==values)return true;if(!save(pending,error))return false;values=std::move(pending);++revision;native=item->flags==HA_PREFAB_VARIANT||(entry->nativeMask&(1u<<std::distance(entry->controls.begin(),item)))!=0;
    }if(native)reapplyWardrobeProviderControls(affected,outfit);return true;
}
bool wardrobeControlPinned(const std::string& outfit,const std::string& control){std::lock_guard<std::mutex> lock(registryMutex);const auto found=values.find(key(outfit,-2,control));return found!=values.end()&&found->second==1;}
bool pinWardrobeControl(const std::string& outfit,const std::string& control,bool pinned,std::string& error){
    std::lock_guard<std::mutex> lock(registryMutex);for(const auto& entry:outfits)if(entry.id==outfit)for(const auto& item:entry.controls)if(item.id==control){auto pending=values;if(pinned)pending[key(outfit,-2,control)]=1;else pending.erase(key(outfit,-2,control));if(pending.size()>2048){error="Wardrobe saved-option limit reached.";return false;}if(!save(pending,error))return false;values=std::move(pending);return true;}error="The outfit control is not registered.";return false;
}
}
extern "C" __declspec(dllexport) const HA_WardrobeAPI* HA_CALL HexAppeal_GetWardrobeAPI(uint32_t version){return version==HA_WARDROBE_ABI?&hex::api:nullptr;}
extern "C" __declspec(dllexport) int HA_CALL HexAppeal_RequestWardrobeRefreshV1(int32_t witch,const char* outfit){return hex::requestRefresh(witch,outfit);}
extern "C" __declspec(dllexport) const HA_BaseUnderwearAPI* HA_CALL HexAppeal_GetBaseUnderwearAPI(uint32_t version){return version==HA_BASE_UNDERWEAR_ABI?&hex::baseUnderwearAPI:nullptr;}
extern "C" __declspec(dllexport) const HA_NativeWardrobeAPI* HA_CALL HexAppeal_GetNativeWardrobeAPI(uint32_t version){return version==HA_NATIVE_WARDROBE_ABI?&hex::nativeWardrobeAPI:nullptr;}
extern "C" __declspec(dllexport) const HA_OutfitVisibilityAPI* HA_CALL HexAppeal_GetOutfitVisibilityAPI(uint32_t version){return version==HA_OUTFIT_VISIBILITY_ABI?&hex::outfitVisibilityAPI:nullptr;}
extern "C" __declspec(dllexport) const HA_OutfitPolicyAPI* HA_CALL HexAppeal_GetOutfitPolicyAPI(uint32_t version){return version==HA_OUTFIT_POLICY_ABI?&hex::outfitPolicyAPI:nullptr;}
extern "C" __declspec(dllexport) int HA_CALL HexAppeal_BindWardrobeItemV1(uint64_t token,const char* visibilityControl){return hex::bindItem(token,visibilityControl);}
