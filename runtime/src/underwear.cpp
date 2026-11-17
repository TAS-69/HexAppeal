#include "underwear.h"
#include "runtime.h"
#include "core/log.h"
#include "wardrobe_native.h"
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>

namespace hex {
namespace {
using PartQuery=float(*)(void*,const uint32_t*,void*,uint8_t);
using SetPart=void(*)(void*,const uint32_t*,float,uint32_t,uint8_t);
using GetPart=bool(*)(void*,const uint32_t*);
using Intern=void*(*)(uint32_t*,const char*,uint32_t,uint32_t);
HMODULE surgeryModule=nullptr;
PartQuery originalQuery=nullptr;SetPart setPart=nullptr;GetPart getPart=nullptr;Intern intern=nullptr;
std::atomic<bool> ready{false};
std::atomic<uint32_t> underwearKey{0};
std::array<std::atomic<uint64_t>,6> confirmed{};
std::array<std::atomic<DWORD>,6> seen{};
std::mutex statusMutex,cacheMutex,preferenceMutex;
std::string status="The underwear controls are waiting for the game.";
struct Applied {int witch=-1;DWORD tick=0;bool hidden=false;uint64_t version=0;uint64_t failedVersion=0;DWORD failureLoggedAt=0;void* renderer=nullptr;float normal=1.f;bool normalKnown=false;};
std::map<uintptr_t,Applied> applied;
OutfitVisibility garmentVisibility;
#ifdef HEX_UNDERWEAR_TESTS
bool(*fixtureIdentity)(void*,ActorIdentity&)=nullptr;
#endif
bool identityOf(void* actor,ActorIdentity& identity) {
#ifdef HEX_UNDERWEAR_TESTS
    if(fixtureIdentity)return fixtureIdentity(actor,identity);
#endif
    return readWitchIdentity(surgeryModule,actor,identity);
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
    uint32_t key=0;ActorIdentity identity;
    void* actor=nullptr;
    if(ready&&keyGuard(&key)&&keyEqual(part,key)&&(actor=queryActorGuard(scene))&&identityOf(actor,identity)) {
        seen[identity.witch]=GetTickCount();
        if(std::isfinite(normal)&&normal>=0.f) {
            auto* renderer=rendererGuard(actor);
            std::lock_guard<std::mutex> lock(cacheMutex);auto& entry=applied[reinterpret_cast<uintptr_t>(actor)];
            if(entry.renderer!=renderer){entry=Applied{};entry.renderer=renderer;}
            entry.witch=identity.witch;entry.normal=normal;entry.normalKnown=true;
        }
        auto& runtime=Runtime::get();if((!runtime.dependencyScanned||runtime.shapingAvailable)&&(runtime.underwearHiddenMask.load()&(1u<<identity.witch)))return 0.f;
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
bool bytesGuard(const unsigned char* ptr,const unsigned char* bytes,size_t count) {
    __try {return memcmp(ptr,bytes,count)==0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void* jumpGuard(void* address) {
    __try {
        auto* p=static_cast<unsigned char*>(address);
        if(p[0]==0xe9)return p+5+*reinterpret_cast<int32_t*>(p+1);
        if(p[0]==0xff&&p[1]==0x25)return *reinterpret_cast<void**>(p+6+*reinterpret_cast<int32_t*>(p+2));
        if(p[0]==0x48&&p[1]==0xb8&&p[10]==0xff&&p[11]==0xe0)return *reinterpret_cast<void**>(p+2);
        return nullptr;
    }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
}
bool initializeUnderwear(HMODULE surgery,std::string& error) {
    const auto fail=[&](const char* message){error=message;Runtime::get().reportCompatibility(Feature::Underwear,CompatibilityState::Blocked,message);std::lock_guard<std::mutex> lock(statusMutex);status=message;return false;};
    auto* game=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    static const unsigned char showBytes[]={0x48,0x83,0xec,0x68,0x8b,0x02,0x4c,0x8d,0x44,0x24,0x40,0x45,0x33,0xd2};
    static const unsigned char getBytes[]={0x48,0x83,0xec,0x28,0x48,0x8b,0x01,0x4c,0x8b,0x40,0x08};
    static const unsigned char internBytes[]={0x48,0x89,0x5c,0x24,0x20,0x44,0x89,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10};
    static const unsigned char queryBytes[]={0x48,0x89,0x5c,0x24,0x08,0x45,0x0f,0xb6,0x58,0x20,0x48,0x8b,0xd9};
    auto* target=game+0xb9bf80;
    if(!bytesGuard(game+0x3043cf0,showBytes,sizeof(showBytes))||!bytesGuard(game+0x3043da0,getBytes,sizeof(getBytes))||!bytesGuard(game+0x14de770,internBytes,sizeof(internBytes))) {
        return fail("The game's underwear functions differ from the inspected build.");
    }
    if(!bytesGuard(target,queryBytes,sizeof(queryBytes))) {
        const auto creator=GetModuleHandleW(L"CharacterCreator.asi");void* next=target;HMODULE owner=nullptr;
        for(int i=0;i<4&&next;++i) {
            next=jumpGuard(next);owner=nullptr;
            if(next&&GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(next),&owner)&&owner==creator)break;
        }
        wchar_t path[32768]{};if(creator)GetModuleFileNameW(creator,path,32768);
        if(!creator||owner!=creator) {
            return fail("Another visibility hook could not be identified safely. The underwear toggle is unavailable.");
        }
    }
    surgeryModule=surgery;setPart=reinterpret_cast<SetPart>(game+0x3043cf0);getPart=reinterpret_cast<GetPart>(game+0x3043da0);intern=reinterpret_cast<Intern>(game+0x14de770);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&query),reinterpret_cast<void**>(&originalQuery))!=MH_OK||MH_EnableHook(target)!=MH_OK) {
        return fail("The underwear visibility route could not be installed.");
    }
    ready=true;{std::lock_guard<std::mutex> lock(statusMutex);status="Ready. Controls the base Underwear part and opted-in Wardrobe providers.";}
    Runtime::get().reportCompatibility(Feature::Underwear,CompatibilityState::Ready,"Base visibility functions and hook ownership checks passed.");
    LOG("Underwear part query installed for the checked game. Original body and equipped outfit assets are retained.");return true;
}
UnderwearStatus underwearStatus(int target) {
    if(target< -1||target>5)return {false,false,false,true,"Choose a supported witch."};
    UnderwearStatus result;result.ready=ready;const auto& r=Runtime::get();const auto mask=r.underwearHiddenMask.load();
    const uint32_t targets=target<0?63u:1u<<target;result.hidden=(mask&targets)==targets;result.mixed=(mask&targets)&&!result.hidden;
    result.pending=false;const auto now=GetTickCount();
    for(int i=0;i<6;++i)if(targets&(1u<<i))if(!seen[i]||now-seen[i]>3000||confirmed[i]!=r.underwearVersion)result.pending=true;
    {std::lock_guard<std::mutex> lock(statusMutex);result.detail=status;}
    return result;
}
UnderwearPreference baseUnderwearPreference() {
    std::lock_guard<std::mutex> lock(preferenceMutex);const auto& r=Runtime::get();return {r.underwearHiddenMask.load(),r.underwearVersion.load()};
}
bool setUnderwearHidden(int target,bool hidden,std::string& error) {
    const auto dependency=Runtime::get().dependencyStatus();const bool filesReady=dependency.scanned&&dependency.game.valid&&dependency.instant.valid&&dependency.venus.valid&&dependency.bodyVerified;
    if(!ready&&!filesReady){error="The native underwear route is not ready.";return false;}
    if(dependency.scanned&&!filesReady){error="Locate the installed body and editor files before using base underwear.";return false;}
    if(target< -1||target>5){error="Choose a supported witch.";return false;}
    std::lock_guard<std::mutex> lock(preferenceMutex);
    auto& r=Runtime::get();const auto before=r.underwearHiddenMask.load();const auto targets=target<0?63u:1u<<target;
    const auto after=hidden?(before|targets):(before&~targets);if(after==before)return true;
    r.underwearHiddenMask=after;
    if(!r.saveUnderwearPreference(after,error)){r.underwearHiddenMask=before;return false;}
    ++r.underwearVersion;return true;
}
void updateUnderwear(void* actor,const ActorIdentity& identity) {
    if(!ready||identity.witch<0||identity.witch>5)return;
#ifndef HEX_UNDERWEAR_TESTS
    OutfitVisibilityPolicy policy;ownedOutfitVisibility(reinterpret_cast<uintptr_t>(actor),identity.witch,policy);
    garmentVisibility.update(reinterpret_cast<uintptr_t>(actor),reinterpret_cast<uintptr_t>(rendererGuard(actor)),identity.witch,policy,GetTickCount64(),visibilityCalls());
#endif
    uint32_t key=0;if(!keyGuard(&key))return;
    auto& r=Runtime::get();const auto version=r.underwearVersion.load();const bool hidden=(!r.dependencyScanned||r.shapingAvailable)&&(r.underwearHiddenMask.load()&(1u<<identity.witch))!=0;
    const auto address=reinterpret_cast<uintptr_t>(actor);const auto now=GetTickCount();
    auto* renderer=rendererGuard(actor);Applied entry;
    {std::lock_guard<std::mutex> lock(cacheMutex);entry=applied[address];}
    if(entry.renderer!=renderer){entry=Applied{};entry.renderer=renderer;}
    seen[identity.witch]=now;
    if(entry.witch==identity.witch&&entry.version==version&&now-entry.tick<500)return;
    if(entry.witch==identity.witch&&entry.failedVersion==version&&now-entry.tick<500)return;
    if(hidden||entry.hidden) {
        const bool baseline=entry.normalKnown||(renderer&&currentGuard(renderer,key,&entry.normal));
        entry.normalKnown=baseline;
        if(!baseline||!changeGuard(renderer,key,hidden?0.f:entry.normal)) {
            if(entry.witch!=identity.witch||entry.failedVersion!=version||now-entry.failureLoggedAt>2000) {
                LOG_ERR("Underwear part update is not ready for %s actor=%p renderer=%p key=%u.",identity.name,actor,renderer,key);entry.failureLoggedAt=now;
            }
            entry.failedVersion=version;
            entry.tick=now;entry.witch=identity.witch;
            {std::lock_guard<std::mutex> lock(cacheMutex);applied[address]=entry;}return;
        }
        if(entry.witch!=identity.witch||entry.version!=version||entry.hidden!=hidden)LOG("Underwear %s on %s (key=%u).",hidden?"hidden":"restored to outfit rules",identity.name,key);
    }
    entry.witch=identity.witch;entry.tick=now;entry.hidden=hidden;entry.version=version;entry.failedVersion=0;
    std::lock_guard<std::mutex> lock(cacheMutex);

    const auto latest=applied.find(address);
    if(latest!=applied.end()&&latest->second.renderer==renderer&&latest->second.normalKnown){entry.normal=latest->second.normal;entry.normalKnown=true;}
    applied[address]=entry;confirmed[identity.witch]=version;
    for(auto i=applied.begin();i!=applied.end();)if(now-i->second.tick>30000)i=applied.erase(i);else++i;
}
bool restoreOutfitVisibility(void* actor){
    return !ready||garmentVisibility.restore(reinterpret_cast<uintptr_t>(actor),reinterpret_cast<uintptr_t>(rendererGuard(actor)),GetTickCount64(),visibilityCalls());
}
#ifdef HEX_UNDERWEAR_TESTS
namespace {
void* fixtureActor=nullptr;void* fixtureOther=nullptr;void* fixtureRenderer=nullptr;
unsigned fixtureWrites=0;float fixtureVisibility=-1;bool fixtureArguments=true,fixtureVisible=true;
bool fixtureWho(void* actor,ActorIdentity& identity) {
    if(actor!=fixtureActor&&actor!=fixtureOther)return false;
    identity.witch=actor==fixtureActor?3:0;strcpy_s(identity.name,actor==fixtureActor?"NHW_Unique_Wisdom_Witch_Store":"NHW_Unique_Modesty_Witch_Store");return true;
}
void* fixtureIntern(uint32_t* key,const char* text,uint32_t mode,uint32_t domain) {
    if(strcmp(text,"Underwear")||mode!=1||domain!=0x2ffff)fixtureArguments=false;*key=0x12345;return key;
}
float fixtureQuery(void*,const uint32_t*,void*,uint8_t){return .7f;}
bool fixtureGet(void* renderer,const uint32_t* key){if(renderer!=fixtureRenderer||*key!=0x12345)fixtureArguments=false;return fixtureVisible;}
void fixtureSet(void* renderer,const uint32_t* key,float value,uint32_t mode,uint8_t flag) {
    ++fixtureWrites;fixtureVisibility=value;
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
    r.underwearHiddenMask=0;r.underwearVersion=1;underwearKey=0;ready=true;applied.clear();fixtureWrites=0;
    ActorIdentity identity;fixtureWho(fixtureActor,identity);uint32_t key=0x12345,unrelated=0x65432;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureWrites==0&&!underwearStatus(3).pending,"normal outfit rules must not force visibility"))return false;
    if(!check(setUnderwearHidden(3,true,error)&&r.underwearHiddenMask==8,"targeted nude-base selection"))return false;
    if(!check(query(scene.data(),&key,table.data(),0)==0&&query(otherScene.data(),&key,table.data(),0)==.7f&&query(scene.data(),&unrelated,table.data(),0)==.7f&&query(nullptr,&key,table.data(),0)==.7f,"only the selected witch's Underwear part can be hidden"))return false;
    if(!check(query(fixtureActor,&key,table.data(),0)==.7f,"an actor must not be mistaken for a visibility context"))return false;
    updateUnderwear(fixtureActor,identity);
    if(!check(fixtureWrites==1&&fixtureVisibility==0&&!underwearStatus(3).pending&&fixtureArguments,"native visibility argument layout and acknowledgement"))return false;
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
    if(!check(!setUnderwearHidden(6,true,error)&&r.underwearHiddenMask==55,"invalid target rejection"))return false;
    ready=false;
    if(!check(!setUnderwearHidden(3,true,error)&&r.underwearHiddenMask==55,"unavailable route cannot silently save an applied toggle"))return false;
    fixtureIdentity=nullptr;applied.clear();return true;
}
#endif
}
