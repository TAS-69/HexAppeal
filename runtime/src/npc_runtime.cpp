#include "npc_runtime.h"
#include "runtime.h"
#include "shaping.h"
#include "pose_access.h"
#include "pose_observation.h"
#include "mounted_dependencies.h"
#include "underwear.h"
#include "wardrobe.h"
#include "wardrobe_native.h"
#include "core/log.h"
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <cstdio>

namespace hex {
namespace {
struct Snapshot {
    uint64_t version=0;
    std::array<bool,6> enabled{};
    std::array<Profile,6> profiles{};
    std::array<SurgeryProfile,6> shapes{};
};
struct Transaction {
    std::shared_ptr<Snapshot> next;
    std::shared_ptr<const Snapshot> previous;
    uint32_t targets=0;
    uint32_t observed=0;
    uint32_t acknowledged=0;
    uint32_t expected=0;
    DWORD queuedAt=0;
    int state=0;
    std::string reason;
};
struct CachedPart {
    std::vector<ShapeBone> bones;
    std::vector<PoseMatrix> base;
    std::vector<PoseMatrix> written;
};
struct ActorCache {
    int witch=-1;
    DWORD last=0;
    std::map<uintptr_t,CachedPart> parts;
};
struct PhysicsReceipt {
    uint64_t version=0;
    uint64_t stamp=0;
    DWORD tick=0;
    bool supplied=false;
    std::vector<ShapeBone> bones;
    std::vector<PoseMatrix> animation;
};
struct RuntimeState {
    HMODULE module=nullptr;
    NativeShapeEngine engine;
    std::atomic<bool> hooked{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> allowWrites{false};
    std::atomic<uint32_t> seen{0};
    std::array<std::atomic<DWORD>,6> lastSeen{};
    std::mutex mutex;
    std::mutex applicationMutex;
    std::mutex engineMutex;
    std::atomic<bool> dependenciesControlled{false};
    std::string status="Waiting for Instant Surgery.";
    std::map<uint64_t,Transaction> transactions;
    uint64_t sequence=0;
    std::shared_ptr<const Snapshot> active=std::make_shared<Snapshot>();
    std::map<uintptr_t,ActorCache> actors;
    std::atomic<uint64_t> persistVersion{0};
    std::filesystem::path observations;
    std::array<uint32_t,6> capturedFrames{};
    std::array<DWORD,6> capturedAt{};
    std::array<std::atomic<uint64_t>,6> physicsCalls{};
    bool physicsRoute=false;
    bool wardrobeObservation=false;
    int (*physicsApplied)(void*)=nullptr;
    std::map<std::pair<uintptr_t,uintptr_t>,PhysicsReceipt> receipts;
    std::array<uint32_t,6> receiptChecks{};
};
RuntimeState& state(){static RuntimeState s;return s;}
bool dependenciesAllow(const Profile& profile){
    if(!state().dependenciesControlled)return true;const auto& r=Runtime::get();
    return r.shapingAvailable&&(!(profile.top==Preset::Hex&&profile.topBlend>0)&&!(profile.bottom==Preset::Hex&&profile.bottomBlend>0)||r.hexAvailable)&&(profile.weight==0||r.weightAvailable);
}
using UpdateFn=void(*)(void*,void*,void*);
UpdateFn originalUpdate=nullptr;
using PhysicsPoseFn=int(*)(void*,void*,const PoseMatrix*,PoseMatrix*,uint32_t,uint64_t*);
PhysicsPoseFn originalPhysicsPose=nullptr;
using ResolveProfileFn=bool(*)(void*,SurgeryProfile*);
ResolveProfileFn originalResolveProfile=nullptr;
struct ProfileOverride {
    uintptr_t actor=0;
    const SurgeryProfile* shape=nullptr;
    bool supplied=false;
};
thread_local ProfileOverride* currentProfileOverride=nullptr;
bool copyProfileGuard(SurgeryProfile* out,const SurgeryProfile* shape) {
    __try {memcpy(out,shape,sizeof(*out));return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool hookedResolveProfile(void* actor,SurgeryProfile* output) {
    auto* choice=currentProfileOverride;
    if(choice&&choice->actor==reinterpret_cast<uintptr_t>(actor)&&choice->shape&&output) {
        choice->supplied=copyProfileGuard(output,choice->shape);return choice->supplied;
    }
    return originalResolveProfile(actor,output);
}
bool readStampGuard(const uint64_t* address,uint64_t* value) {
    __try {if(!address)return false;*value=*address;return *value!=0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool writeStampGuard(uint64_t* address,uint64_t value) {
    __try {if(!address)return false;*address=value;return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool physicsAppliedGuard(int(*query)(void*),uintptr_t part) {
    __try {return query&&query(reinterpret_cast<void*>(part))==1;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void setStatus(const std::string& value){auto& s=state();std::lock_guard<std::mutex> lock(s.mutex);s.status=value;}
void reason(char* out,uint32_t cap,const std::string& value){if(cap)snprintf(out,cap,"%s",value.c_str());}
uint32_t visibleMask() {
    uint32_t mask=0;const auto now=GetTickCount();auto& s=state();
    for(size_t i=0;i<6;++i){const auto last=s.lastSeen[i].load();if(last&&now-last<3000)mask|=1u<<i;}
    return mask;
}
int ready(char* out,uint32_t cap) {
    auto& s=state();std::string message;
    if(!s.allowWrites)message="Observation mode: body and outfit writes are disabled.";
    else if(s.dependenciesControlled&&!Runtime::get().shapingAvailable)message="Shaping is disabled until its dependencies are ready. Check Settings.";
    else if(s.failed)message="NPC shaping stopped after a rejected pose. Restart before retrying.";
    else if(!s.hooked)message="The Instant Surgery NPC route is not installed.";
    else if(!visibleMask())message="Keep a supported witch visible so her body and outfit buffers can be checked.";
    if(!message.empty()){reason(out,cap,message);return 0;}
    return 1;
}
int begin(uint32_t targets,uint64_t* id) {
    if(!id||!targets||targets>6)return 0;
    auto& s=state();std::lock_guard<std::mutex> lock(s.mutex);
    if(!s.allowWrites||!s.hooked||s.failed)return 0;
    for(const auto& item:s.transactions)if(item.second.state==0||item.second.state==1)return 0;
    while(s.transactions.size()>64)s.transactions.erase(s.transactions.begin());
    const auto current=std::atomic_load(&s.active);Transaction transaction;
    transaction.expected=targets;
    transaction.previous=current;transaction.next=std::make_shared<Snapshot>(*current);
    const auto number=++s.sequence;transaction.next->version=number;
    s.transactions[number]=std::move(transaction);*id=number;return 1;
}
int prepare(uint64_t id,const HexAppealTargetV1* target,const HexAppealProfileV1* input) {
    if(!target||!input||target->size!=sizeof(*target)||input->size!=sizeof(*input)||!input->autoFit)return 0;
    int witch=-1;
    for(size_t i=0;i<witches.size();++i)if(target->keyCount==witches[i].target.keyCount&&memcmp(target->characterRecordKeys,witches[i].target.characterRecordKeys,sizeof(target->characterRecordKeys))==0)witch=int(i);
    if(witch<0)return 0;
    Profile choice;choice.top=Preset(input->top);choice.bottom=Preset(input->bottom);choice.topBlend=input->topBlend;choice.bottomBlend=input->bottomBlend;choice.autoFit=input->autoFit!=0;choice.height=input->height;choice.weight=input->weight;
    ResolvedShape shape;std::string error;auto& s=state();
    {std::lock_guard<std::mutex> engineLock(s.engineMutex);if(!dependenciesAllow(choice)||!s.engine.compose(choice,shape,error)){setStatus(error.empty()?"A required dependency is unavailable.":error);return 0;}}
    std::lock_guard<std::mutex> lock(s.mutex);const auto found=s.transactions.find(id);
    if(found==s.transactions.end()||found->second.state!=0)return 0;
    if(found->second.targets&(1u<<witch))return 0;
    found->second.targets|=1u<<witch;found->second.next->profiles[witch]=choice;
    found->second.next->shapes[witch]=shape.native;found->second.next->enabled[witch]=true;return 1;
}
int commit(uint64_t id) {
    auto& s=state();std::lock_guard<std::mutex> application(s.applicationMutex);std::lock_guard<std::mutex> lock(s.mutex);const auto found=s.transactions.find(id);
    if(found==s.transactions.end()||found->second.state!=0||!found->second.targets||s.failed||!s.allowWrites||!s.hooked)return 0;
    auto& transaction=found->second;transaction.observed=transaction.targets&visibleMask();
    uint32_t prepared=0;for(uint32_t mask=transaction.targets;mask;mask>>=1)prepared+=mask&1;
    if(prepared!=transaction.expected||std::atomic_load(&s.active)!=transaction.previous)return 0;
    if(!transaction.observed){s.status="Keep a selected witch visible before applying her body and outfit combination.";return 0;}
    transaction.queuedAt=GetTickCount();
    transaction.state=1;transaction.reason="Queued for the next body and outfit upload.";
    std::atomic_store(&s.active,std::shared_ptr<const Snapshot>(transaction.next));return 1;
}
void abort(uint64_t id) {
    auto& s=state();std::lock_guard<std::mutex> application(s.applicationMutex);std::lock_guard<std::mutex> lock(s.mutex);const auto found=s.transactions.find(id);if(found==s.transactions.end())return;
    if(found->second.state==2)return;
    if(found->second.state==1&&std::atomic_load(&s.active)->version==id)std::atomic_store(&s.active,found->second.previous);
    found->second.state=-1;found->second.reason="The pending selection was cancelled.";
}
int poll(uint64_t id,char* out,uint32_t cap) {
    auto& s=state();std::lock_guard<std::mutex> lock(s.mutex);const auto found=s.transactions.find(id);
    if(found==s.transactions.end()){reason(out,cap,"Unknown fitting transaction.");return -1;}
    const auto& transaction=found->second;reason(out,cap,transaction.reason);
    return transaction.state==2?1:transaction.state<0?-1:0;
}
const HexAppealBackendV1 backend={sizeof(HexAppealBackendV1),HEX_APPEAL_BACKEND_ABI,255,
    "Instant Surgery 0.9.6 - Hex Appeal NPC integration",ready,begin,prepare,commit,abort,poll};
const HexAppealBackendV1* factory(std::string& detail) {
    auto& s=state();std::lock_guard<std::mutex> lock(s.mutex);detail=s.status;return s.hooked?&backend:nullptr;
}
void reject(uint64_t version,const std::string& message,bool stop=false) {
    auto& s=state();std::lock_guard<std::mutex> lock(s.mutex);s.status=message;
    const auto found=s.transactions.find(version);
    if(found!=s.transactions.end()&&(found->second.state==1||found->second.state==2)) {
        auto& transaction=found->second;
        if(std::atomic_load(&s.active)->version==version)std::atomic_store(&s.active,transaction.previous);
        transaction.state=-1;transaction.reason=message;
    }
    if(stop)s.failed=true;
}
void acknowledge(uint64_t version,int witch,size_t parts) {
    auto& s=state();std::lock_guard<std::mutex> lock(s.mutex);const auto found=s.transactions.find(version);
    if(found!=s.transactions.end()&&found->second.state==1) {
        auto& transaction=found->second;transaction.acknowledged|=1u<<witch;
        if(!transaction.observed)transaction.observed=1u<<witch;
        if((transaction.acknowledged&transaction.observed)==transaction.observed) {
            transaction.state=2;transaction.reason="Body and outfit update completed for the visible witches. Other selected witches use it when loaded.";
            s.persistVersion=version;
        }
    }
    s.status=std::string(witches[witch].name)+": updated "+std::to_string(parts)+" body/outfit pose buffers.";
}
bool updateActor(void* upload,void* model,void* actor,const ActorIdentity& identity) {
    auto& s=state();std::unique_lock<std::mutex> application(s.applicationMutex);std::vector<PosePart> parts;std::string error;
    const auto snapshot=std::atomic_load(&s.active);
    if(!capturePoseParts(upload,model,parts,error)) {
        const auto message=std::string(identity.name)+": "+error;
        setStatus(message);
        return false;
    }
    s.lastSeen[identity.witch]=GetTickCount();const auto first=s.seen.fetch_or(1u<<identity.witch);
    size_t changedParts=0;for(const auto& part:parts)changedParts+=part.source!=part.animation;
    if(!(first&(1u<<identity.witch))) {
        LOG("Observed %s key=%u actor=%p model=%p; %zu validated body/outfit pose buffers; %zu differ from animation source.",identity.name,identity.recordKey,actor,model,parts.size(),changedParts);
        for(size_t i=0;i<parts.size();++i)for(const auto renderPart:parts[i].renderParts)
            LOG("Observed render part witch=%d model=%p part=%p skeleton=%p bones=%zu.",identity.witch,model,reinterpret_cast<void*>(renderPart),reinterpret_cast<void*>(parts[i].skeleton),parts[i].bones.size());
    }
    if(!s.observations.empty()&&s.capturedFrames[identity.witch]<4&&GetTickCount()-s.capturedAt[identity.witch]>250) {
        const auto frame=s.capturedFrames[identity.witch]++;s.capturedAt[identity.witch]=GetTickCount();
        if(savePoseObservation(s.observations,identity,parts,frame,error))LOG("Captured read-only pose observation for %s, frame %u, %zu shared buffers.",identity.name,frame,parts.size());
        else LOG_ERR("Pose observation failed: %s",error.c_str());
    }
    if(s.physicsRoute) {
        std::vector<uintptr_t> queried;
        for(const auto& part:parts)for(const auto renderPart:part.renderParts)queried.push_back(renderPart);
        application.unlock();
        const auto nativeLock=reinterpret_cast<PSRWLOCK>(reinterpret_cast<unsigned char*>(s.module)+0x12A730);
        ReleaseSRWLockExclusive(nativeLock);
        bool applied=!queried.empty();for(const auto renderPart:queried)applied=physicsAppliedGuard(s.physicsApplied,renderPart)&&applied;
        AcquireSRWLockExclusive(nativeLock);
        application.lock();
        if(std::atomic_load(&s.active)!=snapshot)return true;
        bool sourcesMatch=true,versionsMatch=true;
        for(const auto& part:parts) {
            const auto receipt=s.receipts.find({reinterpret_cast<uintptr_t>(actor),part.skeleton});
            const bool matching=receipt!=s.receipts.end()&&GetTickCount()-receipt->second.tick<500&&receipt->second.animation==part.animation&&receipt->second.bones.size()==part.bones.size()&&memcmp(receipt->second.bones.data(),part.bones.data(),part.bones.size()*sizeof(ShapeBone))==0;
            sourcesMatch=matching&&sourcesMatch;
            versionsMatch=matching&&receipt->second.supplied&&receipt->second.version==snapshot->version&&versionsMatch;
        }
        if(s.receiptChecks[identity.witch]<4) {
            ++s.receiptChecks[identity.witch];
            LOG("Physics upload check %s: original_sources_match=%d editor_pose_applied=%d selected_version_received=%d buffers=%zu render_parts=%zu.",identity.name,sourcesMatch,applied,versionsMatch,parts.size(),queried.size());
        }
        if(!s.allowWrites||!snapshot->enabled[identity.witch]||!dependenciesAllow(snapshot->profiles[identity.witch])) {
            setStatus(std::string(witches[identity.witch].name)+": physics route observed; original pose match "+(sourcesMatch?"confirmed.":"pending."));return false;
        }
        if(!sourcesMatch||!versionsMatch||!applied) {
            setStatus(std::string(witches[identity.witch].name)+": waiting for the selected body and outfit physics uploads.");return true;
        }
        for(const auto& part:parts)if(!poseBytesEqual(part.destination,part.source))return true;
        acknowledge(snapshot->version,identity.witch,parts.size());return true;
    }
    auto& cache=s.actors[reinterpret_cast<uintptr_t>(actor)];
    if(cache.witch!=identity.witch){cache.parts.clear();cache.witch=identity.witch;}
    cache.last=GetTickCount();
    if(!s.allowWrites||!snapshot->enabled[identity.witch]||!dependenciesAllow(snapshot->profiles[identity.witch])) {
        if(s.allowWrites)for(auto& part:parts) {
            const auto previous=cache.parts.find(reinterpret_cast<uintptr_t>(part.destination));
            if(previous!=cache.parts.end()&&previous->second.written==part.source&&previous->second.base==part.animation)
                if(!writePoseBytes(part.destination,part.animation)){reject(snapshot->version,"Cancelled shape restoration failed. Restart the game.",true);return true;}
        }
        setStatus(std::string(witches[identity.witch].name)+": "+std::to_string(parts.size())+" pose buffers checked; "+std::to_string(changedParts)+" differ from the original animation. No shape selected for application.");
        return false;
    }
    std::vector<std::vector<PoseMatrix>> bases,outputs;
    for(auto& part:parts) {
        auto input=part.animation;const auto previous=cache.parts.find(reinterpret_cast<uintptr_t>(part.destination));
        const bool ours=previous!=cache.parts.end()&&previous->second.bones.size()==part.bones.size()&&memcmp(previous->second.bones.data(),part.bones.data(),part.bones.size()*sizeof(ShapeBone))==0&&previous->second.written==part.source&&previous->second.base==part.animation;
        if(part.source!=part.animation&&!ours) {reject(snapshot->version,"Another modifier changed a body or outfit pose. This selection was rejected before any write.");return true;}
        std::vector<PoseMatrix> output;
        if(!s.engine.deform(part.bones,input,snapshot->shapes[identity.witch],output,error,true)){reject(snapshot->version,std::string(identity.name)+": "+error);return true;}
        std::vector<PoseMatrix> proportioned;
        if(!applyStature(snapshot->profiles[identity.witch],output,proportioned,error)){reject(snapshot->version,error);return true;}
        output=std::move(proportioned);
        bases.push_back(std::move(input));outputs.push_back(std::move(output));
    }
    for(size_t i=0;i<parts.size();++i)if(!poseBytesEqual(parts[i].destination,parts[i].source)) {reject(snapshot->version,"Pose buffers changed during preparation; this selection was rejected before any write.");return true;}
    for(size_t i=0;i<parts.size();++i)if(!writePoseBytes(parts[i].destination,outputs[i])) {
        bool restored=true;for(size_t j=0;j<=i;++j)restored=writePoseBytes(parts[j].destination,parts[j].source)&&restored;
        reject(snapshot->version,restored?"Pose write failed; captured buffers restored and shaping stopped.":"Pose restoration failed; shaping stopped. Restart the game.",true);LOG_ERR("NPC body/outfit write failed; restored=%d",restored);return true;
    }
    for(size_t i=0;i<parts.size();++i)cache.parts[reinterpret_cast<uintptr_t>(parts[i].destination)]={parts[i].bones,std::move(bases[i]),std::move(outputs[i])};
    acknowledge(snapshot->version,identity.witch,parts.size());
    for(auto it=s.actors.begin();it!=s.actors.end();)if(GetTickCount()-it->second.last>30000)it=s.actors.erase(it);else++it;
    return true;
}
void hookedUpdate(void* upload,void* model,void* actor) {
    auto& s=state();ActorIdentity identity;
    const bool witch=readWitchIdentity(s.module,actor,identity);
    const bool handled=!s.failed&&witch&&updateActor(upload,model,actor,identity);
    if(!handled)originalUpdate(upload,model,actor);
    if(witch)updateUnderwear(actor,identity);
    if(witch&&s.wardrobeObservation)observeWardrobeActor(actor,identity);
}
int observedPhysicsPose(void* context,void* skeleton,const PoseMatrix* source,PoseMatrix* destination,uint32_t count,uint64_t* stamp) {
    auto& s=state();ActorIdentity identity;
    const bool witch=readContextWitchIdentity(s.module,context,identity);
    const auto snapshot=std::atomic_load(&s.active);
    PhysicsReceipt receipt;receipt.version=snapshot->version;
    const bool captured=witch&&capturePhysicsSource(skeleton,source,count,receipt.bones,receipt.animation);
    ProfileOverride choice;
    if(captured&&s.physicsRoute&&s.allowWrites&&!s.failed&&snapshot->enabled[identity.witch]&&dependenciesAllow(snapshot->profiles[identity.witch])) {
        choice.actor=identity.actor;choice.shape=&snapshot->shapes[identity.witch];
    }
    auto* previous=currentProfileOverride;currentProfileOverride=&choice;
    auto result=originalPhysicsPose(context,skeleton,source,destination,count,stamp);
    currentProfileOverride=previous;
    if(choice.supplied&&result==1&&linkedStature(snapshot->profiles[identity.witch])!=0.f) {
        std::vector<PoseMatrix> nativeOutput,proportioned;std::string error;uint64_t nativeStamp=0;
        if(!readPoseBytes(destination,count,nativeOutput)||!applyStature(snapshot->profiles[identity.witch],nativeOutput,proportioned,error)||!readStampGuard(stamp,&nativeStamp)||!writePoseBytes(destination,proportioned)||!writeStampGuard(stamp,statureStamp(nativeStamp,snapshot->profiles[identity.witch]))) {
            const bool restored=writePoseBytes(destination,receipt.animation);writeStampGuard(stamp,0);choice.supplied=false;result=0;
            reject(snapshot->version,error.empty()?"Stature pose could not be prepared":error,!restored);
        }
    }
    if(captured) {
        receipt.tick=GetTickCount();receipt.supplied=choice.supplied&&result==1&&readStampGuard(stamp,&receipt.stamp);
        std::lock_guard<std::mutex> application(s.applicationMutex);
        s.receipts[{identity.actor,reinterpret_cast<uintptr_t>(skeleton)}]=std::move(receipt);
        for(auto it=s.receipts.begin();it!=s.receipts.end();)if(GetTickCount()-it->second.tick>5000)it=s.receipts.erase(it);else++it;
    }
    if(witch) {
        const auto calls=s.physicsCalls[identity.witch].fetch_add(1);
        if(calls<3)LOG("Physics export observed %s: context=%p skeleton=%p bones=%u native_result=%d profile_supplied=%d.",identity.name,context,skeleton,count,result,choice.supplied);
    }
    return result;
}
DWORD WINAPI worker(void*) {
    auto& r=Runtime::get();auto& s=state();r.providerFactory=&factory;
    s.dependenciesControlled=true;
    s.allowWrites=GetPrivateProfileIntW(L"Development",L"EnableNpcWrites",0,(r.folder/L"HexAppeal.ini").c_str())!=0;
    s.wardrobeObservation=GetPrivateProfileIntW(L"Development",L"ObserveWardrobe",0,(r.folder/L"HexAppeal.ini").c_str())!=0;
    const bool tracePhysics=s.allowWrites||GetPrivateProfileIntW(L"Development",L"TracePhysicsRoute",0,(r.folder/L"HexAppeal.ini").c_str())!=0;
    if(!r.profiles.empty()&&GetPrivateProfileIntW(L"Development",L"DumpObservedPoses",0,(r.folder/L"HexAppeal.ini").c_str())) {
        s.observations=r.profiles.parent_path()/L"observations"/std::to_wstring(GetCurrentProcessId());
        std::error_code ec;std::filesystem::create_directories(s.observations,ec);
        if(ec)s.observations.clear();
    }
    const auto game=r.folder.parent_path();
    std::string error;DependencyStatus dependencies;uint64_t scannedRevision=0;DWORD scannedAt=0;bool engineBound=false;std::filesystem::path loadedPreset;std::string loadedPresetHash;
    const auto rescan=[&](bool force=false){
        const auto revision=r.dependencyRevision.load();const auto now=GetTickCount();if(!force&&revision==scannedRevision&&now-scannedAt<15000)return;
        scannedAt=now;scannedRevision=revision;
        try {dependencies=inspectDependencies(game,r.configuredDependencyPaths());}
        catch(...){dependencies={};dependencies.scanned=true;dependencies.bodyDetail="Dependency detection failed. Check the paths in Settings.";}
        bool hexReady=false;
        {std::lock_guard<std::mutex> engineLock(s.engineMutex);
            if(engineBound&&dependencies.hex.valid){
                const auto digest=sha256(dependencies.hex.path);
                if(loadedPreset==dependencies.hex.path&&loadedPresetHash==digest)hexReady=true;
                else if(s.engine.readHex(dependencies.hex.path,error)){loadedPreset=dependencies.hex.path;loadedPresetHash=digest;hexReady=true;LOG("Hex preset uses external installed dependency: %ls.",loadedPreset.c_str());}
            }
            if(!hexReady){s.engine.forgetHex();loadedPreset.clear();loadedPresetHash.clear();}
            s.engine.setWeightAvailable(engineBound&&dependencies.weight.valid);
        }
        r.hexAvailable=hexReady;r.weightAvailable=engineBound&&dependencies.weight.valid;
        const bool dmmLoaded=GetModuleHandleW(L"DmmRuntime.asi")!=nullptr;
        r.shapingAvailable=s.hooked&&dependencies.bodyVerified&&dmmLoaded&&!s.failed;
        if(dependencies.bodyVerified&&!dmmLoaded)dependencies.bodyDetail="Body files found, but DMM's runtime is not loaded. Restart after installing/enabling it.";
        else if(dependencies.bodyVerified&&!s.hooked)dependencies.bodyDetail=error.empty()?"Waiting for the separately installed Instant Surgery plugin.":error;
        r.publishDependencies(dependencies);
        refreshWardrobeResources(dependencies);
        if(!s.hooked)setStatus(!dependencies.bodyVerified?dependencies.bodyDetail:!dmmLoaded?dependencies.bodyDetail:"Waiting for the separately installed Instant Surgery plugin.");
    };
    for(int attempt=0;;++attempt) {
        rescan();
        const auto module=GetModuleHandleW(L"InstantSurgery.asi");
        if(module&&dependencies.bodyVerified&&GetModuleHandleW(L"DmmRuntime.asi")&&(engineBound||s.engine.bind(module,error))) {
            engineBound=true;rescan(true);
            HMODULE retained=nullptr;
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(module),&retained))return 0;
            s.module=retained;
            const auto target=reinterpret_cast<unsigned char*>(module)+0x18D70;
            const auto resolver=reinterpret_cast<unsigned char*>(module)+0x17FB0;
            const auto physics=tracePhysics?reinterpret_cast<void*>(GetProcAddress(module,"InstantSurgeryDeformPoseV1")):nullptr;
            if(tracePhysics&&physics!=reinterpret_cast<unsigned char*>(module)+0x22B00){setStatus("The inspected physics integration export has changed.");return 0;}
            const auto bodyPhysics=GetModuleHandleW(L"BodyPhysics.asi");
            if(tracePhysics&&!bodyPhysics&&std::filesystem::exists(r.folder/L"BodyPhysics.asi")){Sleep(250);continue;}
            if(tracePhysics&&bodyPhysics) {
                wchar_t path[32768]{};GetModuleFileNameW(bodyPhysics,path,32768);
                const auto query=GetProcAddress(bodyPhysics,"TwoReasonsEditorPoseAppliedV1");
                if(!query) {
                    setStatus("Body Physics is present, but its pose acknowledgement interface could not be found.");return 0;
                }
                HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(query),&pinned))return 0;
                s.physicsApplied=reinterpret_cast<int(*)(void*)>(query);s.physicsRoute=true;
            }
            const bool updateCreated=MH_CreateHook(target,reinterpret_cast<void*>(&hookedUpdate),reinterpret_cast<void**>(&originalUpdate))==MH_OK;
            const bool physicsCreated=!tracePhysics||MH_CreateHook(physics,reinterpret_cast<void*>(&observedPhysicsPose),reinterpret_cast<void**>(&originalPhysicsPose))==MH_OK;
            const bool resolverCreated=!tracePhysics||MH_CreateHook(resolver,reinterpret_cast<void*>(&hookedResolveProfile),reinterpret_cast<void**>(&originalResolveProfile))==MH_OK;
            const bool queued=updateCreated&&physicsCreated&&resolverCreated&&MH_QueueEnableHook(target)==MH_OK&&(!tracePhysics||(MH_QueueEnableHook(physics)==MH_OK&&MH_QueueEnableHook(resolver)==MH_OK));
            if(!queued||MH_ApplyQueued()!=MH_OK) {
                if(updateCreated){MH_DisableHook(target);MH_RemoveHook(target);}
                if(tracePhysics&&physicsCreated){MH_DisableHook(physics);MH_RemoveHook(physics);}
                if(tracePhysics&&resolverCreated){MH_DisableHook(resolver);MH_RemoveHook(resolver);}
                setStatus("The version-checked NPC observation hooks could not be installed.");return 0;
            }
            std::string visibilityError;
            if(!initializeUnderwear(module,visibilityError))LOG_ERR("Underwear controls unavailable: %s",visibilityError.c_str());
            if(s.wardrobeObservation){std::string error;if(!initializeWardrobeObservation(error)){reportWardrobeUnavailable(error);LOG_ERR("Wardrobe observation unavailable: %s",error.c_str());}}
            s.hooked=true;setStatus("NPC route ready. Saved shapes restore when a selected witch becomes visible.");
            rescan(true);
            LOG("Instant Surgery NPC adapter installed. Body/outfit writes require a validated transaction and witch identity; version verification is reported separately.");
            if(tracePhysics)LOG("Native profile resolver and physics route installed; selected profile supplied only after an explicit Apply. Physics upload receipts enabled=%d.",s.physicsRoute);
            break;
        }
        if(!error.empty())setStatus(error);
        Sleep(500);
    }
    uint64_t persisted=0;
    while(true) {
        rescan();
        {
            std::lock_guard<std::mutex> application(s.applicationMutex);
            std::vector<uint64_t> expired;
            {std::lock_guard<std::mutex> lock(s.mutex);for(const auto& item:s.transactions)if(item.second.state==1&&GetTickCount()-item.second.queuedAt>15000)expired.push_back(item.first);}
            for(const auto version:expired)reject(version,"The selected witches did not finish updating within 15 seconds. The previous selection was restored; keep them visible and retry.");
        }
        const auto version=s.persistVersion.load();
        if(version&&version!=persisted) {
            const auto snapshot=std::atomic_load(&s.active);Model model;model.saved=snapshot->profiles;
            std::string error;
            if(model.save(r.profiles.parent_path()/L"applied-profiles.ini",error)){persisted=version;LOG("Persisted applied profile generation %llu.",version);}
        }
        Sleep(500);
    }
}
}
void startNpcRuntime(){const auto& r=Runtime::get();if(!GetPrivateProfileIntW(L"Development",L"ObserveNpcUploads",0,(r.folder/L"HexAppeal.ini").c_str()))return;const auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
#ifdef HEX_NPC_RUNTIME_TESTS
namespace {
void* fixtureContextActor(void* context){return context;}
bool fixtureNoProfile(void*,SurgeryProfile*){return false;}
bool fixturePhysicsApplied=false;
int fixtureQueryApplied(void*){return fixturePhysicsApplied?1:0;}
}
bool runPhysicsTransactionTests(HMODULE surgery,void* upload,void* model,void* actor,void* skeleton,std::string& error) {
    auto& s=state();s.module=surgery;s.hooked=true;s.physicsRoute=true;s.physicsApplied=&fixtureQueryApplied;
    const auto base=reinterpret_cast<unsigned char*>(surgery);
    const auto resolver=base+0x17FB0,contextResolver=base+0x13140;
    auto check=[&](bool passed,const char* what){if(!passed)error=what;return passed;};
    const auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
    void* originalContext=nullptr;ResolveProfileFn nativeResolver=nullptr;
    if(!check(MH_CreateHook(contextResolver,reinterpret_cast<void*>(&fixtureContextActor),&originalContext)==MH_OK&&MH_CreateHook(resolver,reinterpret_cast<void*>(&hookedResolveProfile),reinterpret_cast<void**>(&nativeResolver))==MH_OK,"fixture native hooks"))return false;
    originalResolveProfile=&fixtureNoProfile;
    if(MH_EnableHook(contextResolver)!=MH_OK||MH_EnableHook(resolver)!=MH_OK)return false;
    originalPhysicsPose=reinterpret_cast<PhysicsPoseFn>(GetProcAddress(surgery,"InstantSurgeryDeformPoseV1"));
    std::vector<PosePart> parts;if(!capturePoseParts(upload,model,parts,error))return false;
    const auto input=parts[0].animation;auto output=input;const auto nativeLock=reinterpret_cast<PSRWLOCK>(base+0x12A730);
    auto update=[&](){ActorIdentity identity;if(!readWitchIdentity(surgery,actor,identity))return false;AcquireSRWLockExclusive(nativeLock);const auto result=updateActor(upload,model,actor,identity);ReleaseSRWLockExclusive(nativeLock);return result;};
    uint64_t stamp=0;s.allowWrites=false;
    if(!check(observedPhysicsPose(actor,skeleton,input.data(),output.data(),uint32_t(input.size()),&stamp)==0&&output==input,"observation leaves native no-profile export unchanged"))return false;
    s.allowWrites=true;s.lastSeen[0]=GetTickCount();uint64_t previousStamp=0;
    for(uint32_t top=0;top<3;++top)for(uint32_t bottom=0;bottom<3;++bottom) {
        uint64_t id=0;HexAppealProfileV1 profile{sizeof(HexAppealProfileV1),top,bottom,1,1,1};char message[256]{};
        if(!check(begin(1,&id)&&prepare(id,&witches[0].target,&profile)&&commit(id),"queue pre-physics combination"))return false;
        fixturePhysicsApplied=true;
        if(!check(update()&&poll(id,message,sizeof(message))==0,"stale native receipt cannot acknowledge a new selection"))return false;
        output=input;stamp=0;
        if(!check(observedPhysicsPose(actor,skeleton,input.data(),output.data(),uint32_t(input.size()),&stamp)==1&&stamp&&stamp!=previousStamp,"native export calculates selected profile and change stamp"))return false;
        previousStamp=stamp;
        const auto snapshot=std::atomic_load(&s.active);std::vector<PoseMatrix> expected;
        if(!s.engine.deform(parts[0].bones,input,snapshot->shapes[0],expected,error))return false;
        if(!check(output==expected,"native pre-physics output matches selected independent-region deformation"))return false;
        if(!writePoseBytes(parts[0].destination,output))return false;
        fixturePhysicsApplied=false;
        if(!check(update()&&poll(id,message,sizeof(message))==0,"missing Body Physics upload cannot acknowledge the selection"))return false;
        fixturePhysicsApplied=true;
        if(!check(update()&&poll(id,message,sizeof(message))==1,"matching body/outfit upload acknowledges the selected native profile"))return false;
        const auto first=output;uint64_t repeatedStamp=0;output=input;
        if(!check(observedPhysicsPose(actor,skeleton,input.data(),output.data(),uint32_t(input.size()),&repeatedStamp)==1&&output==first&&repeatedStamp==stamp,"repeated original animation frames do not compound and keep a stable native stamp"))return false;
    }
    s.engine.setWeightAvailable(true);
    for(uint32_t top:{1u,2u}) {
        uint64_t id=0;const uint32_t bottom=top==1?2u:1u;
        HexAppealProfileV1 profile{sizeof(HexAppealProfileV1),top,bottom,1,1,1,top==1?.3f:-.3f,top==1?1.f:-1.f};char message[256]{};
        if(!check(begin(1,&id)&&prepare(id,&witches[0].target,&profile)&&commit(id),"queue linked stature and optional weight"))return false;
        output=input;stamp=0;
        if(!check(observedPhysicsPose(actor,skeleton,input.data(),output.data(),uint32_t(input.size()),&stamp)==1&&stamp,"linked stature export"))return false;
        const auto snapshot=std::atomic_load(&s.active);std::vector<PoseMatrix> native,expected;
        if(!s.engine.deform(parts[0].bones,input,snapshot->shapes[0],native,error)||!applyStature(snapshot->profiles[0],native,expected,error))return false;
        if(!check(output==expected&&output!=native,"height and regional weight reach the actual physics export before its upload"))return false;
        if(!writePoseBytes(parts[0].destination,output))return false;
        fixturePhysicsApplied=true;if(!check(update()&&poll(id,message,sizeof(message))==1,"linked body/outfit upload acknowledgement"))return false;
        const auto first=output;const auto firstStamp=stamp;output=input;stamp=0;
        if(!check(observedPhysicsPose(actor,skeleton,input.data(),output.data(),uint32_t(input.size()),&stamp)==1&&output==first&&stamp==firstStamp,"linked proportions do not accumulate between frames"))return false;
    }
    s.engine.setWeightAvailable(false);
    output=input;stamp=0;
    if(!check(observedPhysicsPose(nullptr,skeleton,input.data(),output.data(),uint32_t(input.size()),&stamp)==0&&output==input,"unresolved actors retain native pass-through"))return false;
    s.allowWrites=false;output=input;stamp=0;
    if(!check(observedPhysicsPose(actor,skeleton,input.data(),output.data(),uint32_t(input.size()),&stamp)==0&&output==input,"observation gate blocks published pre-physics selections"))return false;
    MH_DisableHook(resolver);MH_DisableHook(contextResolver);MH_RemoveHook(resolver);MH_RemoveHook(contextResolver);
    originalResolveProfile=nullptr;originalPhysicsPose=nullptr;
    for(const auto& part:parts)if(!writePoseBytes(part.destination,part.animation))return false;
    s.physicsRoute=false;s.physicsApplied=nullptr;s.receipts.clear();s.receiptChecks.fill(0);
    s.transactions.clear();s.actors.clear();s.sequence=0;s.active=std::make_shared<Snapshot>();s.persistVersion=0;s.hooked=false;s.seen=0;s.lastSeen[0]=0;
    return true;
}
bool runNpcTransactionTests(HMODULE surgery,const std::filesystem::path& preset,void* upload,void* model,void* actor,std::string& error) {
    auto& s=state();std::vector<PosePart> initial;
    if(!capturePoseParts(upload,model,initial,error)||!s.engine.bind(surgery,error)||!s.engine.readHex(preset,error))return false;
    auto check=[&](bool passed,const char* what){if(!passed)error=what;return passed;};
    auto unchanged=[&](){for(const auto& p:initial)if(!poseBytesEqual(p.destination,p.source))return false;return true;};
    auto apply=[&](uint32_t top,uint32_t bottom,uint64_t& id){
        HexAppealProfileV1 p{sizeof(HexAppealProfileV1),top,bottom,1,1,1};
        return begin(1,&id)&&prepare(id,&witches[0].target,&p)&&commit(id);
    };
    s.hooked=true;s.allowWrites=false;s.lastSeen[0]=GetTickCount();uint64_t id=0;
    if(!check(!begin(1,&id)&&unchanged(),"observation mode cannot begin writes"))return false;
    s.allowWrites=true;
    if(!check(apply(1,2,id),"queue native mixed-region transaction"))return false;
    char message[256]{};
    if(!check(poll(id,message,sizeof(message))==0,"queued transaction remains pending before upload"))return false;
    ActorIdentity lyselia;lyselia.witch=0;lyselia.recordKey=1002941;strcpy_s(lyselia.name,"NHW_Unique_Modesty_Witch_Store");
    if(!check(updateActor(upload,model,actor,lyselia)&&poll(id,message,sizeof(message))==1&&!unchanged(),"native transaction updates and acknowledges captured buffers"))return false;
    std::vector<PosePart> first;
    if(!capturePoseParts(upload,model,first,error))return false;
    if(!check(updateActor(upload,model,actor,lyselia),"repeat shaped upload"))return false;
    for(const auto& p:first)if(!check(poseBytesEqual(p.destination,p.source),"repeated upload must not compound deformation"))return false;
    if(!check(apply(2,1,id)&&updateActor(upload,model,actor,lyselia)&&poll(id,message,sizeof(message))==1,"live combination reselection"))return false;
    std::vector<PosePart> second;
    if(!capturePoseParts(upload,model,second,error))return false;
    if(!check(second[0].source!=first[0].source,"reselection produces a different shape"))return false;
    second[0].source[0][0]+=.001f;
    if(!writePoseBytes(second[0].destination,second[0].source))return false;
    if(!check(apply(1,1,id)&&updateActor(upload,model,actor,lyselia)&&poll(id,message,sizeof(message))<0,"conflicting modifier rejects pending application"))return false;
    if(!check(poseBytesEqual(second[0].destination,second[0].source),"rejected transaction cannot overwrite a conflicting modifier"))return false;
    s.allowWrites=false;
    if(!check(!updateActor(upload,model,actor,lyselia)&&poseBytesEqual(second[0].destination,second[0].source),"observation gate also blocks an already published selection"))return false;
    for(const auto& p:initial)if(!writePoseBytes(p.destination,p.source))return false;
    s.transactions.clear();s.actors.clear();s.sequence=0;s.active=std::make_shared<Snapshot>();s.persistVersion=0;s.hooked=false;s.seen=0;s.lastSeen[0]=0;
    return true;
}
#endif
}
