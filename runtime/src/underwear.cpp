#include "underwear.h"
#include "game_layout.h"
#include "runtime.h"
#include "core/log.h"
#include "wardrobe_native.h"
#include "wardrobe_snapshot.h"
#include "appearance_hooks.h"
#include "character_addons.h"
#include "record_flags.h"
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>

namespace hex {
namespace {
using PartQuery=float(*)(void*,const uint32_t*,void*,uint8_t);
using SetPart=void(*)(void*,const uint32_t*,float,uint32_t,uint8_t);
using GetPart=bool(*)(void*,const uint32_t*);
using Intern=void*(*)(uint32_t*,const char*,uint32_t,uint32_t);
WardrobeActorObserved observeActor=nullptr;
PartQuery originalQuery=nullptr;SetPart setPart=nullptr;GetPart getPart=nullptr;Intern intern=nullptr;
std::atomic<bool> ready{false};
std::atomic<uint32_t> underwearKey{0};
std::array<std::atomic<uint64_t>,characterCapacity> confirmed{};
std::array<std::atomic<DWORD>,characterCapacity> seen{};
RecordFlags<characterCapacity> addonObservedExternalUnderwear;
std::mutex statusMutex,cacheMutex,preferenceMutex,updateMutex;
std::array<std::atomic<uint64_t>,6> outfitConfirmed{};
std::array<std::atomic<DWORD>,6> outfitSeen{};
std::array<uintptr_t,6> outfitConfirmedActor{};
std::string status="The underwear controls are waiting for the game.";
enum class UnderwearOverride {Rules,Hide,Show};
struct Applied {
    int witch=-1;
    DWORD tick=0,outfitTick=0,failureLoggedAt=0;
    uint64_t version=0,failedVersion=0;
    void* renderer=nullptr;
    float normal=1.f;
    UnderwearOverride override=UnderwearOverride::Rules;
    bool normalKnown=false,nativeRuleKnown=false,providerSuppressed=false,outfitExposed=false,basePresent=false,emptyProfile=false;
};
std::map<uintptr_t,Applied> applied;
OutfitVisibility garmentVisibility;
uint32_t detectedUnderwear(const Runtime& r){return r.underwearMountedExternalMask.load()|r.underwearObservedExternalMask.load();}
uint32_t externalUnderwear(const Runtime& r){return r.allowExternalUnderwear.load()?detectedUnderwear(r):0;}
bool observedExternalUnderwear(const Runtime& r,int target){
    if(target<0)return false;
    if(target>=firstAddonCharacter)return addonObservedExternalUnderwear.test(size_t(target));
    return (r.underwearObservedExternalMask.load()&(1u<<target))!=0;
}
bool usesExternalUnderwear(const Runtime& r,int target){
    if(target<0)return false;
    if(target>=firstAddonCharacter)return r.allowExternalUnderwear.load()&&observedExternalUnderwear(r,target);
    return (externalUnderwear(r)&(1u<<target))!=0;
}
bool connectedAddonUnderwear(int target){return target>=firstAddonCharacter&&addonVenusBodyActive(target);}
bool missingBaseMesh(const Runtime& r,int target){
    return connectedAddonUnderwear(target)?!addonVenusUnderwearPresent(target):(r.underwearMissingMeshMask.load()&(1u<<target))!=0;
}
bool requestedUnderwearHide(const Runtime& r,int target){
    return connectedAddonUnderwear(target)?characterAddonEnabled(target)&&addonUnderwearHidden(target):
        (r.underwearHiddenMask.load()&(1u<<target))!=0;
}
bool exposed(const Runtime& r,int witch,const Applied& entry,DWORD now){
    return entry.outfitExposed&&now-entry.outfitTick<1500&&
        (entry.emptyProfile||(connectedAddonUnderwear(witch)?characterAddonEnabled(witch):(r.outfitHiddenMask.load()&(1u<<witch))!=0));
}
UnderwearOverride wantedOverride(const Runtime& r,int witch,const Applied& entry,DWORD now){
    if(entry.providerSuppressed)return UnderwearOverride::Hide;
    if(usesExternalUnderwear(r,witch))return UnderwearOverride::Rules;
    if(requestedUnderwearHide(r,witch))return UnderwearOverride::Hide;
    if(!r.allowExternalUnderwear.load()&&exposed(r,witch,entry,now)&&(entry.nativeRuleKnown||entry.basePresent))return UnderwearOverride::Show;
    return UnderwearOverride::Rules;
}
float shownValue(float baseline){return baseline>0.f?baseline:1.f;}
void recordExternalUnderwear(Runtime& r,int target,bool hidden){
    if(target<0)return;
    if(target>=firstAddonCharacter){
        if(addonObservedExternalUnderwear.exchange(size_t(target),hidden)!=hidden)++r.underwearVersion;
        return;
    }
    const uint32_t bit=1u<<target;
    const auto previous=hidden?r.underwearObservedExternalMask.fetch_or(bit):r.underwearObservedExternalMask.fetch_and(~bit);
    if(bool(previous&bit)!=hidden)++r.underwearVersion;
}
#ifdef HEX_UNDERWEAR_TESTS
bool(*fixtureIdentity)(void*,ActorIdentity&)=nullptr;
bool fixtureReplaceUnderwear=false;
#endif
bool identityOf(void* actor,ActorIdentity& identity) {
#ifdef HEX_UNDERWEAR_TESTS
    if(fixtureIdentity)return fixtureIdentity(actor,identity);
#endif
    if(!readWardrobeIdentity(actor,identity))return false;
    if(identity.witch<firstAddonCharacter)return true;
    const auto* addon=characterAddon(identity.witch);
    return addon&&connectedAddonUnderwear(identity.witch)&&matchesWardrobeAppearance(
        identity.actor,reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),addon->appearance.c_str());
}



void* rendererGuard(void* actor) {
    __try {
        if(!actor)return nullptr;
        const auto components=*reinterpret_cast<const uintptr_t*>(reinterpret_cast<uintptr_t>(actor)+0x68);
        if(!components)return nullptr;
        const auto control=*reinterpret_cast<const uintptr_t*>(components+0x40);
        if(!control||*reinterpret_cast<void* const*>(control+8)!=actor)return nullptr;
        return *reinterpret_cast<void* const*>(control+0x130);
    }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
void* queryActorGuard(void* scene) {
    __try {
        if(!scene)return nullptr;
        const auto control=*reinterpret_cast<const uintptr_t*>(reinterpret_cast<uintptr_t>(scene)+0x88);
        if(!control)return nullptr;
        auto* actor=*reinterpret_cast<void* const*>(control+8);
        if(!actor)return nullptr;
        const auto components=*reinterpret_cast<const uintptr_t*>(reinterpret_cast<uintptr_t>(actor)+0x68);
        if(!components||*reinterpret_cast<const uintptr_t*>(components+0x40)!=control)return nullptr;
        return actor;
    }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}

bool keyGuard(uint32_t* key) {
    __try {if(!underwearKey.load()){uint32_t value=0;intern(&value,"Underwear",1,0x2ffff);if(!value)return false;underwearKey=value;}*key=underwearKey;return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool keyEqual(const uint32_t* input,uint32_t key) {
    __try {return input&&*input==key;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool readKey(const uint32_t* input,uint32_t* key){__try{if(!input)return false;*key=*input;return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
float query(void* scene,const uint32_t* part,void* row,uint8_t mode) {
    const auto normal=originalQuery(scene,part,row,mode);
    if(ready&&observeActor&&scene){
        struct Observation {void* scene=nullptr;uint64_t at=0;};
        static thread_local std::array<Observation,64> samples{};
        auto& sample=samples[(reinterpret_cast<uintptr_t>(scene)>>4)%samples.size()];
        const auto now=GetTickCount64();
        if(sample.scene!=scene||now-sample.at>=200){
            sample={scene,now};
            if(auto* actor=queryActorGuard(scene))observeActor(actor);
        }
    }
    uint32_t key=0;ActorIdentity identity;
    void* actor=nullptr;
    if(ready&&keyGuard(&key)&&keyEqual(part,key)&&(actor=queryActorGuard(scene))&&identityOf(actor,identity)) {
        seen[identity.witch]=GetTickCount();
        auto& runtime=Runtime::get();
        if(missingBaseMesh(runtime,identity.witch))return normal;
        if(std::isfinite(normal)&&normal>=0.f) {
            auto* renderer=rendererGuard(actor);
            std::lock_guard<std::mutex> lock(cacheMutex);auto& entry=applied[reinterpret_cast<uintptr_t>(actor)];
            if(entry.renderer!=renderer||entry.witch!=identity.witch){entry=Applied{};entry.renderer=renderer;}
            const auto now=GetTickCount();
            if(now-entry.tick>=1000)entry.providerSuppressed=false;
            entry.witch=identity.witch;entry.normal=normal;entry.normalKnown=true;entry.nativeRuleKnown=true;
            if(renderer&&exposed(runtime,identity.witch,entry,now)&&!entry.providerSuppressed&&normal==0.f)
                recordExternalUnderwear(runtime,identity.witch,true);
            const auto policy=wantedOverride(runtime,identity.witch,entry,now);
            if(policy==UnderwearOverride::Hide)return 0.f;
            if(policy==UnderwearOverride::Show)return shownValue(normal);
        }
        if(requestedUnderwearHide(runtime,identity.witch)&&!usesExternalUnderwear(runtime,identity.witch))return 0.f;
    }
    uint32_t garmentKey=0;
    if(ready&&readKey(part,&garmentKey)&&garmentVisibility.contains(garmentKey)){
        actor=queryActorGuard(scene);
        if(actor&&identityOf(actor,identity))return garmentVisibility.query(reinterpret_cast<uintptr_t>(actor),reinterpret_cast<uintptr_t>(rendererGuard(actor)),identity.witch,garmentKey,normal,GetTickCount64());
    }
    return normal;
}
bool currentGuard(void* renderer,uint32_t key,float* visible) {
    __try {
        *visible=getPart(renderer,&key)?1.f:0.f;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool changeGuard(void* renderer,uint32_t key,float visible) {
    __try {
        if(!renderer)return false;
        if(!std::isfinite(visible)||visible<0.f)return false;
        setPart(renderer,&key,visible,0,0);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool namedKeyGuard(const char* name,uint32_t* key){__try{*key=0;intern(key,name,1,0x2ffff);return *key!=0;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
OutfitVisibilityCalls visibilityCalls(){return {namedKeyGuard,currentGuard,changeGuard};}
}
bool initializeUnderwear(WardrobeActorObserved observe,std::string& error) {
    const auto fail=[&](const std::string& message){error=message;Runtime::get().reportCompatibility(Feature::Underwear,CompatibilityState::Blocked,message);std::lock_guard<std::mutex> lock(statusMutex);status=message;return false;};
    if(!loadedGameLayout())return fail("This Crimson Desert build has no checked underwear route.");
    auto* game=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    static const unsigned char showBytes[]={0x48,0x83,0xec,0x68,0x8b,0x02,0x4c,0x8d,0x44,0x24,0x40,0x45,0x33,0xd2};
    static const unsigned char showTail[]={0x24,0x40,0x41,0x8b,0xd2,0x4c,0x8b,0xc9,0x83,0xfa,0x01,0x74,0x34};
    static const unsigned char getBytes[]={0x48,0x83,0xec,0x28,0x48,0x8b,0x01,0x4c,0x8b,0x40,0x08};
    static const unsigned char getTail[]={0x4d,0x85,0xc0,0x49,0x8d,0x88,0xb0,0x00,0x00,0x00,0x48,0x0f,0x44,0xc8};
    static const unsigned char internBytes[]={0x48,0x89,0x5c,0x24,0x20,0x44,0x89,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10};
    static const unsigned char internTail[]={0x89,0x4c,0x24,0x08,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    static const unsigned char queryBytes[]={0x48,0x89,0x5c,0x24,0x08,0x45,0x0f,0xb6,0x58,0x20,0x48,0x8b,0xd9};
    static const unsigned char queryTail[]={0x03,0x0f,0x84,0x86,0x00,0x00,0x00,0x48,0x8b,0x41,0x78,0x44,0x8b,0x91,0x80,0x00};
    auto* target=game+gameLayout().visibilityObserver;
    auto* show=game+gameLayout().visibilitySet;
    auto* get=game+gameLayout().visibilityGet;
    auto* names=game+gameLayout().internString;
    if(!ready&&appearance::checkedGameEntry(show,showBytes,16,showTail)==appearance::GameEntry::Unavailable)
        return fail(appearance::entryIssue(show,"The renderer's part visibility setter"));
    if(!ready&&appearance::checkedGameEntry(get,getBytes,16,getTail)==appearance::GameEntry::Unavailable)
        return fail(appearance::entryIssue(get,"The renderer's part visibility reader"));
    if(!ready&&appearance::checkedGameEntry(names,internBytes,16,internTail)==appearance::GameEntry::Unavailable)
        return fail(appearance::entryIssue(names,"The game's part name lookup"));
    const bool mayObserve=appearance::checkedGameEntry(target,queryBytes,16,queryTail)!=appearance::GameEntry::Unavailable;
    const bool wasReady=ready.load();
    if(!wasReady){
        observeActor=observe;
        setPart=reinterpret_cast<SetPart>(show);
        getPart=reinterpret_cast<GetPart>(get);
        intern=reinterpret_cast<Intern>(names);
    }
    bool observes=false;
    if(mayObserve&&MH_CreateHook(target,reinterpret_cast<void*>(&query),reinterpret_cast<void**>(&originalQuery))==MH_OK) {
        observes=MH_EnableHook(target)==MH_OK;
        if(!observes)MH_RemoveHook(target);
    }
    const char* capability=observes?"Base visibility functions are ready.":"Base visibility uses the checked renderer controls. Another plugin can reapply its own visibility during an outfit rebuild.";
    ready=true;{std::lock_guard<std::mutex> lock(statusMutex);status=capability;}
    Runtime::get().reportCompatibility(Feature::Underwear,CompatibilityState::Ready,capability);
    if(!wasReady||observes)LOG("Base underwear ready; visibility rule observer=%d.",observes);
    return observes;
}
UnderwearStatus underwearStatus(int target) {
    if(connectedAddonUnderwear(target)){
        const auto& r=Runtime::get();
        const bool pending=!seen[target]||GetTickCount()-seen[target]>3000||confirmed[target]!=r.underwearVersion;
        std::lock_guard<std::mutex> lock(statusMutex);
        return {ready,addonUnderwearHidden(target),false,pending,status};
    }
    if(target< -1||target>5)return {false,false,false,true,"Choose a supported witch."};
    UnderwearStatus result;result.ready=ready;const auto& r=Runtime::get();const auto mask=r.underwearHiddenMask.load();
    const uint32_t targets=(target<0?63u:1u<<target)&~externalUnderwear(r);
    result.hidden=targets&&(mask&targets)==targets;result.mixed=(mask&targets)&&!result.hidden;
    result.pending=false;const auto now=GetTickCount();
    for(int i=0;i<6;++i)if(targets&(1u<<i))if(!seen[i]||now-seen[i]>3000||confirmed[i]!=r.underwearVersion)result.pending=true;
    {std::lock_guard<std::mutex> lock(statusMutex);result.detail=status;}
    return result;
}
UnderwearPreference baseUnderwearPreference() {
    std::lock_guard<std::mutex> lock(preferenceMutex);const auto& r=Runtime::get();return {r.underwearHiddenMask.load()&~externalUnderwear(r),r.underwearVersion.load()};
}
UnderwearOwnership underwearOwnership(int target){
    if(connectedAddonUnderwear(target)){
        auto& r=Runtime::get();
        const bool delegated=usesExternalUnderwear(r,target);
        return {delegated,delegated,missingBaseMesh(r,target),{}};
    }
    if(target< -1||target>5)return {};
    auto& r=Runtime::get();const uint32_t selected=target<0?63u:1u<<target;
    const auto external=selected&externalUnderwear(r);
    const auto missing=selected&r.underwearMissingMeshMask.load();
    const bool meshMissing=missing==selected;
    const auto mounted=r.underwearMountedExternalMask.load();
    if(!(external&mounted)&&!missing)return {external!=0,external==selected,meshMissing,{},{}};
    const auto dependency=r.dependencyStatus();
    const auto& deps=*dependency;
    const auto namesFor=[&](uint32_t affected){
        std::string result;
        std::set<std::string> seen;
        for(size_t i=0;i<deps.underwearSources.size();++i)if(affected&(1u<<i)){
            const auto& name=deps.underwearSources[i];
            if(name.empty()||!seen.insert(name).second)continue;
            if(!result.empty())result+=", ";
            result+=name;
        }
        return result;
    };
    return {external!=0,external==selected,meshMissing,namesFor(external&mounted),namesFor(missing)};
}
bool setExternalUnderwear(bool external,std::string& error){
    std::lock_guard<std::mutex> lock(preferenceMutex);
    auto& r=Runtime::get();
    if(r.allowExternalUnderwear.load()==external)return true;
    if(!r.saveVisibilityPreference("AllowExternalUnderwear",external,error))return false;
    r.allowExternalUnderwear=external;++r.underwearVersion;
    return true;
}
UnderwearPreference outfitHiddenPreference(){std::lock_guard<std::mutex> lock(preferenceMutex);const auto& r=Runtime::get();return {r.outfitHiddenMask.load(),r.outfitHiddenVersion.load()};}
UnderwearStatus outfitHiddenStatus(int target){
    if(target< -1||target>5)return {false,false,false,true,"Choose a supported witch."};
    UnderwearStatus result;auto& r=Runtime::get();const auto preference=outfitHiddenPreference();
    const auto mask=preference.hiddenMask,targets=target<0?63u:1u<<target;
    result.ready=ready.load();
    result.hidden=(mask&targets)==targets;result.mixed=(mask&targets)&&!result.hidden;result.pending=false;
    for(int i=0;i<6;++i)if(targets&(1u<<i))if(!outfitSeen[i]||GetTickCount()-outfitSeen[i]>3000||outfitConfirmed[i]!=preference.revision)result.pending=true;
    if(!result.ready){result.pending=false;result.detail=underwearStatus(target).detail;}
    else result.detail="Hides default clothing and opted-in outfit parts. Base underwear is controlled separately.";return result;
}
bool setOutfitHidden(int target,bool hidden,std::string& error){
    if(target< -1||target>5){error="Choose a supported witch.";return false;}
    std::lock_guard<std::mutex> lock(preferenceMutex);auto& r=Runtime::get();const auto before=r.outfitHiddenMask.load(),targets=target<0?63u:1u<<target;
    const auto after=hidden?(before|targets):(before&~targets);if(after==before)return true;
    if(!r.saveOutfitHiddenPreference(after,error))return false;r.outfitHiddenMask=after;++r.outfitHiddenVersion;return true;
}
void setOutfitHiddenActor(uintptr_t actor,uintptr_t renderer,int witch,bool hidden,bool complete,bool basePresent,bool emptyProfile){
    if(actor&&renderer&&connectedAddonUnderwear(witch)){
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto& entry=applied[actor];
        auto* identityRenderer=reinterpret_cast<void*>(renderer);
        if(entry.renderer!=identityRenderer||entry.witch!=witch){entry={};entry.renderer=identityRenderer;entry.witch=witch;}
        entry.outfitExposed=complete&&(hidden||emptyProfile);
        entry.emptyProfile=emptyProfile;
        entry.outfitTick=GetTickCount();
        entry.basePresent=basePresent;
        return;
    }
    if(!actor||!renderer||witch<0||witch>5)return;
    const auto preference=outfitHiddenPreference();
    const auto now=GetTickCount();
    std::lock_guard<std::mutex> lock(cacheMutex);
    if(complete&&hidden==((preference.hiddenMask&(1u<<witch))!=0)){
        outfitConfirmedActor[witch]=actor;
        outfitConfirmed[witch]=preference.revision;
        outfitSeen[witch]=now;
    }else if(outfitConfirmedActor[witch]==actor){
        outfitConfirmedActor[witch]=0;
        outfitConfirmed[witch]=0;
        outfitSeen[witch]=0;
    }
    auto& entry=applied[actor];
    auto* identityRenderer=reinterpret_cast<void*>(renderer);
    if(entry.renderer!=identityRenderer||entry.witch!=witch){entry=Applied{};entry.renderer=identityRenderer;entry.witch=witch;}
    entry.outfitExposed=complete&&(hidden||emptyProfile);entry.emptyProfile=emptyProfile;entry.outfitTick=now;entry.basePresent=basePresent;
}
bool setUnderwearHidden(int target,bool hidden,std::string& error) {
    if(target< -1||target>5){error="Choose a supported witch.";return false;}
    std::lock_guard<std::mutex> lock(preferenceMutex);
    auto& r=Runtime::get();const auto before=r.underwearHiddenMask.load();const auto targets=target<0?63u:1u<<target;
    const auto after=hidden?(before|targets):(before&~targets);if(after==before)return true;
    r.underwearHiddenMask=after;
    if(!r.saveUnderwearPreference(after,error)){r.underwearHiddenMask=before;return false;}
    ++r.underwearVersion;return true;
}
void updateUnderwear(void* actor,const ActorIdentity& identity,uintptr_t checkedRenderer) {
    if(!ready||identity.witch<0||(identity.witch>=firstAddonCharacter&&!connectedAddonUnderwear(identity.witch)))return;
    static thread_local bool updating=false;
    if(updating)return;
    std::unique_lock<std::mutex> updateLock(updateMutex,std::try_to_lock);
    if(!updateLock)return;
    struct UpdateGuard {bool& active;~UpdateGuard(){active=false;}} guard{updating};
    updating=true;
    auto* renderer=rendererGuard(actor);
    if(checkedRenderer&&reinterpret_cast<uintptr_t>(renderer)!=checkedRenderer)return;
    OutfitVisibilityPolicy policy;
#ifndef HEX_UNDERWEAR_TESTS
    if(identity.witch<firstAddonCharacter){
        if(!readOwnedOutfitVisibility(reinterpret_cast<uintptr_t>(actor),identity.witch,policy))return;
        garmentVisibility.update(reinterpret_cast<uintptr_t>(actor),reinterpret_cast<uintptr_t>(renderer),identity.witch,policy,GetTickCount64(),visibilityCalls());
    }
#else
    policy.owned=policy.enabled=fixtureReplaceUnderwear;policy.replaceBaseUnderwear=fixtureReplaceUnderwear;
#endif
    if(checkedRenderer&&rendererGuard(actor)!=renderer)return;
    auto& r=Runtime::get();
    if(missingBaseMesh(r,identity.witch)){
        seen[identity.witch]=GetTickCount();
        confirmed[identity.witch]=r.underwearVersion.load();
        return;
    }
    uint32_t key=0;if(!keyGuard(&key))return;
    const bool providerSuppressed=policy.owned&&policy.enabled&&policy.replaceBaseUnderwear;
    const auto address=reinterpret_cast<uintptr_t>(actor);const auto now=GetTickCount();
    Applied entry;
    {std::lock_guard<std::mutex> lock(cacheMutex);entry=applied[address];}
    if(entry.renderer!=renderer||entry.witch!=identity.witch){entry=Applied{};entry.renderer=renderer;entry.witch=identity.witch;}
    seen[identity.witch]=now;
    const bool providerChanged=entry.providerSuppressed!=providerSuppressed;
    entry.providerSuppressed=providerSuppressed;
    auto desired=wantedOverride(r,identity.witch,entry,now);
    auto version=r.underwearVersion.load();
    if(!providerChanged&&now-entry.tick<500&&
       ((entry.version==version&&entry.override==desired)||entry.failedVersion==version))return;
    if(renderer&&exposed(r,identity.witch,entry,now)&&!providerSuppressed){
        float natural=0.f;
        if(entry.override==UnderwearOverride::Rules&&currentGuard(renderer,key,&natural)){
            if(!entry.nativeRuleKnown){entry.normal=natural;entry.normalKnown=true;}
            recordExternalUnderwear(r,identity.witch,natural==0.f||(entry.nativeRuleKnown&&entry.normal==0.f));
        }
        desired=wantedOverride(r,identity.witch,entry,now);
        version=r.underwearVersion.load();
    }
    if(desired!=UnderwearOverride::Rules||entry.override!=UnderwearOverride::Rules) {
        entry.normalKnown=entry.normalKnown||(renderer&&currentGuard(renderer,key,&entry.normal));
        const bool releaseHidden=desired==UnderwearOverride::Rules&&r.allowExternalUnderwear.load()&&
            observedExternalUnderwear(r,identity.witch)&&exposed(r,identity.witch,entry,now);
        const float value=desired==UnderwearOverride::Hide||releaseHidden?0.f:
            desired==UnderwearOverride::Show?shownValue(entry.normal):entry.normal;
        if(!entry.normalKnown||!changeGuard(renderer,key,value)) {
            if(entry.failedVersion!=version||now-entry.failureLoggedAt>2000) {
                LOG_ERR("Underwear part update is not ready for %s actor=%p renderer=%p key=%u.",identity.name,actor,renderer,key);entry.failureLoggedAt=now;
            }
            entry.failedVersion=version;
            entry.tick=now;
            {std::lock_guard<std::mutex> lock(cacheMutex);applied[address]=entry;}return;
        }
        if(entry.version!=version||entry.override!=desired){
            float actual=-1.f;
            currentGuard(renderer,key,&actual);
            LOG("Underwear %s on %s (key=%u actor=%p renderer=%p baseline=%.3f visible=%.0f outfit_provider=%d).",
                desired==UnderwearOverride::Hide?"hidden":desired==UnderwearOverride::Show?"shown":"restored to outfit rules",
                identity.name,key,actor,renderer,entry.normal,actual,int(providerSuppressed));
        }
    }
    entry.tick=now;entry.override=desired;entry.version=version;entry.failedVersion=0;
    std::lock_guard<std::mutex> lock(cacheMutex);

    const auto latest=applied.find(address);
    if(latest!=applied.end()&&latest->second.renderer==renderer&&latest->second.witch==identity.witch){
        const auto& current=latest->second;
        if(current.nativeRuleKnown){entry.normal=current.normal;entry.normalKnown=true;entry.nativeRuleKnown=true;}
        entry.outfitTick=current.outfitTick;entry.outfitExposed=current.outfitExposed;entry.basePresent=current.basePresent;
    }
    applied[address]=entry;confirmed[identity.witch]=version;
    for(auto i=applied.begin();i!=applied.end();)if(now-i->second.tick>30000)i=applied.erase(i);else++i;
}
bool restoreOutfitVisibility(void* actor){
    return !ready||garmentVisibility.restore(reinterpret_cast<uintptr_t>(actor),reinterpret_cast<uintptr_t>(rendererGuard(actor)),GetTickCount64(),visibilityCalls());
}
bool readOutfitPartVisibility(uintptr_t renderer,uint32_t key,bool& visible){
    if(!ready||!renderer||!key)return false;
    float current=0;
    if(!currentGuard(reinterpret_cast<void*>(renderer),key,&current))return false;
    visible=current!=0;
    return true;
}
bool writeOutfitPartVisibility(uintptr_t renderer,uint32_t key,bool visible){
    return ready&&renderer&&key&&changeGuard(reinterpret_cast<void*>(renderer),key,visible?1.f:0.f);
}
#ifdef HEX_UNDERWEAR_TESTS
namespace {
void* fixtureActor=nullptr;void* fixtureOther=nullptr;void* fixtureRenderer=nullptr;
unsigned fixtureWrites=0;float fixtureVisibility=-1;bool fixtureArguments=true,fixtureVisible=true;
float fixtureRule=.7f;bool fixtureTrackVisibility=false;
bool fixtureWho(void* actor,ActorIdentity& identity) {
    if(actor!=fixtureActor&&actor!=fixtureOther)return false;
    identity.witch=actor==fixtureActor?3:0;strcpy_s(identity.name,actor==fixtureActor?"NHW_Unique_Wisdom_Witch_Store":"NHW_Unique_Modesty_Witch_Store");return true;
}
void* fixtureIntern(uint32_t* key,const char* text,uint32_t mode,uint32_t domain) {
    if(strcmp(text,"Underwear")||mode!=1||domain!=0x2ffff)fixtureArguments=false;*key=0x12345;return key;
}
float fixtureQuery(void*,const uint32_t*,void*,uint8_t){return fixtureRule;}
bool fixtureGet(void* renderer,const uint32_t* key){if(renderer!=fixtureRenderer||*key!=0x12345)fixtureArguments=false;return fixtureVisible;}
void fixtureSet(void* renderer,const uint32_t* key,float value,uint32_t mode,uint8_t flag) {
    ++fixtureWrites;fixtureVisibility=value;
    if(fixtureTrackVisibility)fixtureVisible=value>0.f;
    if(renderer!=fixtureRenderer||*key!=0x12345||mode||flag)fixtureArguments=false;
}
}
bool runUnderwearPolicyTests(const std::filesystem::path& profile,std::string& error) {
    auto check=[&](bool ok,const char* message){if(!ok)error=message;return ok;};
    auto& r=Runtime::get();r.profiles=profile;std::filesystem::create_directories(profile.parent_path());
    std::array<uintptr_t,0x100> owner{},otherOwner{};
    std::array<uintptr_t,0x20> actor{},other{},components{},otherComponents{},scene{},otherScene{},table{};uintptr_t renderer=0;
    owner[1]=reinterpret_cast<uintptr_t>(actor.data());owner[0x130/8]=reinterpret_cast<uintptr_t>(&renderer);
    components[0x40/8]=reinterpret_cast<uintptr_t>(owner.data());actor[0x68/8]=reinterpret_cast<uintptr_t>(components.data());scene[0x88/8]=reinterpret_cast<uintptr_t>(owner.data());
    otherOwner[1]=reinterpret_cast<uintptr_t>(other.data());otherOwner[0x130/8]=reinterpret_cast<uintptr_t>(&renderer);
    otherComponents[0x40/8]=reinterpret_cast<uintptr_t>(otherOwner.data());other[0x68/8]=reinterpret_cast<uintptr_t>(otherComponents.data());otherScene[0x88/8]=reinterpret_cast<uintptr_t>(otherOwner.data());
    fixtureActor=actor.data();fixtureOther=other.data();fixtureRenderer=&renderer;
    intern=fixtureIntern;getPart=fixtureGet;setPart=fixtureSet;originalQuery=fixtureQuery;fixtureIdentity=fixtureWho;
    r.underwearHiddenMask=0;r.allowExternalUnderwear=true;r.underwearVersion=1;underwearKey=0;ready=true;applied.clear();fixtureWrites=0;
    ActorIdentity identity;fixtureWho(fixtureActor,identity);uint32_t key=0x12345,unrelated=0x65432;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureWrites==0&&!underwearStatus(3).pending,"normal outfit rules must not force visibility"))return false;
    if(!check(setUnderwearHidden(3,true,error)&&r.underwearHiddenMask==8,"targeted nude-base selection"))return false;
    r.dependencyScanned=true;r.shapingAvailable=false;
    if(!check(query(scene.data(),&key,table.data(),0)==0&&query(otherScene.data(),&key,table.data(),0)==.7f&&query(scene.data(),&unrelated,table.data(),0)==.7f&&query(nullptr,&key,table.data(),0)==.7f,"only the selected witch's Underwear part can be hidden"))return false;
    if(!check(query(fixtureActor,&key,table.data(),0)==.7f,"an actor must not be mistaken for a visibility context"))return false;
    updateUnderwear(fixtureActor,identity,reinterpret_cast<uintptr_t>(fixtureOther));
    if(!check(fixtureWrites==0&&underwearStatus(3).pending,"the range update rejects a renderer that was replaced"))return false;
    updateUnderwear(fixtureActor,identity,reinterpret_cast<uintptr_t>(fixtureRenderer));
    if(!check(fixtureWrites==1&&fixtureVisibility==0&&!underwearStatus(3).pending&&fixtureArguments,"native visibility argument layout and acknowledgement"))return false;
    r.dependencyScanned=false;
    if(!check(setUnderwearHidden(3,false,error),"restore selection"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureWrites==2&&fixtureVisibility==.7f,"restore original outfit rules rather than forcing a visible part"))return false;
    applied.clear();fixtureVisible=false;
    if(!check(setUnderwearHidden(3,true,error),"NPC with no underwear visibility rule"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureWrites==3&&fixtureVisibility==0,"hiding works when the NPC has no PartInOutSocket row"))return false;
    if(!check(setUnderwearHidden(3,false,error),"restore unruled prefab part"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureWrites==4&&fixtureVisibility==0,"an unqueried part returns to its previously hidden state"))return false;
    applied.clear();fixtureVisible=true;
    if(!check(setUnderwearHidden(3,true,error),"unqueried visible prefab part"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(setUnderwearHidden(3,false,error),"restore unqueried visible prefab part"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureWrites==6&&fixtureVisibility==1,"an unqueried part returns to its previously visible state"))return false;
    applied.clear();owner[1]=reinterpret_cast<uintptr_t>(other.data());
    if(!check(setUnderwearHidden(3,true,error),"mismatched component test"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureWrites==6&&underwearStatus(3).pending&&query(scene.data(),&key,table.data(),0)==.7f,"mismatched actor component must fail before a native write"))return false;
    owner[1]=reinterpret_cast<uintptr_t>(actor.data());
    if(!check(setUnderwearHidden(-1,true,error)&&r.underwearHiddenMask==63&&setUnderwearHidden(3,false,error)&&r.underwearHiddenMask==55,"all-witch selection and independent override"))return false;
    const auto saved=profile.parent_path()/L"interface.ini";
    if(!check(GetPrivateProfileIntW(L"Interface",L"UnderwearHiddenMask",999,saved.c_str())==55,"visibility preferences persist independently of shapes"))return false;
    const auto preference=baseUnderwearPreference();
    if(!check(preference.hiddenMask==55&&preference.revision==r.underwearVersion,"SDK snapshot reflects menu preferences"))return false;
    if(!check(setUnderwearHidden(3,false,error)&&baseUnderwearPreference().revision==preference.revision,"unchanged visibility does not invalidate provider revisions"))return false;
    const auto locked=CreateFileW(saved.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(!check(locked!=INVALID_HANDLE_VALUE,"lock underwear preference fixture"))return false;
    const bool rejected=!setUnderwearHidden(3,true,error);CloseHandle(locked);
    if(!check(rejected&&baseUnderwearPreference().hiddenMask==55&&baseUnderwearPreference().revision==preference.revision,"failed native preference save preserves SDK visibility and revision"))return false;
    r.underwearHiddenMask=0;r.loadInterface(saved);
    if(!check(baseUnderwearPreference().hiddenMask==55,"saved base visibility is available to providers after reload"))return false;
    r.compact=true;r.quickPins=PinTop|PinOutfit;r.blockGameInput=false;
    if(!check(r.saveInterface()&&setUnderwearHidden(3,true,error),"save interface and update only the base preference"))return false;
    if(!check(GetPrivateProfileIntW(L"Interface",L"Compact",0,saved.c_str())==1&&GetPrivateProfileIntW(L"Interface",L"QuickPins",0,saved.c_str())==(PinTop|PinOutfit)&&GetPrivateProfileIntW(L"Interface",L"BlockGameInput",1,saved.c_str())==0,"provider preference update preserves UI settings without accessing UI fields"))return false;
    if(!check(setUnderwearHidden(3,false,error),"restore selected witch after isolated preference save"))return false;
    r.reportCompatibility(Feature::Wardrobe,CompatibilityState::Ready,"Fixture");r.outfitHiddenMask=0;r.outfitHiddenVersion=1;applied.clear();
    if(!check(setOutfitHidden(3,true,error)&&r.outfitHiddenMask==8&&r.underwearHiddenMask==55,"Hide outfit preference is separate from saved Base underwear"))return false;
    if(!check(setUnderwearHidden(-1,false,error),"isolate Hide outfit fixture"))return false;
    setOutfitHiddenActor(reinterpret_cast<uintptr_t>(fixtureActor),reinterpret_cast<uintptr_t>(fixtureRenderer),3,true,true);
    fixtureVisibility=.7f;
    if(!check(query(scene.data(),&key,table.data(),0)==.7f&&query(otherScene.data(),&key,table.data(),0)==.7f,"Hide outfit alone leaves base underwear unchanged on both witches"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureVisibility==.7f&&!outfitHiddenStatus(3).pending,"outfit acknowledgement does not force underwear visibility"))return false;
    setOutfitHiddenActor(reinterpret_cast<uintptr_t>(fixtureOther),reinterpret_cast<uintptr_t>(fixtureRenderer),3,true,false);
    if(!check(!outfitHiddenStatus(3).pending,"an incomplete observation on another actor preserves the verified outfit status"))return false;
    setOutfitHiddenActor(reinterpret_cast<uintptr_t>(fixtureActor),reinterpret_cast<uintptr_t>(fixtureRenderer),3,true,false);
    if(!check(outfitHiddenStatus(3).pending,"a partial assembly invalidates the same actor's previous outfit confirmation"))return false;
    setOutfitHiddenActor(reinterpret_cast<uintptr_t>(fixtureActor),reinterpret_cast<uintptr_t>(fixtureRenderer),3,true,true);
    if(!check(!outfitHiddenStatus(3).pending,"the rebuilt outfit can confirm the same saved preference again"))return false;
    if(!check(setUnderwearHidden(3,true,error)&&query(scene.data(),&key,table.data(),0)==0,"both independent controls are needed for full nudity"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(setOutfitHidden(3,false,error)&&query(scene.data(),&key,table.data(),0)==0&&r.underwearHiddenMask==8,"restoring clothing preserves hidden base underwear"))return false;
    if(!check(setUnderwearHidden(3,false,error),"restore separate underwear preference"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureVisibility==.7f&&r.underwearHiddenMask==0,"base underwear restores its own natural visibility"))return false;
    const auto scopedPreference=baseUnderwearPreference();
    fixtureReplaceUnderwear=true;updateUnderwear(fixtureActor,identity);
    if(!check(fixtureVisibility==0&&query(scene.data(),&key,table.data(),0)==0&&query(otherScene.data(),&key,table.data(),0)==.7f,"owned outfit suppresses only its witch's normal underwear"))return false;
    if(!check(setOutfitHidden(3,true,error)&&query(scene.data(),&key,table.data(),0)==0,"Hide outfit keeps the outfit's scoped underwear replacement active"))return false;
    if(!check(baseUnderwearPreference().hiddenMask==scopedPreference.hiddenMask&&baseUnderwearPreference().revision==scopedPreference.revision,"outfit-scoped suppression never edits the player's preference or revision"))return false;
    fixtureReplaceUnderwear=false;updateUnderwear(fixtureActor,identity);
    if(!check(fixtureVisibility==.7f&&query(scene.data(),&key,table.data(),0)==.7f,"ownership loss or Default restores natural underwear with Hide outfit still enabled"))return false;
    fixtureReplaceUnderwear=true;updateUnderwear(fixtureActor,identity);
    if(!check(setUnderwearHidden(3,true,error),"hide both active underwear sources"))return false;
    updateUnderwear(fixtureActor,identity);fixtureReplaceUnderwear=false;updateUnderwear(fixtureActor,identity);
    if(!check(fixtureVisibility==0&&query(scene.data(),&key,table.data(),0)==0,"provider removal retains the player's separate hide preference"))return false;
    if(!check(setUnderwearHidden(3,false,error)&&setOutfitHidden(3,false,error),"restore scoped underwear fixture"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureVisibility==.7f,"show after provider removal restores captured natural visibility"))return false;
    const auto nudeRevision=r.outfitHiddenVersion.load();const auto lockedNude=CreateFileW(saved.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(!check(lockedNude!=INVALID_HANDLE_VALUE,"lock Hide outfit preference fixture"))return false;
    const bool failedNude=!setOutfitHidden(3,true,error);CloseHandle(lockedNude);
    if(!check(failedNude&&r.outfitHiddenMask==0&&r.outfitHiddenVersion==nudeRevision,"failed Hide outfit save preserves native selection and revision"))return false;
    if(!check(setOutfitHidden(-1,true,error)&&setOutfitHidden(3,false,error)&&r.outfitHiddenMask==55,"Hide outfit All and per-witch override"))return false;
    r.outfitHiddenMask=0;r.loadInterface(saved);
    if(!check(r.outfitHiddenMask==55&&r.underwearHiddenMask==0&&setOutfitHidden(-1,false,error),"Hide outfit preference reloads independently"))return false;
    setOutfitHiddenActor(reinterpret_cast<uintptr_t>(fixtureActor),reinterpret_cast<uintptr_t>(fixtureRenderer),3,false,true);
    if(!check(setUnderwearHidden(-1,true,error)&&setUnderwearHidden(3,false,error),"restore prior base preference for invalid-input checks"))return false;
    if(!check(!setUnderwearHidden(6,true,error)&&r.underwearHiddenMask==55,"invalid target rejection"))return false;
    ready=false;
    if(!check(setOutfitHidden(3,true,error)&&setOutfitHidden(3,false,error),"the outfit route remains usable when the separate underwear hook is unavailable"))return false;
    if(!check(setUnderwearHidden(3,true,error)&&r.underwearHiddenMask==63&&!underwearStatus(3).ready&&underwearStatus(3).pending,
        "waiting for the renderer preserves the requested preference without acknowledging it"))return false;
    if(!check(setUnderwearHidden(3,false,error)&&r.underwearHiddenMask==55,"cancel the queued preference before the route is ready"))return false;
    DependencyStatus vanilla;vanilla.scanned=true;vanilla.bodyMode=BodyMode::Vanilla;vanilla.bodyVerified=true;vanilla.game.valid=true;vanilla.instant.valid=true;r.publishDependencies(vanilla);
    if(!check(setUnderwearHidden(3,true,error)&&setUnderwearHidden(3,false,error),"Vanilla option retains Base underwear preferences without Venus Body for Damiane"))return false;
    if(!check(setOutfitHidden(3,true,error)&&setOutfitHidden(3,false,error),"Vanilla option retains outfit visibility without Venus Body for Damiane"))return false;
    r.publishDependencies({});
    r.underwearHiddenMask=0;r.outfitHiddenMask=0;r.allowExternalUnderwear=true;r.underwearObservedExternalMask=0;
    applied.clear();fixtureTrackVisibility=true;fixtureVisible=false;fixtureRule=0;ready=true;
    const auto fixtureAddress=reinterpret_cast<uintptr_t>(fixtureActor),fixtureRendererAddress=reinterpret_cast<uintptr_t>(fixtureRenderer);
    if(!check(setOutfitHidden(3,true,error),"isolate the hidden outfit for ownership detection"))return false;
    setOutfitHiddenActor(fixtureAddress,fixtureRendererAddress,3,true,true,true);
    updateUnderwear(fixtureActor,identity);
    if(!check(underwearOwnership(3).fullyExternal&&underwearOwnership(3).source.empty()&&!underwearOwnership(0).delegated,"a hidden base part defers to unnamed rules only for that witch"))return false;
    if(!check(setOutfitHidden(3,false,error),"clear the independent Hide outfit preference"))return false;
    setOutfitHiddenActor(fixtureAddress,fixtureRendererAddress,3,false,true,true,true);
    updateUnderwear(fixtureActor,identity);
    if(!check(!outfitHiddenStatus(3).pending&&!outfitHiddenStatus(3).hidden&&underwearOwnership(3).fullyExternal,
        "an empty Wardrobe exposes the same base rules without changing the Hide outfit preference"))return false;
    if(!check(setUnderwearHidden(3,true,error)&&!(baseUnderwearPreference().hiddenMask&8),"deferring control suspends the saved Hex Appeal override"))return false;
    if(!check(setUnderwearHidden(3,false,error)&&setExternalUnderwear(false,error),"take control of the combined base part"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureVisible&&fixtureVisibility==1.f&&query(scene.data(),&key,table.data(),0)==1.f&&query(otherScene.data(),&key,table.data(),0)==0.f,
        "global takeover keeps regular coverage rules for a witch whose outfit is still present"))return false;
    setOutfitHiddenActor(reinterpret_cast<uintptr_t>(fixtureOther),fixtureRendererAddress,0,false,true,true,true);
    if(!check(query(otherScene.data(),&key,table.data(),0)==1.f,"global takeover also shows another witch's exposed base part"))return false;
    if(!check(setUnderwearHidden(3,true,error),"hide after takeover"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(!fixtureVisible&&(baseUnderwearPreference().hiddenMask&8),"Hex Appeal hides the combined part and exposes the effective SDK preference"))return false;
    if(!check(setUnderwearHidden(3,false,error),"show after takeover"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureVisible,"the base part can be shown again under Hex Appeal control"))return false;
    r.loadInterface(saved);
    if(!check(!r.allowExternalUnderwear,"global takeover survives reloading the interface"))return false;
    DependencyStatus named;named.underwearSources[3]="No Underwear – NPCs";r.publishDependencies(named);
    if(!check(r.underwearMountedExternalMask==0,"a name alone does not establish mounted ownership"))return false;
    named.confirmedUnderwearRules=8;
    r.publishDependencies(named);
    if(!check(setExternalUnderwear(true,error)&&underwearOwnership(3).fullyExternal&&underwearOwnership(3).source==named.underwearSources[3]&&
        query(otherScene.data(),&key,table.data(),0)==0.f&&underwearOwnership(-1).delegated&&!underwearOwnership(-1).fullyExternal,
        "hand back control and identify the mounted source"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(!fixtureVisible&&fixtureVisibility==0.f&&query(scene.data(),&key,table.data(),0)==0.f,"release restores the external visibility policy"))return false;
    applied.clear();
    setOutfitHiddenActor(fixtureAddress,fixtureRendererAddress,3,false,true,false,true);
    const auto writesBefore=fixtureWrites;
    named.missingUnderwearMeshMask=8;
    r.publishDependencies(named);
    if(!check(setExternalUnderwear(false,error)&&setUnderwearHidden(3,true,error),"save Hex Appeal visibility for a witch with no base mesh"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureWrites==writesBefore&&!fixtureVisible&&underwearOwnership(3).meshMissing&&
        !underwearOwnership(3).delegated&&(baseUnderwearPreference().hiddenMask&8),
        "known missing mesh skips native writes and retains the independent SDK hide preference"))return false;
    if(!check(setExternalUnderwear(true,error)&&underwearOwnership(3).delegated&&!(baseUnderwearPreference().hiddenMask&8),
        "resuming external control suspends the saved preference"))return false;
    r.publishDependencies({});r.underwearObservedExternalMask=0;
    if(!check(r.allowExternalUnderwear&&!underwearOwnership(3).delegated&&(baseUnderwearPreference().hiddenMask&8),
        "without a detected owner the per-witch hide preference applies even with external control allowed"))return false;
    const auto globalRevision=r.underwearVersion.load();
    const auto lockedOwnership=CreateFileW(saved.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(!check(lockedOwnership!=INVALID_HANDLE_VALUE,"lock ownership preference fixture"))return false;
    const bool ownershipRejected=!setExternalUnderwear(false,error);
    CloseHandle(lockedOwnership);
    if(!check(ownershipRejected&&r.allowExternalUnderwear&&r.underwearVersion==globalRevision,
        "failed ownership save leaves the global preference and SDK revision intact"))return false;
    constexpr uint32_t masks[]={0,1,8,21,63};
    for(bool allow:{false,true})for(auto savedMask:masks)for(auto mountedMask:masks)for(auto observedMask:masks){
        r.allowExternalUnderwear=allow;
        r.underwearHiddenMask=savedMask;
        r.underwearMountedExternalMask=mountedMask;
        r.underwearObservedExternalMask=observedMask;
        const auto delegated=allow?(mountedMask|observedMask):0u;
        if(!check(baseUnderwearPreference().hiddenMask==(savedMask&~delegated),
            "hide preferences survive overlapping mounted and renderer hiding mods"))return false;
        for(int witch=0;witch<6;++witch)
            if(!check(underwearOwnership(witch).delegated==bool(delegated&(1u<<witch)),
                "ownership on one witch must not block another"))return false;
    }
    const auto addOnRoot=profile.parent_path()/L"shared-base-fixture";
    std::filesystem::create_directories(addOnRoot/L"Characters");
    std::ofstream(addOnRoot/L"Characters/base-test.tsv")<<
        "HexAppealCharacters\t1\ncharacter\tbase_test\tBase Test\t6042\tBase_Test\tcharacter/appearance/3_npc/base_test.app_xml\n"
        "default\tbase_test\tRobe\tcd_phw_00_ub_00_0158\n";
    initializeCharacterAddons(addOnRoot);
    recordAddonVenusBody(firstAddonCharacter,true);
    if(!check(addonVenusUnderwearPresent(firstAddonCharacter),"the add-on uses the mounted body underwear"))return false;
    fixtureIdentity=[](void* observed,ActorIdentity& result){
        if(observed!=fixtureActor)return false;
        result.witch=firstAddonCharacter;strcpy_s(result.name,"Base_Test");return true;
    };
    fixtureIdentity(fixtureActor,identity);
    r.underwearHiddenMask=0;r.underwearMountedExternalMask=0;r.underwearObservedExternalMask=0;r.allowExternalUnderwear=false;
    applied.clear();fixtureVisible=true;fixtureRule=.7f;
    setOutfitHiddenActor(fixtureAddress,fixtureRendererAddress,firstAddonCharacter,true,true,true);
    if(!check(saveAddonUnderwearHidden(firstAddonCharacter,true,error)&&query(scene.data(),&key,table.data(),0)==0.f,
        "shared underwear query masks the add-on's combined base part"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(!fixtureVisible&&!underwearStatus(firstAddonCharacter).pending&&!underwearOwnership(firstAddonCharacter).meshMissing,
        "shared renderer applies and confirms the add-on's visibility"))return false;
    if(!check(saveAddonUnderwearHidden(firstAddonCharacter,false,error),"restore the add-on's saved base preference"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureVisible&&r.underwearHiddenMask==0,"shared renderer restores the add-on without changing another character's defaults"))return false;
    for(size_t target=firstAddonCharacter;target<characterCapacity;++target){
        recordExternalUnderwear(r,int(target),true);
        if(!check(observedExternalUnderwear(r,int(target))&&r.underwearObservedExternalMask.load()==0,
            "a high add-on observation never changes a built-in character's underwear policy"))return false;
    }
    r.allowExternalUnderwear=true;
    const int lastAddon=int(characterCapacity)-1;
    recordExternalUnderwear(r,firstAddonCharacter,false);
    if(!check(!usesExternalUnderwear(r,firstAddonCharacter)&&usesExternalUnderwear(r,lastAddon)&&!usesExternalUnderwear(r,0),
        "delegation on one add-on is independent of every other character"))return false;
    addonObservedExternalUnderwear.clear();
    fixtureTrackVisibility=false;fixtureRule=.7f;
    fixtureIdentity=nullptr;applied.clear();return true;
}
#endif
}
