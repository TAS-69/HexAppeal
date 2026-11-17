#include "wardrobe_native.h"
#include "wardrobe_snapshot.h"
#include "wardrobe_transport.h"
#include "wardrobe_api.h"
#include "wardrobe_refresh.h"
#include "wardrobe_nude.h"
#include "runtime.h"
#include "underwear.h"
#include "core/log.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <fstream>
#include <map>
#include <set>

namespace hex {
namespace {
WardrobeSelection selections;
std::mutex mutex;
bool engineReady=false,writesAllowed=false,faulted=false;
std::array<uint64_t,6> armed{};
std::array<uint64_t,6> appliedGeneration{};
std::array<std::string,6> witchDetail{};
std::array<std::string,6> choiceErrors{};
std::string detail="Wardrobe integration is not ready.";
uintptr_t base=0;
struct Owned {int witch=-1;uintptr_t node=0;uint64_t attempted=0,observedAt=0;bool installed=false,pending=false;DWORD deadline=0;std::string original,current,requested,removed,refreshOutfit,desired;WardrobeRefreshCycle refresh;WardrobeNudeCycle nude;std::vector<WardrobeRecord> nudeRecords;std::vector<std::string> nudeNames;};
std::map<uintptr_t,Owned> owned;
uintptr_t ptr(uintptr_t at){__try{return at?*reinterpret_cast<const uintptr_t*>(at):0;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
uint32_t num(uintptr_t at){__try{return at?*reinterpret_cast<const uint32_t*>(at):0;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
bool read(uintptr_t at,void* out,size_t size){__try{std::memcpy(out,reinterpret_cast<const void*>(at),size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool text(uintptr_t wrapper,char* out,size_t capacity){
    const auto count=num(wrapper+8);const auto string=ptr(wrapper);if(!string||!count||count>=capacity||!read(string,out,count))return false;
    for(uint32_t i=0;i<count;++i)if(static_cast<unsigned char>(out[i])<32||static_cast<unsigned char>(out[i])>126)return false;
    out[count]=0;return true;
}
bool bytes(uintptr_t at,const unsigned char* expected,size_t count){unsigned char actual[32]{};return count<=sizeof actual&&read(at,actual,count)&&!memcmp(actual,expected,count);}
bool retain(uintptr_t wrapper){__try{auto* refs=reinterpret_cast<volatile LONG*>(wrapper+16);const auto value=*refs;if(value<0)return true;if(value<1||value>0x1000000)return false;InterlockedIncrement(refs);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool release(uintptr_t* wrapper){__try{reinterpret_cast<void(*)(uintptr_t*)>(base+0x3e6f50)(wrapper);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool destroy(WardrobeRecord* record){__try{reinterpret_cast<void(*)(void*)>(base+0x536660)(record);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool pipeline(uintptr_t node,WardrobeVector* removals,WardrobeVector* additions,uint8_t flag){
    __try{reinterpret_cast<intptr_t(*)(uintptr_t,void*,void*,uint8_t)>(base+0x2f8c200)(node,removals,additions,flag);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool rebuild(uintptr_t node,const WardrobeVector* ready,WardrobeVector* removals,WardrobeVector* additions,uint8_t flag){return rebuildWardrobePipeline(node,ready,removals,additions,flag,pipeline);}
uintptr_t canonical(const char* name){
    const auto loader=ptr(base+0x7160288),table=loader+0x50;const auto count=num(table+4),capacity=num(table+8);const auto entries=ptr(table+0x18);
    if(!loader||!entries||count>capacity||count>100000)return 0;
    for(uint32_t i=0;i<count;++i){const auto entry=ptr(entries+i*8),wrapper=ptr(entry+8);char candidate[192]{};
        if(text(wrapper,candidate,sizeof candidate)&&!strcmp(candidate,name))return wrapper;}
    return 0;
}
bool readyRecord(const WardrobeSnapshot& snapshot,const char* expected,WardrobeRecord& record){
    const auto data=ptr(snapshot.node+0x40);const auto count=num(snapshot.node+0x48),capacity=num(snapshot.node+0x4c);
    if(!data||count>64||count>capacity)return false;
    for(uint32_t i=0;i<count;++i){WardrobeRecord candidate;const auto address=data+i*0x70;char name[192]{};
        if(!read(address,&candidate,sizeof candidate))return false;
        uintptr_t wrapper=0;memcpy(&wrapper,candidate.bytes.data(),8);
        if(text(wrapper,name,sizeof name)&&!strcmp(name,expected)&&isSimpleWardrobeRecord(candidate,base)){
            if(ptr(snapshot.node+0x40)!=data||num(snapshot.node+0x48)!=count)return false;
            record=candidate;return true;}
    }return false;
}
bool readyRecords(const WardrobeSnapshot& snapshot,std::vector<WardrobeRecord>& records){
    const auto data=ptr(snapshot.node+0x40);const auto count=num(snapshot.node+0x48),capacity=num(snapshot.node+0x4c);
    if(!data||!count||count>64||count>capacity)return false;
    records.resize(count);if(!read(data,records.data(),count*sizeof(WardrobeRecord)))return false;
    for(const auto& record:records)if(!isSimpleWardrobeRecord(record,base))return false;
    return ptr(snapshot.node+0x40)==data&&num(snapshot.node+0x48)==count;
}
std::string defaultGarment(const WardrobeSnapshot& snapshot){for(uint32_t i=0;i<snapshot.claimCount;++i)if(snapshot.claims[i].kind==WardrobePartKind::DefaultGarment)return snapshot.claims[i].prefab;return {};}
WardrobeCapabilities capabilities(){WardrobeCapabilities result;result.switchReady=engineReady&&writesAllowed&&!faulted;return result;}
uint32_t refreshDelay(){const auto ini=Runtime::get().folder/L"HexAppeal.ini";return std::clamp<uint32_t>(GetPrivateProfileIntW(L"Wardrobe",L"RefreshDelayMs",500,ini.c_str()),100,10000);}
void failWardrobe(const char* message){faulted=true;detail=message;LOG("Wardrobe stopped: %s",message);Runtime::get().reportCompatibility(Feature::Wardrobe,CompatibilityState::Blocked,detail);}
bool releaseNudeRecords(Owned& entry){
    bool ok=true;for(const auto& record:entry.nudeRecords){uintptr_t wrapper=0;std::memcpy(&wrapper,record.bytes.data(),sizeof wrapper);if(!release(&wrapper))ok=false;}
    entry.nudeRecords.clear();entry.nudeNames.clear();entry.nude={};return ok;
}
bool defaultNude(uintptr_t actor,int witch,const WardrobeSnapshot& snapshot,Owned& entry,bool requested,bool accessoryOnly,std::string& status){
    const auto now=GetTickCount64();const auto present=[&](const std::string& name){for(uint32_t i=0;i<snapshot.claimCount;++i)if(name==snapshot.claims[i].prefab)return true;return false;};
    bool any=false,all=!entry.nudeNames.empty(),outfitPresent=false;for(const auto& name:entry.nudeNames){const bool found=present(name);any|=found;all&=found;}
    for(uint32_t i=0;i<snapshot.claimCount;++i)outfitPresent|=defaultOutfitPart(witch,snapshot.claims[i].prefab);
    if(snapshot.customGarment&&(!entry.installed||!present(entry.current))){
        if(!releaseNudeRecords(entry)){faulted=true;detail="Outfit references could not be released.";}
        setOutfitHiddenActor(actor,snapshot.renderer,witch,false,!requested);status="Custom outfit retained";return false;
    }
    if(entry.nude.pending()){
        const bool settled=entry.nude.observe(any,all,now);
        if(entry.nude.phase==WardrobeNudeCycle::Failed){failWardrobe("Hide outfit outfit change was not acknowledged. Restart before retrying.");status=detail;return true;}
        if(settled){Runtime::get().requestShapeReapply(witch);LOG("Hide outfit assembly acknowledged witch=%d hidden=%d.",witch,entry.nude.phase==WardrobeNudeCycle::Nude);}
        if(entry.nude.phase==WardrobeNudeCycle::Dressed){if(!releaseNudeRecords(entry)){faulted=true;detail="Outfit references could not be released.";}setOutfitHiddenActor(actor,snapshot.renderer,witch,false,true);status="Outfit / active";return false;}
        if(entry.nude.pending()){setOutfitHiddenActor(actor,snapshot.renderer,witch,requested,false);status="Hide outfit / refreshing";return true;}
    }
    if(!requested&&entry.nude.phase==WardrobeNudeCycle::Nude){
        WardrobeSnapshot fresh;if(!captureWardrobeSnapshot(actor,witch,base,fresh)||fresh.node!=snapshot.node||fresh.claimData!=snapshot.claimData||fresh.claimCount!=snapshot.claimCount)return true;
        std::vector<WardrobeRecord> ready;if(!readyRecords(snapshot,ready)){status="Hide outfit / waiting for outfit records";return true;}
        const auto result=invokeWardrobeRebuild(snapshot.node,ready,{},entry.nudeRecords,base,{retain,release,destroy,pipeline,rebuild});
        if(result!=WardrobeCallResult::Completed){failWardrobe("Hide outfit outfit restoration failed. Restart before retrying.");status=detail;return true;}
        entry.nude.restoring(now);setOutfitHiddenActor(actor,snapshot.renderer,witch,false,false);status="Hide outfit / restoring outfit";LOG("Hide outfit restore call witch=%d records=%zu.",witch,entry.nudeRecords.size());return true;
    }
    if(!requested){setOutfitHiddenActor(actor,snapshot.renderer,witch,false,true);return false;}
    if(entry.nude.phase==WardrobeNudeCycle::Nude&&!outfitPresent){setOutfitHiddenActor(actor,snapshot.renderer,witch,true,true);status="Hide outfit / active";return !accessoryOnly;}
    if(entry.nude.phase==WardrobeNudeCycle::Dressed&&!snapshot.defaultGarment&&!accessoryOnly){setOutfitHiddenActor(actor,snapshot.renderer,witch,false,false);status="Hide outfit / waiting for default outfit";return true;}
    std::vector<WardrobeRecord> removals;std::set<std::string> names;
    for(uint32_t i=0;i<snapshot.claimCount;++i){const auto& claim=snapshot.claims[i];if(!defaultOutfitPart(witch,claim.prefab)||!names.insert(claim.prefab).second)continue;
        if(removals.size()>=8){status="Hide outfit / outfit part limit reached";return true;}
        WardrobeRecord record;if(!readyRecord(snapshot,claim.prefab,record)){status="Hide outfit / waiting for outfit records";return true;}removals.push_back(record);
    }
    if(removals.empty()){setOutfitHiddenActor(actor,snapshot.renderer,witch,accessoryOnly,accessoryOnly);status=accessoryOnly?"Hide outfit / active":"Hide outfit / waiting for outfit";return !accessoryOnly;}
    for(const auto& name:names)if(std::find(entry.nudeNames.begin(),entry.nudeNames.end(),name)==entry.nudeNames.end()){
        WardrobeRecord record;if(!readyRecord(snapshot,name.c_str(),record)){status="Hide outfit / waiting for outfit records";return true;}
        uintptr_t wrapper=0;std::memcpy(&wrapper,record.bytes.data(),sizeof wrapper);if(!retain(wrapper)){status="Hide outfit / outfit reference unavailable";return true;}
        entry.nudeRecords.push_back(record);entry.nudeNames.push_back(name);
    }
    WardrobeSnapshot fresh;if(!captureWardrobeSnapshot(actor,witch,base,fresh)||fresh.node!=snapshot.node||fresh.claimData!=snapshot.claimData||fresh.claimCount!=snapshot.claimCount)return true;
    if(!restoreOutfitVisibility(reinterpret_cast<void*>(actor))){status="Hide outfit / waiting for garment visibility";return true;}
    std::vector<WardrobeRecord> ready;if(!readyRecords(snapshot,ready)){status="Hide outfit / unsupported outfit record layout";return true;}
    const auto result=invokeWardrobeRebuild(snapshot.node,ready,removals,{},base,{retain,release,destroy,pipeline,rebuild});
    if(result!=WardrobeCallResult::Completed){failWardrobe("Hide outfit outfit removal failed. Restart before retrying.");status=detail;return true;}
    entry.nude.removing(now);setOutfitHiddenActor(actor,snapshot.renderer,witch,true,false);status="Hide outfit / refreshing";LOG("Hide outfit removal call witch=%d records=%zu.",witch,removals.size());return true;
}
}
bool initializeNativeWardrobe(std::string& error){
    auto& runtime=Runtime::get();std::lock_guard<std::mutex> lock(mutex);
    engineReady=false;
    base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(runtime.profiles.empty()){error="Wardrobe settings folder is unavailable.";detail=error;return false;}
    if(!selections.load(runtime.profiles.parent_path()/L"wardrobe.ini",error)){detail=error;return false;}
    if(!initializeWardrobeAPI(runtime.profiles.parent_path()/L"wardrobe-controls.ini",error)){detail=error;return false;}
    const auto ini=runtime.folder/L"HexAppeal.ini";
    writesAllowed=GetPrivateProfileIntW(L"Development",L"EnableWardrobeWrites",0,ini.c_str())!=0;
    const unsigned char pipelineBytes[]={0x48,0x89,0x5c,0x24,0x10,0x44,0x88,0x4c,0x24,0x20,0x4c,0x89,0x44,0x24,0x18};
    const unsigned char destroyBytes[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
    const unsigned char releaseBytes[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20};
    engineReady=bytes(base+0x2f8c200,pipelineBytes,sizeof pipelineBytes)&&bytes(base+0x536660,destroyBytes,sizeof destroyBytes)&&bytes(base+0x3e6f50,releaseBytes,sizeof releaseBytes);
    detail=!engineReady?"The game is present, but its outfit functions could not be located by this adapter.":!writesAllowed?"Wardrobe writes are disabled in this development build.":"Native Wardrobe ready. Install a provider outfit to add replacements.";
    if(!engineReady){error=detail;return false;}
    armed=selections.snapshot().generations;
    LOG("Saved Wardrobe choices queued; application waits for live witch ownership, outfit records and dependencies.");
    runtime.reportCompatibility(Feature::Wardrobe,writesAllowed?CompatibilityState::Ready:CompatibilityState::Blocked,writesAllowed?"Native outfit functions and assembly observation checks passed.":"Wardrobe writes are disabled in this development configuration.");
    return true;
}
void reportWardrobeUnavailable(const std::string& error){
    std::lock_guard<std::mutex> lock(mutex);engineReady=false;detail=error;
    Runtime::get().reportCompatibility(Feature::Wardrobe,CompatibilityState::Blocked,error);
    const auto& profiles=Runtime::get().profiles;std::string ignored;
    if(!profiles.empty())selections.load(profiles.parent_path()/L"wardrobe.ini",ignored);
}
void tickNativeWardrobe(uintptr_t actor,int witch){
    if(witch<0||witch>5)return;
    const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);
    if(!engineReady||!writesAllowed||faulted||armed[witch]!=selected.generations[witch])return;
    const auto choiceId=wardrobeOutfitId(selected.choices[witch]);bool isDefault=choiceId=="hexappeal/default";
    WardrobeSnapshot snapshot;if(!captureWardrobeSnapshot(actor,witch,base,snapshot))return;
    auto& status=witchDetail[witch];
    std::string desired;
    if(!isDefault){WardrobeProviderOutfit provider;std::string error;if(!resolveWardrobeProvider(choiceId,witch,provider,error)){status=error;return;}desired=provider.prefab;}
    const auto has=[&](const std::string& name){for(uint32_t i=0;i<snapshot.claimCount;++i)if(name==snapshot.claims[i].prefab)return true;return false;};
    if(owned.find(actor)==owned.end()&&owned.size()>=64){status="Instance limit reached; reload before adding more.";return;}auto& entry=owned[actor];
    if(entry.node!=snapshot.node||entry.witch!=witch){if(!releaseNudeRecords(entry)){faulted=true;detail="Outfit references could not be released.";return;}entry={};entry.node=snapshot.node;entry.witch=witch;entry.observedAt=GetTickCount64();appliedGeneration[witch]=0;}
    if(GetTickCount64()-entry.observedAt<250){status="Outfit / waiting for witch";return;}
    const bool nudeRequested=(Runtime::get().outfitHiddenMask.load()&(1u<<witch))!=0;
    bool providerNude=false;if(!isDefault&&nudeRequested&&entry.installed&&!entry.pending&&has(entry.current)){WardrobeProviderOutfit provider;std::string ignored;if(resolveWardrobeProvider(choiceId,witch,provider,ignored))providerNude=std::any_of(provider.controls.begin(),provider.controls.end(),[](const auto& c){return (c.flags&HA_FOLLOW_OUTFIT_HIDE)!=0;});}
    if(entry.nude.phase!=WardrobeNudeCycle::Dressed||(isDefault&&nudeRequested&&!entry.installed&&!entry.pending)||providerNude){
        if(defaultNude(actor,witch,snapshot,entry,(isDefault&&nudeRequested&&!entry.installed)||providerNude,providerNude,status))return;
    }
    setOutfitHiddenActor(actor,snapshot.renderer,witch,providerNude,!nudeRequested||providerNude);
    if(entry.refresh.pending()&&entry.refreshOutfit==choiceId)entry.refresh.generation=selected.generations[witch];else entry.refresh.sync(selected.generations[witch]);
    if(entry.pending){
        const bool observed=has(entry.requested)&&!has(entry.removed);
        if(observed){entry.pending=false;entry.installed=entry.requested!=entry.original;entry.current=entry.installed?entry.requested:"";const bool transition=entry.refresh.acknowledged(entry.installed,GetTickCount64());appliedGeneration[witch]=entry.refresh.pending()?0:entry.attempted;if(transition)entry.attempted=0;status=entry.refresh.pending()?"Outfit / refreshing":"Outfit / active";if(!entry.refresh.pending()){Runtime::get().requestShapeReapply(witch);LOG("Wardrobe final assembly acknowledged; saved shape refit queued for witch=%d.",witch);}LOG("Wardrobe switch acknowledged witch=%d actor=%p node=%p prefab=%s.",witch,reinterpret_cast<void*>(actor),reinterpret_cast<void*>(snapshot.node),entry.requested.c_str());}
        else if(int32_t(GetTickCount()-entry.deadline)>=0){entry.pending=false;faulted=true;std::string ignored;if(entry.refresh.phase==WardrobeRefreshCycle::Idle)selections.change(witch,WardrobeField::Outfit,0,{true},ignored);detail="Outfit call completed, but the expected assembly was not observed. Reload before retrying.";}
        return;
    }
    bool custom=false;for(uint32_t i=0;i<snapshot.claimCount;++i){const auto& claim=snapshot.claims[i];if(std::strstr(claim.prefab,"_ub_")&&claim.kind!=WardrobePartKind::DefaultGarment&&(!entry.installed||entry.current!=claim.prefab))custom=true;}
    if(custom){appliedGeneration[witch]=0;status="Custom outfit retained";return;}
    if(entry.desired!=desired){entry.desired=desired;entry.attempted=0;appliedGeneration[witch]=0;}
    if(entry.installed&&!has(entry.current)&&snapshot.defaultGarment){entry.installed=false;entry.current.clear();entry.attempted=0;appliedGeneration[witch]=0;}
    if(!isDefault&&entry.refresh.begin(entry.installed&&entry.current==desired))entry.attempted=0;
    if(entry.refresh.phase==WardrobeRefreshCycle::Original)isDefault=true;
    if(!entry.refresh.readyToRestore(GetTickCount64())){status="Outfit / refreshing";return;}
    if((!isDefault&&entry.installed&&entry.current==desired&&has(desired)&&!snapshot.defaultGarment)||(!entry.installed&&snapshot.defaultGarment&&(isDefault||has(desired)))){
        entry.attempted=selected.generations[witch];appliedGeneration[witch]=entry.attempted;status="Outfit / active";return;
    }
    appliedGeneration[witch]=0;
    if(entry.attempted==selected.generations[witch]){status="Waiting for outfit assembly";return;}
    if(isDefault&&!entry.installed){status="External outfit retained";return;}
    const auto original=entry.installed?entry.original:defaultGarment(snapshot);
    if(original.empty()||(!entry.installed&&!snapshot.defaultGarment)||(entry.installed&&!has(entry.current))){status="Waiting for a default outfit";return;}
    const auto sourceName=entry.installed?entry.current:original,targetName=isDefault?original:desired;
    const auto* source=sourceName.c_str();const auto* target=targetName.c_str();


    WardrobeRecord record;if(!readyRecord(snapshot,original.c_str(),record)){status="Original outfit record unavailable";return;}
    const auto removeWrapper=canonical(source),addWrapper=canonical(target);if(!removeWrapper||!addWrapper){status="Waiting for outfit registry";return;}
    std::memcpy(record.bytes.data(),&removeWrapper,sizeof removeWrapper);
    WardrobeSnapshot fresh;if(!captureWardrobeSnapshot(actor,witch,base,fresh)||fresh.node!=snapshot.node||fresh.claimData!=snapshot.claimData||fresh.claimCount!=snapshot.claimCount)return;
    bool sourceStillPresent=false;for(uint32_t i=0;i<fresh.claimCount;++i){const auto& claim=fresh.claims[i];sourceStillPresent|=sourceName==claim.prefab;if(std::strstr(claim.prefab,"_ub_")&&claim.kind!=WardrobePartKind::DefaultGarment&&(!entry.installed||entry.current!=claim.prefab))return;}
    if(!sourceStillPresent)return;
    if(!restoreOutfitVisibility(reinterpret_cast<void*>(actor))){status="Outfit / waiting to restore garment visibility";return;}
    entry.attempted=selected.generations[witch];
    entry.refresh.applyingSelected();
    if(entry.refresh.phase==WardrobeRefreshCycle::Selected)LOG("Wardrobe refresh restore witch=%d delay_ms=%u observed_wait_ms=%llu.",witch,entry.refresh.delayMs,static_cast<unsigned long long>(GetTickCount64()-(entry.refresh.restoreAt-entry.refresh.delayMs)));
    const auto result=invokeWardrobeSwap(snapshot.node,record,addWrapper,base,{retain,release,destroy,pipeline});
    LOG("Wardrobe switch call witch=%d actor=%p node=%p source=%s target=%s result=%d thread=%u.",witch,reinterpret_cast<void*>(actor),reinterpret_cast<void*>(snapshot.node),source,target,int(result),GetCurrentThreadId());
    if(result!=WardrobeCallResult::Completed){faulted=true;std::string ignored;if(entry.refresh.phase==WardrobeRefreshCycle::Idle)selections.change(witch,WardrobeField::Outfit,0,{true},ignored);detail="The outfit call failed. Reload before retrying.";return;}
    entry.original=original;entry.removed=sourceName;entry.requested=targetName;
    entry.pending=true;entry.deadline=GetTickCount()+5000;status="Saved / waiting for outfit assembly";
}
WardrobeEditorStatus wardrobeEditorStatus(int target){
    const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);WardrobeEditorStatus result;result.available=capabilities();result.detail=detail;
    if(faulted)Runtime::get().reportCompatibility(Feature::Wardrobe,CompatibilityState::Blocked,detail);
    if(target< -1||target>5){result.available={};return result;}result.selected=selected.choices[target<0?0:target];
    if(target<0)for(const auto& choice:selected.choices){result.mixedOutfit|=wardrobeOutfitId(choice)!=wardrobeOutfitId(result.selected);}
    if(result.available.switchReady){
        result.warning=false;
        if(target>=0){
            result.detail=armed[target]!=selected.generations[target]?"Outfit / awaiting selection":
                witchDetail[target].empty()?"Outfit / waiting for witch":witchDetail[target];
        }else{
            unsigned active=0;for(size_t i=0;i<6;++i)if(armed[i]==selected.generations[i]&&appliedGeneration[i]==selected.generations[i])++active;
            result.detail="Outfits / "+std::to_string(active)+" active, "+std::to_string(6-active)+" waiting";
        }
    }
    if(result.available.switchReady)result.caption=result.detail;
    if(result.selected.outfit==WitchOutfit::Legacy){result.caption="Saved outfit / unavailable";result.detail="This saved legacy replacement is not included. Choose Default or install a compatible provider.";result.warning=true;}
    for(int witch=0;witch<6;++witch)if((target<0||target==witch)&&!choiceErrors[witch].empty()){
        result.caption="Selection not saved";result.detail=choiceErrors[witch];result.warning=true;break;
    }
    return result;
}
bool changeWardrobeChoice(int target,WardrobeField field,int value,std::string& error){
    const auto status=wardrobeEditorStatus(target);if(!selections.change(target,field,value,status.available,error)){
        std::lock_guard<std::mutex> lock(mutex);for(int witch=0;witch<6;++witch)if(target<0||target==witch)choiceErrors[witch]=error;
        LOG_ERR("Wardrobe selection rejected target=%d field=%d: %s",target,int(field),error.c_str());return false;
    }
    const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);
    for(int witch=0;witch<6;++witch)if(target<0||target==witch){armed[witch]=selected.generations[witch];choiceErrors[witch].clear();witchDetail[witch]="Outfit / waiting for witch";}
    return true;
}
bool requestWardrobeOutfit(int witch,const std::string& id,std::string& error){
    if(id=="hexappeal/default")return changeWardrobeChoice(witch,WardrobeField::Outfit,0,error);
    if(witch< -1||witch>5){error="Choose a supported witch.";return false;}
    const auto status=wardrobeEditorStatus(witch);if(!status.available.switchReady){error=status.detail;return false;}
    for(int i=0;i<6;++i)if(witch<0||witch==i){WardrobeProviderOutfit provider;if(!resolveWardrobeProvider(id,i,provider,error))return false;}
    if(!selections.providerOutfit(witch,id,error))return false;
    const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);for(int i=0;i<6;++i)if(witch<0||witch==i){armed[i]=selected.generations[i];choiceErrors[i].clear();witchDetail[i]="Outfit / waiting for witch";}return true;
}
bool nativeWardrobeSelection(int witch,std::string& id,std::string& prefab,bool& available,uint64_t& revision){
    if(witch<0||witch>5)return false;const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);if(!engineReady)return false;
    id=wardrobeOutfitId(selected.choices[witch]);revision=selected.generations[witch];available=capabilities().switchReady;
    if(id!="hexappeal/default"){WardrobeProviderOutfit provider;std::string ignored;const bool ready=resolveWardrobeProvider(id,witch,provider,ignored);prefab=provider.prefab;available=available&&ready;}
    return true;
}
void reapplyWardrobeProviderControls(int witch,const std::string& id){
    selections.invalidate(witch,id);const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);for(int i=0;i<6;++i)if((witch<0||witch==i)&&wardrobeOutfitId(selected.choices[i])==id){armed[i]=selected.generations[i];choiceErrors[i].clear();witchDetail[i]="Outfit / waiting for variant";}
    for(auto& row:owned){auto& entry=row.second;if(entry.witch<0||entry.witch>5||(witch>=0&&witch!=entry.witch)||wardrobeOutfitId(selected.choices[entry.witch])!=id||!entry.installed)continue;WardrobeProviderOutfit provider;std::string ignored;if(resolveWardrobeProvider(id,entry.witch,provider,ignored)&&provider.prefab==entry.current){entry.refreshOutfit=id;entry.refresh.request(selected.generations[entry.witch],GetTickCount64(),refreshDelay());}}
}
bool requestSelectedWardrobeRefresh(int witch,const std::string& id,std::string& error){
    if(witch< -1||witch>5){error="Choose one witch or All witches.";return false;}
    const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);
    if(!engineReady||!writesAllowed||faulted){error=detail;return false;}
    unsigned queued=0;for(auto& row:owned){auto& entry=row.second;if(entry.witch<0||entry.witch>5||(witch>=0&&entry.witch!=witch)||wardrobeOutfitId(selected.choices[entry.witch])!=id||(!entry.installed&&entry.refresh.phase==WardrobeRefreshCycle::Idle))continue;entry.refreshOutfit=id;entry.refresh.request(selected.generations[entry.witch],GetTickCount64(),refreshDelay());++queued;}
    if(!queued){error="No owned selected replacement is visible to refresh. Its next assembly will use the current resources.";return false;}return true;
}
bool ownedOutfitVisibility(uintptr_t actor,int witch,OutfitVisibilityPolicy& policy){
    policy={};if(witch<0||witch>5)return false;
    const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);
    const auto found=owned.find(actor);if(found==owned.end())return false;const auto& entry=found->second;
    if(!entry.installed||entry.pending||entry.refresh.pending()||entry.witch!=witch)return false;
    WardrobeSnapshot snapshot;if(!captureWardrobeSnapshot(actor,witch,base,snapshot)||snapshot.node!=entry.node)return false;
    bool present=false;for(uint32_t i=0;i<snapshot.claimCount;++i)if(entry.current==snapshot.claims[i].prefab)present=true;
    if(!present)return false;
    policy.owned=true;policy.owner=entry.node;policy.prefab=entry.current;
    if(faulted)return true;
    WardrobeProviderOutfit provider;std::string error;
    if(!resolveWardrobeProvider(wardrobeOutfitId(selected.choices[witch]),witch,provider,error)||provider.prefab!=entry.current)return true;
    policy.enabled=true;for(const auto& part:provider.visibilityParts)policy.parts.push_back({part.part,part.visible});
    return true;
}
}
