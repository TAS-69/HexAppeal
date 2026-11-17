#include "wardrobe.h"
#include "wardrobe_snapshot.h"
#include "wardrobe_native.h"
#include "runtime.h"
#include "core/log.h"
#include "hooks/frame_dispatch.h"
#include "appearance_hooks.h"
#include <MinHook.h>
#include <array>
#include <cstring>
#include <mutex>
#include <set>
#include <map>
#include <string>

namespace hex {
namespace {
using Merge=intptr_t(*)(uintptr_t,uintptr_t,uintptr_t);
using Copy=intptr_t(*)(uintptr_t,uintptr_t);
Merge originalMerge=nullptr;Copy originalCopy=nullptr;
thread_local int currentWitch=-1;
thread_local uintptr_t currentNode=0;
std::mutex mutex;
std::set<std::string> observedParts;
std::set<uintptr_t> observedActors;
struct ActorRef {int witch=-1;uintptr_t control=0,controller=0,appearance=0;DWORD seen=0,snapshotAt=0;uintptr_t node=0;bool snapshotValid=false,defaultGarment=false,customGarment=false;};
std::map<uintptr_t,ActorRef> actorRefs;
std::atomic<bool> refreshReady{false};
std::atomic<uint32_t> refreshMask{0};
std::array<std::atomic<uint64_t>,6> assemblyGeneration{};
std::string refreshStatus="Outfit refresh diagnostics are disabled.";
std::string appearanceWarning;
bool textGuard(uintptr_t wrapper,char* output,size_t capacity) {
    __try {
        if(!wrapper)return false;const auto* text=*reinterpret_cast<const char*const*>(wrapper);const auto length=*reinterpret_cast<const uint32_t*>(wrapper+8);
        if(!text||!length||length>=capacity)return false;
        for(size_t i=0;i<length;++i){const unsigned char c=text[i];if(c<32||c>126)return false;output[i]=char(c);}output[length]=0;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
uintptr_t pointerGuard(uintptr_t address){__try{return address?*reinterpret_cast<const uintptr_t*>(address):0;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
uint32_t wordGuard(uintptr_t address){__try{return address?*reinterpret_cast<const uint32_t*>(address):0;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
int classify(uintptr_t node) {
    char path[512]{};if(!textGuard(pointerGuard(node+0x18),path,sizeof(path)))return -1;
    struct Row{const char* suffix;int witch;};
    const Row rows[]={{"cd_nhw_south_20166.app_xml",0},{"cd_nhw_north_20026.app_xml",1},{"cd_nhw_south_20167.app_xml",1},{"cd_nhw_south_20165.app_xml",2},{"cd_nhw_south_20168.app_xml",3},{"cd_nhw_south_20169.app_xml",4},{"cd_m0001_00_phw_north_44001.app_xml",5}};
    const std::string value=path;for(const auto& row:rows){const auto n=strlen(row.suffix);if(value.size()>=n&&value.compare(value.size()-n,n,row.suffix)==0)return row.witch;}return -1;
}
intptr_t merge(uintptr_t node,uintptr_t input,uintptr_t output) {
    const auto previous=currentWitch;const auto previousNode=currentNode;currentWitch=classify(node);currentNode=node;
    if(currentWitch>=0)++assemblyGeneration[currentWitch];
    intptr_t result=0;
    __try {result=originalMerge(node,input,output);}
    __finally {currentWitch=previous;currentNode=previousNode;}
    return result;
}
intptr_t copy(uintptr_t destination,uintptr_t source) {
    if(currentWitch>=0){
        char name[192]{};
        if(textGuard(pointerGuard(source),name,sizeof(name))){
            const auto key=std::to_string(currentWitch)+":"+name;
            std::lock_guard<std::mutex> guard(mutex);
            if(observedParts.size()<128&&observedParts.insert(key).second)LOG("Wardrobe observed witch=%d node=%p prefab=%s wrapper=%p.",currentWitch,reinterpret_cast<void*>(currentNode),name,reinterpret_cast<void*>(pointerGuard(source)));
        }
    }
    return originalCopy(destination,source);
}
bool bytesGuard(void* address,const unsigned char* bytes,size_t size){__try{return memcmp(address,bytes,size)==0;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool knownJump(void* address) {
    return appearance::knownJump(address,GetModuleHandleW(L"SpouseOverhaul.asi"),GetModuleHandleW(L"CharacterCreator.asi"));
}
void registry() {
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));const auto loader=pointerGuard(base+0x7160288);if(!loader)return;
    const auto table=loader+0x50;const auto count=wordGuard(table+4),capacity=wordGuard(table+8);const auto entries=pointerGuard(table+0x18);
    if(!entries||count>capacity||count>100000)return;
    unsigned printed=0;
    for(uint32_t i=0;i<count&&printed<24;++i){const auto row=pointerGuard(entries+i*8),wrapper=pointerGuard(row+0x08);char name[192]{};
        if(textGuard(wrapper,name,sizeof(name))&&(strstr(name,"0191")||strstr(name,"uw_00_0001")||strstr(name,"uw_0001"))){LOG("Wardrobe registry prefab=%s wrapper=%p.",name,reinterpret_cast<void*>(wrapper));++printed;}}
}
bool rebuildGuard(uintptr_t controller,uintptr_t appearance) {
    __try {
        if(!controller||!appearance||pointerGuard(controller+0x138)!=appearance)return false;
        const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if(pointerGuard(controller)!=base+0x59a9548||pointerGuard(appearance)!=base+0x5dff260)return false;
        reinterpret_cast<void(*)(void*,void*)>(base+0x82d7f0)(reinterpret_cast<void*>(controller),reinterpret_cast<void*>(appearance));return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void refreshFrame() noexcept {
    static DWORD lastTick=0;const auto tickNow=GetTickCount();
    if(tickNow-lastTick>=150){lastTick=tickNow;try{
        std::map<uintptr_t,ActorRef> instances;{std::lock_guard<std::mutex> guard(mutex);instances=actorRefs;}
        for(const auto& row:instances)if(tickNow-row.second.seen<2000){ActorIdentity identity;
            if(readWitchIdentity(GetModuleHandleW(L"InstantSurgery.asi"),reinterpret_cast<void*>(row.first),identity)&&identity.witch==row.second.witch)tickNativeWardrobe(row.first,identity.witch);}
    }catch(...){LOG_ERR("Wardrobe update could not finish.");}}
    const auto requested=refreshMask.exchange(0);if(!requested)return;
    try {
        const auto now=GetTickCount();std::map<uintptr_t,ActorRef> candidates;
        {std::lock_guard<std::mutex> guard(mutex);candidates=actorRefs;}
        unsigned attempted=0,done=0,assembled=0;
        for(const auto& row:candidates){const auto actor=row.first;const auto& ref=row.second;
            if(ref.witch<0||ref.witch>5||!(requested&(1u<<ref.witch))||now-ref.seen>2000)continue;
            ActorIdentity identity;
            if(!readWitchIdentity(GetModuleHandleW(L"InstantSurgery.asi"),reinterpret_cast<void*>(actor),identity)||identity.witch!=ref.witch)continue;
            const auto components=pointerGuard(actor+0x68),control=pointerGuard(components+0x40);
            const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            if(pointerGuard(actor)!=base+0x5991108||pointerGuard(control)!=base+0x59c1660)continue;
            if(control!=ref.control||pointerGuard(control+8)!=actor||pointerGuard(pointerGuard(control+0xb8)+0x20)!=ref.controller||pointerGuard(ref.controller+0x138)!=ref.appearance)continue;
            const auto before=assemblyGeneration[ref.witch].load();
            ++attempted;const auto success=rebuildGuard(ref.controller,ref.appearance);if(success)++done;
            const bool rebuilt=assemblyGeneration[ref.witch].load()!=before;if(rebuilt)++assembled;
            LOG("Wardrobe refresh call witch=%d actor=%p controller=%p thread=%u call_completed=%d assembly_observed=%d. Shared appearance data was not changed.",ref.witch,reinterpret_cast<void*>(actor),reinterpret_cast<void*>(ref.controller),GetCurrentThreadId(),success,rebuilt);
        }
        {std::lock_guard<std::mutex> guard(mutex);refreshStatus=done==attempted&&done?(assembled?"Refresh call completed; outfit assembly observed.":"Refresh call completed; no outfit assembly requested."):attempted?"A refresh call failed. Check the log before retrying.":"No recently visible selected witch was available.";}
    }catch(...){std::lock_guard<std::mutex> guard(mutex);refreshStatus="Outfit refresh could not finish.";}
}
}
bool initializeWardrobeObservation(std::string& error) {
    auto* game=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto spouse=GetModuleHandleW(L"SpouseOverhaul.asi"),creator=GetModuleHandleW(L"CharacterCreator.asi");
    std::string warnings;
    for(const auto mod:{spouse,creator})if(mod){wchar_t path[32768]{};GetModuleFileNameW(mod,path,32768);const auto digest=sha256(path);
        const auto warning=appearance::versionWarning(mod==spouse?"Spouse Overhaul":"Character Creator",digest,mod==spouse?"653a650945ee0fcfd5e3c70bf584b27a860bda88098e58e44abd4715b8df4513":"de9b8b3acf79dce35d219cc9daf2f1daf7f6e482950cab389fc2dd4b8f236b32");
        if(!warning.empty()){if(!warnings.empty())warnings+='\n';warnings+=warning;LOG("Wardrobe compatibility: %s SHA256=%s",warning.c_str(),digest.c_str());}}
    {std::lock_guard<std::mutex> guard(mutex);appearanceWarning=warnings;}
    static const unsigned char mergeBytes[]={0x48,0x89,0x54,0x24,0x10,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    static const unsigned char copyBytes[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x6c,0x24,0x20,0x48,0x89,0x4c,0x24,0x08};
    auto* mergeTarget=game+0x2f94ce0;auto* copyTarget=game+0x5477a0;
    if((!bytesGuard(mergeTarget,mergeBytes,sizeof(mergeBytes))&&!knownJump(mergeTarget))||(!bytesGuard(copyTarget,copyBytes,sizeof(copyBytes))&&!knownJump(copyTarget))){error="Appearance assembly signatures differ";return false;}
    if(MH_CreateHook(mergeTarget,reinterpret_cast<void*>(&merge),reinterpret_cast<void**>(&originalMerge))!=MH_OK){error="Appearance observer unavailable";return false;}
    if(MH_CreateHook(copyTarget,reinterpret_cast<void*>(&copy),reinterpret_cast<void**>(&originalCopy))!=MH_OK){MH_RemoveHook(mergeTarget);error="Part observer unavailable";return false;}
    if(MH_QueueEnableHook(mergeTarget)!=MH_OK||MH_QueueEnableHook(copyTarget)!=MH_OK||MH_ApplyQueued()!=MH_OK){MH_DisableHook(mergeTarget);MH_DisableHook(copyTarget);MH_RemoveHook(mergeTarget);MH_RemoveHook(copyTarget);error="Appearance observer could not start";return false;}
    registry();LOG("Wardrobe observation installed. Outfit writes use a separate development gate; source records are not modified.");
    static const unsigned char rebuildBytes[]={0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x48,0x8b,0xec,0x48,0x81,0xec,0x80,0x00,0x00,0x00};
    if(!bytesGuard(game+0x82d7f0,rebuildBytes,sizeof(rebuildBytes))&&!knownJump(game+0x82d7f0)){error="The outfit rebuild entry differs from the supported interface.";return false;}
    std::string frameError;
    if(!initializeFrameDispatch(refreshFrame,frameError)){error="Outfit frame dispatch unavailable: "+frameError;return false;}
    refreshReady=true;{std::lock_guard<std::mutex> guard(mutex);refreshStatus="Read-only assembly observation and checked game-thread refresh are ready.";}
    std::string wardrobeError;if(!initializeNativeWardrobe(wardrobeError)){reportWardrobeUnavailable(wardrobeError);LOG_ERR("Wardrobe switching unavailable: %s",wardrobeError.c_str());}
    return true;
}
void observeWardrobeActor(void* actor,const ActorIdentity& identity) {
    const auto address=reinterpret_cast<uintptr_t>(actor);std::lock_guard<std::mutex> guard(mutex);
    const auto components=pointerGuard(address+0x68),control=pointerGuard(components+0x40);
    if(!control||pointerGuard(control+8)!=address)return;
    const auto holder=pointerGuard(control+0xb8),customization=pointerGuard(holder+0x20);if(!customization)return;
    auto& ref=actorRefs[address];const auto now=GetTickCount();
    ref.witch=identity.witch;ref.control=control;ref.controller=customization;ref.appearance=pointerGuard(customization+0x138);ref.seen=now;
    if(!ref.snapshotAt||now-ref.snapshotAt>=500){
        WardrobeSnapshot snapshot;const bool captured=captureWardrobeSnapshot(address,identity.witch,reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),snapshot);
        const bool changed=captured&&(!ref.snapshotValid||ref.node!=snapshot.node||ref.defaultGarment!=snapshot.defaultGarment||ref.customGarment!=snapshot.customGarment);
        ref.snapshotAt=now;ref.snapshotValid=captured;
        if(captured){ref.node=snapshot.node;ref.defaultGarment=snapshot.defaultGarment;ref.customGarment=snapshot.customGarment;
            if(changed)LOG("Wardrobe instance witch=%d actor=%p node=%p claims=%u ready=%u default=%d custom=%d. Read-only ownership verified.",identity.witch,actor,reinterpret_cast<void*>(snapshot.node),snapshot.claimCount,snapshot.readyCount,snapshot.defaultGarment,snapshot.customGarment);
        }
    }
    for(auto i=actorRefs.begin();i!=actorRefs.end();)if(GetTickCount()-i->second.seen>10000)i=actorRefs.erase(i);else++i;
    if(observedActors.size()>=32||observedActors.count(address))return;
    observedActors.insert(address);
    LOG("Wardrobe actor witch=%d actor=%p control=%p customization=%p appearance_table=%p renderer=%p.",identity.witch,actor,reinterpret_cast<void*>(control),reinterpret_cast<void*>(customization),reinterpret_cast<void*>(pointerGuard(customization+0x138)),reinterpret_cast<void*>(pointerGuard(control+0x130)));
}
WardrobeProbeStatus wardrobeProbeStatus(){std::lock_guard<std::mutex> guard(mutex);return {refreshReady.load(),refreshMask.load()!=0,refreshStatus,appearanceWarning};}
bool requestWardrobeRefresh(int target,std::string& error){
    if(!refreshReady||target< -1||target>5){error="Outfit refresh is unavailable.";return false;}
    refreshMask.fetch_or(target<0?63u:1u<<target);return true;
}
}
