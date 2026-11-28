#include "npc_runtime.h"
#include "game_layout.h"
#include "game_signatures.h"
#include "runtime.h"
#include "shaping.h"
#include "pose_access.h"
#include "pose_observation.h"
#include "mounted_dependencies.h"
#include "underwear.h"
#include "wardrobe.h"
#include "wardrobe_native.h"
#include "wardrobe_snapshot.h"
#include "character_addons.h"
#include "record_flags.h"
#include "native_interface.h"
#include "appearance_hooks.h"
#include "core/log.h"
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <cstdio>

namespace hex {
namespace {
constexpr uint64_t addonPoseVersionBit=1ull<<63;
struct Snapshot {
    uint64_t version=0;
    std::array<bool,6> enabled{};
    std::array<Profile,6> profiles{};
    std::array<SurgeryProfile,6> shapes{};
};
struct AddonPose {
    uint64_t version=0,profileRevision=0,dependencyVersion=0;
    Profile profile;
    SurgeryProfile shape{};
    bool ready=false;
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
    uint64_t retryPoseAt=0;
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
    std::atomic<bool> sawCompatibleUpload{false};
    std::atomic<bool> reportedUploadLayout{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> allowWrites{false};
    RecordFlags<characterCapacity> seen;
    std::array<std::atomic<DWORD>,characterCapacity> lastSeen{};
    std::mutex mutex;
    std::mutex applicationMutex;
    std::mutex engineMutex;
    std::atomic<bool> dependenciesControlled{false};
    std::string status="Waiting for Instant Surgery.";
    std::map<uint64_t,Transaction> transactions;
    uint64_t sequence=0;
    std::shared_ptr<const Snapshot> active=std::make_shared<Snapshot>();
    std::array<std::shared_ptr<const AddonPose>,maximumAddonCharacters> addonPoses;
    std::array<std::atomic<uint64_t>,maximumAddonCharacters> rejectedAddonPose{};
    std::array<std::atomic<uint64_t>,maximumAddonCharacters> addonApplied{};
    uint64_t addonPoseSequence=0,addonDependencyVersion=0;
    std::map<uintptr_t,ActorCache> actors;
    std::atomic<uint64_t> persistVersion{0};
    std::filesystem::path observations;
    std::array<uint32_t,characterCapacity> capturedFrames{};
    std::array<DWORD,characterCapacity> capturedAt{};
    std::array<std::atomic<uint64_t>,characterCapacity> physicsCalls{};
    std::atomic<bool> physicsRoute{false};
    int (*physicsApplied)(void*)=nullptr;
    std::map<std::pair<uintptr_t,uintptr_t>,PhysicsReceipt> receipts;
    DWORD receiptsPrunedAt=0;
    std::array<uint32_t,characterCapacity> receiptChecks{};
};
RuntimeState& state(){static RuntimeState s;return s;}
bool dependenciesAllow(int witch,const Profile& profile){
    if(!state().dependenciesControlled)return true;const auto& r=Runtime::get();
    return r.shapingAvailable&&(witch>=firstAddonCharacter?characterAddonEnabled(witch):
        !(r.unavailableBodyWitches.load()&(1u<<witch)))&&
        (!(profile.top==Preset::Hex&&profile.topBlend>0)&&!(profile.bottom==Preset::Hex&&profile.bottomBlend>0)||r.hexAvailable)&&
        (profile.weight==0||r.weightAvailable);
}
const CharacterAddon* checkedAddon(const ActorIdentity& identity){
    const auto* addon=characterAddon(identity.witch);
    return addon&&matchesWardrobeAppearance(identity.actor,reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),
        addon->appearance.c_str())?addon:nullptr;
}
std::shared_ptr<const AddonPose> currentAddonPose(int target){
    return std::atomic_load(&state().addonPoses[target-firstAddonCharacter]);
}
bool addonPoseReady(int target,const std::shared_ptr<const AddonPose>& pose){
    return pose&&pose->ready&&dependenciesAllow(target,pose->profile)&&
        state().rejectedAddonPose[target-firstAddonCharacter].load()!=pose->version;
}
void prepareAddonPoses(){
    auto& s=state();
    if(!s.allowWrites||s.failed)return;
    for(int target:enabledAddonTargets()){
        const auto current=addonBodyProfile(target);
        const auto previous=currentAddonPose(target);
        if(!current.revision||(previous&&previous->profileRevision==current.revision&&
           previous->dependencyVersion==s.addonDependencyVersion))continue;
        auto pose=std::make_shared<AddonPose>();
        pose->version=addonPoseVersionBit|++s.addonPoseSequence;
        pose->profileRevision=current.revision;
        pose->dependencyVersion=s.addonDependencyVersion;
        pose->profile=current.profile;
        std::string detail;
        if(current.profile==Profile{})detail="The character uses its mounted body midpoint.";
        else if(!dependenciesAllow(target,current.profile))detail="Waiting for the selected shape dependencies.";
        else {
            ResolvedShape resolved;
            std::lock_guard<std::mutex> lock(s.engineMutex);
            if(s.engine.compose(current.profile,resolved,detail)){
                pose->shape=resolved.native;
                pose->ready=true;
                detail="Shape saved. Bring the character into view to apply it.";
            }
        }
        std::atomic_store(&s.addonPoses[target-firstAddonCharacter],std::shared_ptr<const AddonPose>(pose));
        reportAddonShape(target,current.revision,std::move(detail));
    }
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
    else if(!(visibleMask()&~Runtime::get().unavailableBodyWitches.load()))message="Keep a supported witch visible so her body and outfit buffers can be checked.";
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
    {std::lock_guard<std::mutex> engineLock(s.engineMutex);if(!dependenciesAllow(witch,choice)||!s.engine.compose(choice,shape,error)){setStatus(error.empty()?"A required dependency is unavailable.":error);return 0;}}
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
    if(!transaction.observed){
        transaction.reason="Saved shapes will apply when the selected witches are visible.";
        transaction.state=-1;
        return 0;
    }
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
    "Instant Surgery - Hex Appeal NPC integration",ready,begin,prepare,commit,abort,poll};
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
    if(stop){s.failed=true;Runtime::get().shapingAvailable=false;Runtime::get().reportCompatibility(Feature::Shaping,CompatibilityState::Blocked,message);}
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
    const int target=identity.witch;
    const bool isAddon=target>=firstAddonCharacter;
    const auto addonPose=isAddon?currentAddonPose(target):nullptr;
    if(isAddon&&(!checkedAddon(identity)||!addonPose))return false;
    const auto& profile=isAddon?addonPose->profile:snapshot->profiles[target];
    const auto& shape=isAddon?addonPose->shape:snapshot->shapes[target];
    const auto version=isAddon?addonPose->version:snapshot->version;
    const auto enabled=[&]{
        return s.allowWrites&&(isAddon?addonPoseReady(target,addonPose):
            snapshot->enabled[target]&&dependenciesAllow(target,profile));
    };
    const auto fail=[&](const std::string& detail,bool stop=false){
        if(!isAddon||stop)reject(version,detail,stop);
        if(isAddon){
            s.rejectedAddonPose[target-firstAddonCharacter]=version;
            reportAddonShape(target,addonPose->profileRevision,detail);
            LOG_ERR("Character add-on shaping rejected for %s: %s",identity.name,detail.c_str());
        }
    };
    const auto appliedShape=[&](size_t count){
        if(!isAddon)acknowledge(version,target,count);
        else if(s.addonApplied[target-firstAddonCharacter].exchange(version)!=version)
            reportAddonShape(target,addonPose->profileRevision,"Shape active.");
    };
    if(!capturePoseParts(upload,model,parts,error)) {
        const auto message=std::string(identity.name)+": "+error;
        if(gameLayout().expandedRecords&&!s.sawCompatibleUpload&&!s.reportedUploadLayout.exchange(true))
            LOG_ERR("Could not validate the 2.06 character upload: %s",message.c_str());
        if(isAddon&&enabled())fail(message);
        else if(!isAddon)setStatus(message);
        return false;
    }
    s.sawCompatibleUpload=true;
    s.lastSeen[identity.witch]=GetTickCount();
    const bool firstObservation=!s.seen.exchange(size_t(identity.witch),true);
    const auto countChangedParts=[&]{
        return std::count_if(parts.begin(),parts.end(),[](const auto& part){return part.source!=part.animation;});
    };
    if(firstObservation) {
        LOG("Observed %s key=%u actor=%p model=%p; %zu validated body/outfit pose buffers; %zu differ from animation source.",identity.name,identity.recordKey,actor,model,parts.size(),size_t(countChangedParts()));
        for(size_t i=0;i<parts.size();++i)for(const auto renderPart:parts[i].renderParts)
            LOG("Observed render part witch=%d model=%p part=%p skeleton=%p bones=%zu.",identity.witch,model,reinterpret_cast<void*>(renderPart),reinterpret_cast<void*>(parts[i].skeleton),parts[i].bones.size());
    }
    if(!s.observations.empty()&&s.capturedFrames[identity.witch]<4&&GetTickCount()-s.capturedAt[identity.witch]>250) {
        const auto frame=s.capturedFrames[identity.witch]++;s.capturedAt[identity.witch]=GetTickCount();
        if(savePoseObservation(s.observations,identity,parts,frame,error))LOG("Captured read-only pose observation for %s, frame %u, %zu shared buffers.",identity.name,frame,parts.size());
        else LOG_ERR("Pose observation failed: %s",error.c_str());
    }
    if(s.physicsRoute) {
        application.unlock();
        const auto nativeLock=reinterpret_cast<PSRWLOCK>(reinterpret_cast<unsigned char*>(s.module)+0x12A730);
        ReleaseSRWLockExclusive(nativeLock);
        bool applied=true;
        size_t renderPartCount=0;
        for(const auto& part:parts)for(const auto renderPart:part.renderParts){
            applied=physicsAppliedGuard(s.physicsApplied,renderPart)&&applied;
            ++renderPartCount;
        }
        applied=renderPartCount&&applied;
        AcquireSRWLockExclusive(nativeLock);
        application.lock();
        if(isAddon?currentAddonPose(target)!=addonPose:std::atomic_load(&s.active)!=snapshot)return true;
        bool sourcesMatch=true,versionsMatch=true;
        for(const auto& part:parts) {
            const auto receipt=s.receipts.find({reinterpret_cast<uintptr_t>(actor),part.skeleton});
            const bool matching=receipt!=s.receipts.end()&&GetTickCount()-receipt->second.tick<500&&receipt->second.animation==part.animation&&receipt->second.bones.size()==part.bones.size()&&memcmp(receipt->second.bones.data(),part.bones.data(),part.bones.size()*sizeof(ShapeBone))==0;
            sourcesMatch=matching&&sourcesMatch;
            versionsMatch=matching&&receipt->second.supplied&&receipt->second.version==version&&versionsMatch;
        }
        if(s.receiptChecks[identity.witch]<4) {
            ++s.receiptChecks[identity.witch];
            LOG("Physics upload check %s: original_sources_match=%d editor_pose_applied=%d selected_version_received=%d buffers=%zu render_parts=%zu.",identity.name,sourcesMatch,applied,versionsMatch,parts.size(),renderPartCount);
        }
        if(!enabled()) {
            if(!isAddon)setStatus(std::string(identity.name)+": physics route observed; original pose match "+(sourcesMatch?"confirmed.":"pending."));
            return false;
        }
        if(!sourcesMatch||!versionsMatch||!applied) {
            if(!isAddon)setStatus(std::string(identity.name)+": waiting for the selected body and outfit physics uploads.");
            return !isAddon||versionsMatch;
        }
        for(const auto& part:parts)if(!poseBytesEqual(part.destination,part.source))return true;
        appliedShape(parts.size());return true;
    }
    auto& cache=s.actors[reinterpret_cast<uintptr_t>(actor)];
    if(cache.witch!=identity.witch){cache={};cache.witch=identity.witch;}
    cache.last=GetTickCount();
    if(GetTickCount64()<cache.retryPoseAt)return !isAddon;
    if(!enabled()) {
        if(s.allowWrites)for(auto& part:parts) {
            const auto previous=cache.parts.find(reinterpret_cast<uintptr_t>(part.destination));
            if(previous!=cache.parts.end()&&previous->second.written==part.source&&previous->second.base==part.animation)
                if(!writePoseBytes(part.destination,part.animation)){
                    cache.retryPoseAt=GetTickCount64()+500;
                    setStatus("Waiting to restore the previous pose.");
                    return true;
                }
        }
        if(!isAddon)setStatus(std::string(identity.name)+": "+std::to_string(parts.size())+" pose buffers checked; "+std::to_string(countChangedParts())+" differ from the original animation. No shape selected for application.");
        return false;
    }
    std::vector<std::vector<PoseMatrix>> outputs;
    outputs.reserve(parts.size());
    for(const auto& part:parts) {
        const auto previous=cache.parts.find(reinterpret_cast<uintptr_t>(part.destination));
        const bool ours=previous!=cache.parts.end()&&previous->second.bones.size()==part.bones.size()&&memcmp(previous->second.bones.data(),part.bones.data(),part.bones.size()*sizeof(ShapeBone))==0&&previous->second.written==part.source&&previous->second.base==part.animation;
        if(part.source!=part.animation&&!ours) {
            setStatus("Waiting for stable pose data. Another mod or the game is still updating it.");
            return !isAddon;
        }
        auto& output=outputs.emplace_back();
        if(!s.engine.deform(part.bones,part.animation,shape,output,error,true)){fail(std::string(identity.name)+": "+error);return !isAddon;}
        if(!applyStature(profile,output,output,error)){fail(error);return !isAddon;}
    }
    for(size_t i=0;i<parts.size();++i)if(!poseBytesEqual(parts[i].destination,parts[i].source)) {
        setStatus("Waiting for stable pose data. The buffers changed during preparation.");
        return !isAddon;
    }
    for(size_t i=0;i<parts.size();++i)if(!writePoseBytes(parts[i].destination,outputs[i])) {
        bool restored=true;for(size_t j=0;j<=i;++j)restored=writePoseBytes(parts[j].destination,parts[j].source)&&restored;
        LOG_ERR("NPC body/outfit write failed; restored=%d",restored);
        if(restored){cache.retryPoseAt=GetTickCount64()+500;setStatus("Pose restored. Retrying the shape change.");}
        else fail("Pose restoration failed; shaping stopped. Restart the game.",true);
        return true;
    }
    for(size_t i=0;i<parts.size();++i){
        auto& part=parts[i];
        cache.parts[reinterpret_cast<uintptr_t>(part.destination)]={std::move(part.bones),std::move(part.animation),std::move(outputs[i])};
    }
    appliedShape(parts.size());
    for(auto it=s.actors.begin();it!=s.actors.end();)if(GetTickCount()-it->second.last>30000)it=s.actors.erase(it);else++it;
    return true;
}
void hookedUpdate(void* upload,void* model,void* actor) {
    auto& s=state();ActorIdentity identity;
    const bool observed=readWitchIdentity(s.module,actor,identity,addonTargetForRecord);
    const bool handled=!s.failed&&observed&&updateActor(upload,model,actor,identity);
    if(!handled)originalUpdate(upload,model,actor);
}
using GameUpload=void(*)(void*,void*);
GameUpload originalGameUpload=nullptr;
void* readUploadActor(void* model){
    __try{
        const auto at=[](uintptr_t address,uintptr_t offset){return address?*reinterpret_cast<const uintptr_t*>(address+offset):0;};
        const auto address=reinterpret_cast<uintptr_t>(model);
        if(!address||*reinterpret_cast<const uint8_t*>(address+0x2bc)!=0)return nullptr;
        const auto link=at(address,0x18),owner=at(link,8);
        if(!link||!owner||at(owner,8)!=link||at(owner,0x1d8)!=address)return nullptr;
        const auto scene=at(owner,0x40),component=at(scene,0x180);
        if(!scene||!component||at(component,0x10)!=scene)return nullptr;
        const auto attachment=at(component,0xb0);
        if(!attachment||at(attachment,0x10)!=component)return nullptr;
        const auto actor=at(at(attachment,0x28),8);
        const auto game=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        return isWardrobeNpc(actor,game)?reinterpret_cast<void*>(actor):nullptr;
    }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
void updateGameUpload(void* upload,void* model){
    originalGameUpload(upload,model);
    auto& s=state();
    if(!s.hooked||s.failed)return;
    auto* actor=readUploadActor(model);
    ActorIdentity identity;
    if(!actor||!readWardrobeIdentity(actor,identity))return;
    struct SurgeryLock {
        PSRWLOCK lock;
        explicit SurgeryLock(HMODULE module):lock(reinterpret_cast<PSRWLOCK>(reinterpret_cast<uintptr_t>(module)+0x12a730)){
            AcquireSRWLockExclusive(lock);
        }
        ~SurgeryLock(){ReleaseSRWLockExclusive(lock);}
    } guard(s.module);
    try{updateActor(upload,model,actor,identity);}
    catch(...){if(!s.reportedUploadLayout.exchange(true))LOG_ERR("Could not finish a 2.06 character pose update.");}
}
bool installGamePoseUpload(){
    if(!gameLayout().expandedRecords)return false;
    // Found by signature on any 2.06 executable; on the checked Steam build it must be 0x324fd80.
    const auto* found=signatureExpanded();
    if(!found||!found->poseUpload){
        LOG_ERR("The 2.06 animation upload was not found on this build. Shape waits for a compatible upload.");
        return false;
    }
    const auto game=GetModuleHandleW(nullptr);
    auto* entry=reinterpret_cast<unsigned char*>(game)+found->poseUpload;
    const unsigned char head[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
    const unsigned char tail[]{0x48,0x8b,0x9a,0x90,0x01,0x00,0x00,0x4c,0x8d,0x7a,0x40,0x8b,0x82,0x98,0x01,0x00,0x00};
    if(!executablePluginFunction(game,entry)||appearance::checkedGameEntry(entry,head,28,tail)!=appearance::GameEntry::Stock)return false;
    if(MH_CreateHook(entry,reinterpret_cast<void*>(&updateGameUpload),reinterpret_cast<void**>(&originalGameUpload))!=MH_OK)return false;
    if(MH_EnableHook(entry)!=MH_OK){MH_RemoveHook(entry);return false;}
    LOG("Hex Appeal attached directly to the checked 2.06 animation upload. Instant Surgery supplies the shaping engine.");
    return true;
}
int observedPhysicsPose(void* context,void* skeleton,const PoseMatrix* source,PoseMatrix* destination,uint32_t count,uint64_t* stamp) {
    auto& s=state();ActorIdentity identity;
    bool observed=readContextWitchIdentity(s.module,context,identity,addonTargetForRecord);
    const bool isAddon=observed&&identity.witch>=firstAddonCharacter;
    const auto addonPose=isAddon?currentAddonPose(identity.witch):nullptr;
    if(isAddon)observed=addonPoseReady(identity.witch,addonPose)&&checkedAddon(identity);
    const auto snapshot=std::atomic_load(&s.active);
    PhysicsReceipt receipt;receipt.version=isAddon&&addonPose?addonPose->version:snapshot->version;
    const bool captured=observed&&capturePhysicsSource(skeleton,source,count,receipt.bones,receipt.animation);
    ProfileOverride choice;
    if(captured&&s.physicsRoute&&s.allowWrites&&!s.failed&&(isAddon||
       (snapshot->enabled[identity.witch]&&dependenciesAllow(identity.witch,snapshot->profiles[identity.witch])))) {
        choice.actor=identity.actor;choice.shape=isAddon?&addonPose->shape:&snapshot->shapes[identity.witch];
    }
    auto* previous=currentProfileOverride;currentProfileOverride=&choice;
    auto result=originalPhysicsPose(context,skeleton,source,destination,count,stamp);
    currentProfileOverride=previous;
    const auto& profile=isAddon&&addonPose?addonPose->profile:snapshot->profiles[observed?identity.witch:0];
    if(choice.supplied&&result==1&&linkedStature(profile)!=0.f) {
        std::vector<PoseMatrix> nativeOutput;std::string error;uint64_t nativeStamp=0;
        if(!readPoseBytes(destination,count,nativeOutput)||!applyStature(profile,nativeOutput,nativeOutput,error)||!readStampGuard(stamp,&nativeStamp)||!writePoseBytes(destination,nativeOutput)||!writeStampGuard(stamp,statureStamp(nativeStamp,profile))) {
            const bool restored=writePoseBytes(destination,receipt.animation);writeStampGuard(stamp,0);choice.supplied=false;result=0;
            const auto detail=error.empty()?"Stature pose could not be prepared":error;
            if(isAddon){
                s.rejectedAddonPose[identity.witch-firstAddonCharacter]=addonPose->version;
                reportAddonShape(identity.witch,addonPose->profileRevision,detail);
            }
            if(!isAddon||!restored)reject(receipt.version,detail,!restored);
        }
    }
    if(captured) {
        receipt.tick=GetTickCount();receipt.supplied=choice.supplied&&result==1&&readStampGuard(stamp,&receipt.stamp);
        std::lock_guard<std::mutex> application(s.applicationMutex);
        s.receipts[{identity.actor,reinterpret_cast<uintptr_t>(skeleton)}]=std::move(receipt);
        const auto now=GetTickCount();
        if(now-s.receiptsPrunedAt>=1000){
            s.receiptsPrunedAt=now;
            for(auto it=s.receipts.begin();it!=s.receipts.end();)
                if(now-it->second.tick>5000)it=s.receipts.erase(it);
                else ++it;
        }
    }
    if(observed) {
        const auto calls=s.physicsCalls[identity.witch].fetch_add(1);
        if(calls<3)LOG("Physics export observed %s: context=%p skeleton=%p bones=%u native_result=%d profile_supplied=%d.",identity.name,context,skeleton,count,result,choice.supplied);
    }
    return result;
}
DWORD WINAPI worker(void*) {
    waitForGameLayout();  // on a build missing from the table, until signature lookup finishes
    auto& r=Runtime::get();auto& s=state();r.providerFactory=&factory;
    s.dependenciesControlled=true;
    const bool allowNpcWrites=GetPrivateProfileIntW(L"Development",L"EnableNpcWrites",0,(r.folder/L"HexAppeal.ini").c_str())!=0;
    s.allowWrites=allowNpcWrites;
    const bool tracePhysics=GetPrivateProfileIntW(L"Development",L"TracePhysicsRoute",0,(r.folder/L"HexAppeal.ini").c_str())!=0;
    const bool checkPhysics=s.allowWrites||tracePhysics;
    if(!r.profiles.empty()&&GetPrivateProfileIntW(L"Development",L"DumpObservedPoses",0,(r.folder/L"HexAppeal.ini").c_str())) {
        s.observations=r.profiles.parent_path()/L"observations"/std::to_wstring(GetCurrentProcessId());
        std::error_code ec;std::filesystem::create_directories(s.observations,ec);
        if(ec)s.observations.clear();
    }
    const auto game=r.folder.parent_path();
    waitForGameLayout();
    bool gamePoseUpload=false;
    std::string error,hexError,hexWarning;DependencyStatus dependencies;uint64_t scannedRevision=0;DWORD scannedAt=0;bool engineBound=false;std::filesystem::path loadedPreset;std::string loadedPresetHash;
    bool hadCompatibleUpload=false;
    const auto rescan=[&](bool force=false){
        // Without game addresses, or on 2.06 (where Instant Surgery receives no uploads),
        // Shape is Ready only once a compatible upload has been seen.
        const bool layoutReady=loadedGameLayout()!=nullptr;
        const bool compatibleUpload=(layoutReady&&!gameLayout().expandedRecords)||s.sawCompatibleUpload.load();
        const auto revision=r.dependencyRevision.load();const auto now=GetTickCount();if(!force&&revision==scannedRevision&&now-scannedAt<15000&&compatibleUpload==hadCompatibleUpload)return;
        hadCompatibleUpload=compatibleUpload;
        scannedAt=now;scannedRevision=revision;
        try {dependencies=inspectDependencies(game,r.configuredDependencyPaths());}
        catch(...){dependencies={};dependencies.scanned=true;dependencies.bodyDetail="Dependency detection failed. Check the paths in Settings.";}
        const bool oldHex=r.hexAvailable.load(),oldShape=r.shapingAvailable.load();
        bool hexReady=false,presetLoaded=false;hexError.clear();
        {std::lock_guard<std::mutex> engineLock(s.engineMutex);
            if(engineBound&&dependencies.hex.valid){
                const auto digest=sha256(dependencies.hex.path);
                if(loadedPreset==dependencies.hex.path&&loadedPresetHash==digest)hexReady=true;
                else if(s.engine.readHex(dependencies.hex.path,hexError,&hexWarning)){loadedPreset=dependencies.hex.path;loadedPresetHash=digest;hexReady=presetLoaded=true;LOG("Using external Instant Surgery preset: %ls.",loadedPreset.c_str());}
            }
            if(!hexReady){s.engine.forgetHex();loadedPreset.clear();loadedPresetHash.clear();hexWarning.clear();}
        }
        if(!hexWarning.empty())dependencies.hex.detail+=" "+hexWarning;
        r.hexAvailable=hexReady;
        const bool shapingReady=s.hooked&&engineBound&&compatibleUpload&&dependencies.canShapeBody()&&!s.failed;
        if(!shapingReady)r.shapingAvailable=false;
        r.publishDependencies(dependencies);
        r.shapingAvailable=shapingReady;
        r.weightAvailable=engineBound&&shapingReady;
        if(force||presetLoaded||oldHex!=hexReady||oldShape!=shapingReady)++s.addonDependencyVersion;
        const auto shapeIssue=!dependencies.canShapeBody()?dependencies.bodyDetail:!dependencies.instant.valid?dependencies.instant.detail:
            !error.empty()?error:s.hooked&&engineBound&&!compatibleUpload?
            std::string(gamePoseUpload?"Waiting for a compatible game animation upload. Keep a supported character visible.":
                !layoutReady?"Game addresses for this Crimson Desert build were not found. Waiting for a compatible animation upload from Instant Surgery. Keep a supported character visible.":
                "Waiting for a compatible animation upload from Instant Surgery. Keep a supported character visible."):
            std::string("Waiting for the installed Instant Surgery plugin to initialize.");
        if(!s.failed)r.reportCompatibility(Feature::Shaping,r.shapingAvailable?CompatibilityState::Ready:
            !dependencies.canShapeBody()||!dependencies.instant.valid?CompatibilityState::Blocked:CompatibilityState::Waiting,
            r.shapingAvailable?(dependencies.bodyVerified?"Native shaping and active body checks passed.":dependencies.bodyDetail):shapeIssue);
        r.reportCompatibility(Feature::Hex,hexReady?CompatibilityState::Ready:!dependencies.hex.valid||!hexError.empty()||!error.empty()?CompatibilityState::Blocked:CompatibilityState::Waiting,hexReady?(hexWarning.empty()?"The external preset was parsed by the installed shaping adapter.":hexWarning):!hexError.empty()?hexError:!dependencies.hex.valid?dependencies.hex.detail:!error.empty()?error:"Waiting for the native shaping adapter.");
        r.reportCompatibility(Feature::Weight,r.weightAvailable?CompatibilityState::Ready:r.featureCompatibility(Feature::Shaping).state,r.weightAvailable?"Native proportion controls are available; no outfit dependency is required.":shapeIssue);
        if(presetLoaded)r.presetChanged=true;
        if(!s.hooked||!compatibleUpload)setStatus(shapeIssue);
    };
    const auto stopShaping=[&](const std::string& message){error=message;r.shapingAvailable=false;r.weightAvailable=false;r.reportCompatibility(Feature::Shaping,CompatibilityState::Blocked,message);r.reportCompatibility(Feature::Weight,CompatibilityState::Blocked,message);setStatus(message);};
    uint64_t nextPhysicsCheck=0;
    bool reportedPhysicsIssue=false;
    const auto tryOptionalPhysics=[&](){
        const auto now=GetTickCount64();
        if(gamePoseUpload||!engineBound||!checkPhysics||s.physicsRoute||now<nextPhysicsCheck)return;
        nextPhysicsCheck=now+2000;
        const auto bodyPhysics=GetModuleHandleW(L"BodyPhysics.asi");
        if(!bodyPhysics)return;
        std::string issue;
        const auto query=pluginExport(bodyPhysics,"TwoReasonsEditorPoseAppliedV1",issue);
        const auto physics=query?reinterpret_cast<void*>(pluginExport(s.module,"InstantSurgeryDeformPoseV1",issue)):nullptr;
        const auto resolver=reinterpret_cast<unsigned char*>(s.module)+0x17FB0;
        HMODULE retained=nullptr;
        if(physics&&GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(query),&retained)){
            const bool poseCreated=MH_CreateHook(physics,reinterpret_cast<void*>(&observedPhysicsPose),reinterpret_cast<void**>(&originalPhysicsPose))==MH_OK;
            const bool resolverCreated=poseCreated&&MH_CreateHook(resolver,reinterpret_cast<void*>(&hookedResolveProfile),reinterpret_cast<void**>(&originalResolveProfile))==MH_OK;
            if(resolverCreated&&MH_QueueEnableHook(resolver)==MH_OK&&MH_QueueEnableHook(physics)==MH_OK&&MH_ApplyQueued()==MH_OK){
                s.physicsApplied=reinterpret_cast<int(*)(void*)>(query);
                s.physicsRoute=true;
                LOG("Optional Two Reasons Body Physics handoff enabled. Hex Appeal owns both shaping hooks in Instant Surgery.");
                return;
            }
            if(poseCreated)MH_RemoveHook(physics);
            if(resolverCreated)MH_RemoveHook(resolver);
            FreeLibrary(retained);
            issue="The compatibility hooks in Instant Surgery could not start.";
        }
        if(!reportedPhysicsIssue){
            LOG("Optional Two Reasons Body Physics handoff pending. Standard shaping remains available. %s",issue.c_str());
            reportedPhysicsIssue=true;
        }
    };
    const auto startupAt=GetTickCount();
    for(;;) {
        rescan();
        const auto module=GetModuleHandleW(L"InstantSurgery.asi");
        if(module&&probeSurgeryObserver(module,error)) {
            if(!engineBound){
                error.clear();
                engineBound=s.engine.bind(module,error);
                if(!engineBound&&probeSurgeryShaping(module,hexError)&&GetTickCount()-startupAt<15000){
                    Sleep(500);continue;
                }
            }
            HMODULE retained=nullptr;
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(module),&retained)){stopShaping("The loaded Instant Surgery module could not be retained. Restart the game before retrying.");return 0;}
            s.module=retained;
            const auto target=reinterpret_cast<unsigned char*>(module)+0x18D70;
            s.allowWrites=allowNpcWrites&&engineBound;
            gamePoseUpload=engineBound&&installGamePoseUpload();
            const bool prepared=!gamePoseUpload&&MH_CreateHook(target,reinterpret_cast<void*>(&hookedUpdate),reinterpret_cast<void**>(&originalUpdate))==MH_OK;
            const bool installed=gamePoseUpload||(prepared&&MH_EnableHook(target)==MH_OK);
            if(!installed){
                if(prepared)MH_RemoveHook(target);
                s.module=nullptr;
                FreeLibrary(retained);
                error="The NPC pose observer could not start. Hex Appeal will retry.";
                r.reportCompatibility(Feature::Shaping,CompatibilityState::Waiting,error);
                setStatus(error);
                Sleep(1000);
                continue;
            }
            tryOptionalPhysics();
            if(!engineBound)LOG("Native actor observation is ready without the shaping interface: %s",error.c_str());
            s.hooked=true;setStatus(!error.empty()?error:"NPC route ready. Saved shapes restore when a selected witch becomes visible.");
            rescan(true);
            if(!gamePoseUpload)LOG("Instant Surgery NPC adapter installed. Body/outfit writes require a validated transaction and witch identity; version verification is reported separately.");
            break;
        }
        if(!error.empty()){
            r.reportCompatibility(Feature::Shaping,CompatibilityState::Blocked,error);
            setStatus(error);
        }
        else if(!module&&dependencies.instant.valid&&GetTickCount()-startupAt>=15000){const std::string message="Instant Surgery is installed but is not loaded. Enable its ASI and restart Crimson Desert.";r.reportCompatibility(Feature::Shaping,CompatibilityState::Blocked,message);setStatus(message);}
        Sleep(500);
    }
    uint64_t persisted=0,nextBind=0;
    while(true) {
        const auto now=GetTickCount64();
        if(!engineBound&&now>=nextBind){
            nextBind=now+2000;
            {
                std::lock_guard<std::mutex> engineLock(s.engineMutex);
                engineBound=s.engine.bind(s.module,error);
            }
            if(engineBound){
                s.allowWrites=allowNpcWrites;
                LOG("Instant Surgery's shaping interface became ready after observer startup.");
                rescan(true);
            }
        }
        rescan();
        tryOptionalPhysics();
        prepareAddonPoses();
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
void startNpcRuntime(){const auto& r=Runtime::get();if(!GetPrivateProfileIntW(L"Development",L"ObserveNpcUploads",0,(r.folder/L"HexAppeal.ini").c_str()))return;const auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);else LOG_ERR("Windows could not start the shaping worker.");}
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
    for(uint32_t top:{1u,2u}) {
        uint64_t id=0;const uint32_t bottom=top==1?2u:1u;
        HexAppealProfileV1 profile{sizeof(HexAppealProfileV1),top,bottom,1,1,1,top==1?.3f:-.3f,top==1?1.f:-1.f};char message[256]{};
        if(!check(begin(1,&id)&&prepare(id,&witches[0].target,&profile)&&commit(id),"queue independent height and weight"))return false;
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
    output=input;stamp=0;
    if(!check(observedPhysicsPose(nullptr,skeleton,input.data(),output.data(),uint32_t(input.size()),&stamp)==0&&output==input,"unresolved actors retain native pass-through"))return false;
    s.allowWrites=false;output=input;stamp=0;
    if(!check(observedPhysicsPose(actor,skeleton,input.data(),output.data(),uint32_t(input.size()),&stamp)==0&&output==input,"observation gate blocks published pre-physics selections"))return false;
    MH_DisableHook(resolver);MH_DisableHook(contextResolver);MH_RemoveHook(resolver);MH_RemoveHook(contextResolver);
    originalResolveProfile=nullptr;originalPhysicsPose=nullptr;
    for(const auto& part:parts)if(!writePoseBytes(part.destination,part.animation))return false;
    s.physicsRoute=false;s.physicsApplied=nullptr;s.receipts.clear();s.receiptChecks.fill(0);
    s.transactions.clear();s.actors.clear();s.sequence=0;s.active=std::make_shared<Snapshot>();s.persistVersion=0;s.hooked=false;s.seen.clear();s.lastSeen[0]=0;
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
    if(!check(apply(1,1,id)&&updateActor(upload,model,actor,lyselia)&&poll(id,message,sizeof(message))==0,
              "conflicting modifier keeps a pending application queued for retry"))return false;
    if(!check(poseBytesEqual(second[0].destination,second[0].source),"pending transaction cannot overwrite a conflicting modifier"))return false;
    s.allowWrites=false;
    if(!check(!updateActor(upload,model,actor,lyselia)&&poseBytesEqual(second[0].destination,second[0].source),"observation gate also blocks an already published selection"))return false;
    s.allowWrites=true;
    for(const auto& p:initial)if(!writePoseBytes(p.destination,p.animation))return false;
    if(!check(updateActor(upload,model,actor,lyselia)&&poll(id,message,sizeof(message))==1,
              "pending shape resumes after the pose buffers become stable"))return false;
    for(const auto& p:initial)if(!writePoseBytes(p.destination,p.source))return false;
    s.transactions.clear();s.actors.clear();s.sequence=0;s.active=std::make_shared<Snapshot>();s.persistVersion=0;s.hooked=false;s.seen.clear();s.lastSeen[0]=0;
    return true;
}
#endif
}
