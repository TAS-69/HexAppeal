#include "wardrobe_native.h"
#include "appearance_hooks.h"
#include "game_layout.h"
#include "wardrobe_snapshot.h"
#include "wardrobe_transport.h"
#include "wardrobe_api.h"
#include "character_addons.h"
#include "wardrobe_refresh.h"
#include "barber_catalog.h"
#include "spouse_wardrobe.h"
#include "runtime.h"
#include "underwear.h"
#include "pose_access.h"
#include "core/log.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string_view>

extern "C" {
void* HexWardrobeApplyContinuation=nullptr;
void HexWardrobeApplyStock(uintptr_t node,void* removals,void* additions,uint8_t flag);
}

namespace hex {
namespace {
WardrobeSelection selections;
std::mutex mutex;
thread_local bool ticking=false;
bool engineReady=false,writesAllowed=false,traceBarberOutfit=false;
uint64_t nextPipelineProbe=0,nextStorageProbe=0;
bool selectionsReady=false;
std::string selectionIssue;
bool refreshApplyAppearance(bool beforeWrite);
std::array<uint64_t,6> armed{};
std::array<uint64_t,6> appliedGeneration{};
std::array<std::string,6> witchDetail{};
std::array<std::string,6> choiceErrors{};
std::string detail="Wardrobe integration is not ready.";
std::string slotTransportIssue="The equipment slot functions have not been checked yet.";
std::string slotStoreIssue="Waiting for wardrobe-slots.txt to load.";
uintptr_t base=0;
WardrobeSlots slotStore;
BarberCatalog barberCatalog;
SpouseBarberSource spouseBarber;
SpouseWardrobeState spouseState;
SpouseEditorHandoff spouseEditorHandoff;
std::map<unsigned,SpouseGearHandoff> spouseGearHandoffs;
uint64_t spouseReadAt=0;
uint64_t spouseEditorSettlesAt=0;
std::filesystem::file_time_type spouseLogWrite{};
uintmax_t spouseLogSize=0;
bool observeBarber=false, savedBarberLayoutReady=false, spouseLooksReady=false, slotStoreReady=false, slotTransportReady=false;
bool barberEnabled=false;
std::array<uint64_t,6> slotGeneration{};
struct WornOutfit {
    uintptr_t actor=0, node=0, control=0, renderer=0;
    uint64_t seen=0;
    uint64_t outfitChangedAt=0;
    OutfitSlots slots;
    unsigned spouseNpc=0;
    unsigned barberRevision=0;
    bool barberConfirmed=false;
    bool barberUpdateFailed=false;
};
std::array<WornOutfit,6> wornOutfits;
std::map<std::string,OutfitSlots> defaultOutfits;
std::map<std::string,uint64_t> defaultRetry;
const OutfitSlots& defaultSlots(const WardrobeSnapshot& snapshot);
struct SavedWardrobeEdit {
    bool open=false,restoring=false;
    OutfitSlots before,draft,restore;
    WardrobeItemOptions options;
};
std::array<SavedWardrobeEdit,6> savedEdits;
struct Owned {
    int witch=-1;
    uintptr_t control=0,node=0,renderer=0;
    uint64_t attempted=0,observedAt=0,lastSeen=0,slotAttempt=0,slotDeadline=0,slotRetryAt=0,slotRequest=0;
    uint64_t appearanceRetryAt=0;
    WardrobeSlotRetry slotRetry;
    bool installed=false,pending=false,unacknowledged=false;
    DWORD deadline=0;
    std::string original,current,requested,removed,refreshOutfit,desired,barberTrace;
    WardrobeRefreshCycle refresh;
    OutfitParts slotTarget;
    std::string missingSavedSlot;
    OutfitSlots itemTarget;
    OutfitParts itemVariants,missingItemVariants;
    uint64_t itemVariantsChecked=0;
    OutfitParts combatFallbacks;
    uint64_t combatFallbackChanged=0;
    bool slotTargetValid=false,cleanupFailed=false;
    std::vector<WardrobeVisibilityClaim> hiddenParts;
};
std::map<uintptr_t,Owned> owned;
struct AddonSession {
    WardrobeSlots store;
    bool loaded=false,checked=false,underwearPresent=false,followKnown=false;
    uintptr_t actor=0;
    uint64_t generation=1,seen=0,nextStoreProbe=0;
    std::string storageIssue;
    OutfitSlots worn,follow;
    SavedWardrobeEdit edit;
    Owned live;
    std::vector<WardrobeVisibilityClaim> shownBase;
    bool externalUnderwear=false;
    std::string status;
};
std::map<int,AddonSession> addonSessions;
AddonSession& addonSession(int target,const CharacterAddon& addon){
    auto [entry,inserted]=addonSessions.try_emplace(target);
    auto& session=entry->second;
    const auto now=GetTickCount64();
    if(!session.loaded&&now>=session.nextStoreProbe){
        session.nextStoreProbe=now+2000;
        const auto& profiles=Runtime::get().profiles;
        const auto file=profiles.parent_path()/L"CharacterWardrobe"/("npc_"+addon.id+".txt");
        std::string issue;
        session.loaded=!profiles.empty()&&session.store.load(file,issue,true);
        if(profiles.empty())issue="The character Wardrobe settings folder is unavailable.";
        if(!session.loaded&&!profiles.empty())issue+="\nFile: "+file.u8string();
        if(!issue.empty()&&issue!=session.storageIssue)
            LOG("Character Wardrobe %s: %s",addon.id.c_str(),issue.c_str());
        session.storageIssue=std::move(issue);
    }
    return session;
}
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
bool release(uintptr_t* wrapper){__try{reinterpret_cast<void(*)(uintptr_t*)>(base+gameLayout().releaseWrapper)(wrapper);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool destroy(WardrobeRecord* record){__try{reinterpret_cast<void(*)(void*)>(base+gameLayout().destroyRecord)(record);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool copyPart(WardrobeRecord* destination,const WardrobeRecord* source){
    __try{return reinterpret_cast<WardrobeRecord*(*)(WardrobeRecord*,const WardrobeRecord*)>(base+gameLayout().copyRecord)(destination,source)==destination;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool pipeline(uintptr_t node,WardrobeVector* removals,WardrobeVector* additions,uint8_t flag){
    if(!HexWardrobeApplyContinuation)return false;
    __try{HexWardrobeApplyStock(node,removals,additions,flag);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
template<class Visitor>
bool visitRegisteredParts(Visitor visitor){
    const auto loader=ptr(base+gameLayout().loaderGlobal),table=loader+0x50;const auto count=num(table+4),capacity=num(table+8);const auto entries=ptr(table+0x18);
    if(!loader||!entries||!count||count>capacity||count>100000)return false;
    for(uint32_t i=0;i<count;++i){const auto entry=ptr(entries+i*8),wrapper=ptr(entry+8);char candidate[192]{};
        if(text(wrapper,candidate,sizeof candidate)&&!visitor(candidate,wrapper))break;
    }
    return ptr(table+0x18)==entries&&num(table+4)==count;
}
uintptr_t canonical(const char* name){
    uintptr_t result=0;
    const auto stable=visitRegisteredParts([&](const char* candidate,uintptr_t wrapper){
        if(strcmp(candidate,name))return true;
        result=wrapper;
        return false;
    });
    return stable?result:0;
}
std::map<std::string,uintptr_t> registeredParts(const OutfitParts& parts){
    std::map<std::string,uintptr_t> found;
    if(parts.empty())return found;
    if(!visitRegisteredParts([&](const char* name,uintptr_t wrapper){
        if(parts.count(name))found.try_emplace(name,wrapper);
        return found.size()<parts.size();
    }))found.clear();
    return found;
}
bool readyRecord(const WardrobeSnapshot& snapshot,const char* expected,WardrobeRecord& record){
    const auto data=ptr(snapshot.node+0x40);const auto count=num(snapshot.node+0x48),capacity=num(snapshot.node+0x4c);
    if(!data||count>64||count>capacity)return false;
    for(uint32_t i=0;i<count;++i){WardrobeRecord candidate;const auto address=data+i*0x70;char name[192]{};
        if(!read(address,&candidate,sizeof candidate))return false;
        uintptr_t wrapper=0;memcpy(&wrapper,candidate.bytes.data(),8);
        if(text(wrapper,name,sizeof name)&&(!expected||!strcmp(name,expected))&&isSimpleWardrobeRecord(candidate,base)){
            if(ptr(snapshot.node+0x40)!=data||num(snapshot.node+0x48)!=count)return false;
            record=candidate;return true;}
    }return false;
}
std::string packedMesh(const std::array<uint8_t,16>& choices){
    std::string packed;
    for(const auto choice:choices){
        constexpr char hex[]="0123456789abcdef";
        packed+=hex[choice>>4];
        packed+=hex[choice&15];
    }
    return packed;
}
void observeBarberOutfit(const WardrobeSnapshot& snapshot,Owned& entry){
    std::array<uint8_t,16> choices{};
    if(!captureBarberMeshChoices(snapshot,base,choices))return;
    std::string observation="mesh="+packedMesh(choices);
    std::map<std::string,std::pair<unsigned,unsigned>> clothes;
    for(uint32_t i=0;i<snapshot.claimCount;++i){
        const auto& claim=snapshot.claims[i];
        const std::string_view prefab=claim.prefab;
        constexpr std::string_view garmentKinds[]={"_ub_","_lb_","_foot_","_hel_","_hand_","_cloak_","_necklace_"};
        if(claim.kind!=WardrobePartKind::Underwear&&!defaultOutfitPart(snapshot.witch,claim.prefab)&&std::none_of(std::begin(garmentKinds),std::end(garmentKinds),[&](auto part){return prefab.find(part)!=std::string_view::npos;}))continue;
        auto& counts=clothes[std::string(prefab)];
        ++counts.first;
        bool visible=false;
        if(readOutfitPartVisibility(snapshot.renderer,claim.key,visible)&&visible)++counts.second;
    }
    for(const auto& [prefab,counts]:clothes){
        WardrobeRecord record;
        observation+=" "+prefab+"="+std::to_string(counts.second)+"/"+std::to_string(counts.first);
        if(readyRecord(snapshot,prefab.c_str(),record))observation+="(ready)";
    }
    if(observation==entry.barberTrace)return;
    entry.barberTrace=std::move(observation);
    LOG("Barber outfit observation witch=%d actor=%p node=%p %s.",snapshot.witch,reinterpret_cast<void*>(snapshot.actor),reinterpret_cast<void*>(snapshot.node),entry.barberTrace.c_str());
}
void confirmOutfitHidden(const WardrobeSnapshot& snapshot,bool hidden,bool complete,bool emptyProfile){
    bool basePresent=false;
    for(uint32_t i=0;i<snapshot.claimCount;++i)basePresent|=snapshot.claims[i].kind==WardrobePartKind::Underwear;
    setOutfitHiddenActor(snapshot.actor,snapshot.renderer,snapshot.witch,hidden,complete,basePresent,emptyProfile);
}
bool checkSlotTransport(){
    const unsigned char copyStart[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x6c,0x24,0x20,0x48,0x89,0x4c,0x24,0x08};
    const unsigned char copyTail[]={0x57,0x41,0x56,0x48,0x83,0xec,0x20,0x4c,0x8b,0xf2,0x48,0x8b,0xf1,0x33,0xed};
    const unsigned char copyRetain[]={0x8b,0x41,0x10,0x85,0xc0,0x78,0x04,0xf0,0xff,0x41,0x10};
    auto* entry=reinterpret_cast<void*>(base+gameLayout().copyRecord);
    slotTransportIssue.clear();
    if(appearance::checkedGameEntry(entry,copyStart,16,copyTail)==appearance::GameEntry::Unavailable)
        slotTransportIssue=appearance::entryIssue(entry,"The game's equipped part copy");
    else if(!bytes(base+gameLayout().copyRecord+0x34,copyRetain,sizeof copyRetain))
        slotTransportIssue=appearance::entryIssue(entry,"The equipped part copy's reference retention instructions");
    return slotTransportIssue.empty();
}
void configureSlots(const std::filesystem::path& ini){
    slotTransportReady=checkSlotTransport();
    const auto fallback=GetPrivateProfileIntW(L"Development",L"EnableBarberOutfits",1,ini.c_str());
    barberEnabled=GetPrivateProfileIntW(L"Wardrobe",L"EnableBarberOutfits",fallback,ini.c_str())!=0;
    LOG("Native Wardrobe slots: transport=%d.",slotTransportReady);
}
bool visibilityConfirmed(uintptr_t renderer,uint32_t key,bool wanted){
    bool visible=false;
    return writeOutfitPartVisibility(renderer,key,wanted)&&readOutfitPartVisibility(renderer,key,visible)&&visible==wanted;
}
void discardReplacedVisibilityClaims(const WardrobeSnapshot& snapshot,std::vector<WardrobeVisibilityClaim>& hidden,
                                     bool rebindAddonMasks=false){
    for(auto part=hidden.begin();part!=hidden.end();){
        if(hasWardrobeVisibilityClaim(snapshot,*part)||(rebindAddonMasks&&rebindAddonVisibilityClaim(snapshot,*part)))++part;
        else part=hidden.erase(part);
    }
}
bool captureObservedWitch(uintptr_t actor,int witch,WardrobeSnapshot& snapshot,const char** issue=nullptr){
    if(captureWardrobeSnapshot(actor,witch,base,snapshot,nullptr,issue))return true;
    ActorIdentity identity{};
    return readWardrobeIdentity(reinterpret_cast<void*>(actor),identity)&&identity.recordKey&&identity.witch==witch&&
        captureWardrobeSnapshot(actor,witch,base,snapshot,nullptr,issue,true);
}
bool sameOutfitSnapshot(const WardrobeSnapshot& snapshot,WardrobeSnapshot& fresh){
    const auto* addon=characterAddon(snapshot.witch);
    const bool captured=addon?captureWardrobeSnapshot(snapshot.actor,snapshot.witch,base,fresh,addon->appearance.c_str()):
        captureObservedWitch(snapshot.actor,snapshot.witch,fresh);
    if(!captured||std::strcmp(fresh.appearance,snapshot.appearance)||
       fresh.control!=snapshot.control||fresh.node!=snapshot.node||fresh.renderer!=snapshot.renderer||
       fresh.claimCount!=snapshot.claimCount||fresh.claimData!=snapshot.claimData)return false;
    for(uint32_t i=0;i<snapshot.claimCount;++i)
        if(!(wardrobeVisibilityClaim(snapshot,snapshot.claims[i])==wardrobeVisibilityClaim(fresh,fresh.claims[i])))return false;
    return true;
}
std::string slotFor(const WardrobeSnapshot& snapshot,const WardrobeClaim& claim,const SlotProfile& profile){
    std::string name=claim.prefab;
    if(claim.kind==WardrobePartKind::Underwear||protectedOutfitPart(name)||
       !validOutfitPart(name))return {};
    for(const auto* look:{&profile.saved,&profile.barber,&profile.combat})
        for(const auto& [slot,parts]:*look)if(parts.count(name))return slot;
    auto slot=outfitSlotForPart(name);
    if(!slot.empty())return slot;
    slot=barberCatalog.slotFor(name);
    if(!slot.empty())return slot;
    return defaultOutfitPart(snapshot.witch,claim.prefab)?"Armour":"";
}
OutfitSlots wornSlots(const WardrobeSnapshot& snapshot,const SlotProfile& profile){
    OutfitSlots current;
    for(uint32_t i=0;i<snapshot.claimCount;++i){
        const auto& claim=snapshot.claims[i];
        const auto slot=slotFor(snapshot,claim,profile);
        if(!slot.empty())current[slot].insert(claim.prefab);
    }
    return current;
}
void refreshSpouseWardrobe(uint64_t now,bool nativeWardrobeOpened=false){
    if(!barberEnabled||(now<spouseReadAt&&!nativeWardrobeOpened))return;
    spouseReadAt=now+250;
    if(!observeBarber){
        if(!GetModuleHandleW(L"SpouseOverhaul.asi"))return;
        const auto definition=Runtime::get().dependencyStatus()->barberMeshDefinition;
        if(definition.empty()||!barberCatalog.load(definition)){
            spouseReadAt=now+2000;
            return;
        }
        observeBarber=true;
        savedBarberLayoutReady=true;
        for(size_t i=8;i<16;++i)savedBarberLayoutReady&=barberCatalog.isArmorSlot(i);
        LOG("Spouse Overhaul's mounted Barber catalog is available.");
    }
    const auto& bin=Runtime::get().folder;
    const auto log=bin/L"SpouseOverhaul.log";
    std::error_code error;
    const auto write=std::filesystem::last_write_time(log,error);
    const auto size=error?0:std::filesystem::file_size(log,error);
    if(!error&&(nativeWardrobeOpened||write!=spouseLogWrite||size!=spouseLogSize)){
        auto state=activeSpouseWardrobe(log);
        const bool recovered=spouseEditorHandoff.reconcile(state,nativeWardrobeOpened);
        const bool changedSave=state.save!=spouseState.save||state.saveEpoch!=spouseState.saveEpoch;
        if(changedSave){
            spouseLooksReady=false;
            spouseGearHandoffs.clear();
            spouseEditorSettlesAt=now+750;
        }
        for(const auto& [npc,revision]:state.gearRevision)spouseGearHandoffs[npc].observe(now,revision);
        for(int witch=0;witch<int(wornOutfits.size());++witch){
            auto& worn=wornOutfits[witch];
            if(changedSave){
                worn.barberConfirmed=false;
                worn.spouseNpc=0;
                worn.barberRevision=0;
                slotStore.observe(witch,SlotOutfit::Barber,{});
                slotStore.observe(witch,SlotOutfit::Combat,{});
                ++slotGeneration[witch];
            }else if(state.barberEdits!=spouseState.barberEdits){
                const auto next=state.barberEdits.find(worn.spouseNpc);
                const auto previous=spouseState.barberEdits.find(worn.spouseNpc);
                if(worn.spouseNpc&&(next==state.barberEdits.end()||
                   (previous!=spouseState.barberEdits.end()&&next->second==previous->second)))continue;
                worn.barberConfirmed=false;
                spouseEditorSettlesAt=now+750;
                if(slotStore.profile(witch).selected==SlotOutfit::Barber){
                    for(auto& [actor,entry]:owned){
                        if(entry.witch!=witch)continue;
                        entry.slotAttempt=slotGeneration[witch];
                        entry.slotDeadline=0;
                        entry.slotTargetValid=true;
                    }
                }
            }
        }
        if(spouseState.editorOpen()&&!state.editorOpen())spouseEditorSettlesAt=now+750;
        if(nativeWardrobeOpened&&recovered){
            spouseEditorSettlesAt=0;
            LOG("Wardrobe resumed after a native interaction superseded a stale Spouse Overhaul editor event.");
        }
        spouseState=std::move(state);
        spouseLogWrite=write;
        spouseLogSize=size;
    }else if(error&&nativeWardrobeOpened&&spouseEditorHandoff.reconcile(spouseState,true)){
        spouseEditorSettlesAt=0;
        LOG("Wardrobe resumed after the game confirmed a native interaction.");
    }
    spouseLooksReady=!spouseState.save.empty()&&
        (spouseBarber.refresh(bin/L"SpouseOverhaul.data.json",barberCatalog)||spouseLooksReady);
}
bool settlingSpouseGear(unsigned npc,uint64_t now){
    const auto handoff=spouseGearHandoffs.find(npc);
    return handoff!=spouseGearHandoffs.end()&&handoff->second.settling(now);
}
bool rememberWorn(const WardrobeSnapshot& snapshot,const OutfitSlots& slots,uint64_t now,uint64_t observedAt,bool focused=false){
    auto& worn=wornOutfits[snapshot.witch];
    const bool different=worn.actor!=snapshot.actor||worn.node!=snapshot.node||
        worn.control!=snapshot.control||worn.renderer!=snapshot.renderer;
    const bool previousLive=wardrobeActorRecentlySeen(now,worn.seen);
    if(different&&previousLive&&!focused){
        WardrobeSnapshot previous;
        if(captureObservedWitch(worn.actor,snapshot.witch,previous)&&
           previous.node==worn.node&&previous.control==worn.control&&previous.renderer==worn.renderer)return false;
    }
    if(different||!previousLive){worn.barberConfirmed=false;worn.spouseNpc=0;}
    if(different||!previousLive||worn.slots!=slots)worn.outfitChangedAt=now;
    worn.actor=snapshot.actor;worn.node=snapshot.node;worn.control=snapshot.control;
    worn.renderer=snapshot.renderer;worn.seen=observedAt;worn.slots=slots;
    if(!observeBarber||!slotStoreReady)return true;
    std::array<uint8_t,16> choices{};
    if(!captureBarberMeshChoices(snapshot,base,choices))return true;
    OutfitSlots archived,live;
    const auto spouseNpc=spouseLooksReady?spouseBarber.find(spouseState.save,choices,barberCatalog,archived):0;
    if(spouseNpc)worn.spouseNpc=spouseNpc;
    if(settlingSpouseGear(worn.spouseNpc,now))return true;
    const auto current=outfitSlotParts(slots);
    const auto edit=spouseState.barberEdits.find(worn.spouseNpc);
    const bool edited=edit!=spouseState.barberEdits.end();
    OutfitSlots committed;
    const bool resolvedCommit=edited&&edit->second.choices&&savedBarberLayoutReady&&
        barberCatalog.resolveSlots(*edit->second.choices,committed);
    const bool resolvedLive=barberCatalog.resolveSlots(choices,live);
    const bool matchesLive=resolvedLive&&(!live.empty()?outfitPartsPresent(current,outfitSlotParts(live)):
        edited&&snapshot.defaultGarment&&!slots.empty())&&(!resolvedCommit||live==committed);
    const auto& before=slotStore.profile(snapshot.witch);
    if(spouseState.usesCombatLook(worn.spouseNpc)){
        if(savedBarberLayoutReady&&!savedEdits[snapshot.witch].open&&now-worn.outfitChangedAt>=500&&snapshot.customGarment){
            const auto borrowed=barberCatalog.combatFallbacks(choices);
            OutfitSlots combat;
            for(const auto& [slot,parts]:slots)
                for(const auto& part:parts)
                    if(!borrowed.count(part))combat[slot].insert(part);
            if(!combat.empty()&&before.combat!=combat&&slotStore.observe(snapshot.witch,SlotOutfit::Combat,std::move(combat)))
                LOG("Native Combat outfit captured for witch=%d.",snapshot.witch);
        }
        return true;
    }
    const bool onActor=matchesLive;
    const bool useArchive=!onActor&&spouseNpc&&!edited;
    if(!onActor&&!useArchive&&!resolvedCommit)return true;
    auto look=onActor?std::move(live):resolvedCommit?std::move(committed):std::move(archived);
    if(resolvedCommit&&look.empty())look=defaultSlots(snapshot);
    const auto barber=outfitSlotParts(look);
    if(onActor||!resolvedCommit){
        const auto& inherited=onActor?slots:before.barber;
        for(const auto& [slot,parts]:inherited){
            for(const auto& name:parts)
                if(!barber.count(name)&&defaultOutfitPart(snapshot.witch,name.c_str()))look[slot].insert(name);
        }
    }
    const bool changed=before.barber!=look;
    const bool hasLook=!look.empty();
    if(changed&&!slotStore.observe(snapshot.witch,SlotOutfit::Barber,std::move(look))){
        if(!worn.barberUpdateFailed)LOG_ERR("Barber slots could not be resolved for witch=%d.",snapshot.witch);
        worn.barberUpdateFailed=true;
        return true;
    }
    worn.barberConfirmed=true;
    worn.barberUpdateFailed=false;
    if(changed)LOG("Barber slot profile refreshed for witch=%d from %s.",snapshot.witch,
        resolvedCommit?"committed slots":onActor?"equipped slots":"Spouse Overhaul saves");
    if(edited&&worn.barberRevision!=edit->second.revision&&hasLook){
        worn.barberRevision=edit->second.revision;
        const auto witch=snapshot.witch;
        if(!savedEdits[witch].open&&!savedEdits[witch].restoring&&before.selected!=SlotOutfit::Game){
            std::string error;
            if(slotStore.select(witch,SlotOutfit::Barber,error))++slotGeneration[witch];
            else LOG_ERR("%s",error.c_str());
        }
        LOG("Barber save accepted for witch=%d npc=%u revision=%u from %s.",witch,worn.spouseNpc,worn.barberRevision,
            resolvedCommit?"committed slots":"equipped slots");
    }
    return true;
}
bool applySlotVisibility(const WardrobeSnapshot& snapshot,Owned& entry,const SlotProfile& profile,bool hideAll,bool allowSlots=true,const OutfitParts* allowed=nullptr,bool providerReady=true,const OutfitParts* suppressed=nullptr,const OutfitParts* underwear=nullptr,bool hideBase=false,bool rebindAddonMasks=false){
    const auto& hiddenSlots=profile.hidden;
    if(!hideBase&&!hideAll&&hiddenSlots.empty()&&entry.hiddenParts.empty()&&!allowed&&(!suppressed||suppressed->empty()))return true;
    WardrobeSnapshot fresh;
    if(!sameOutfitSnapshot(snapshot,fresh))return false;
    discardReplacedVisibilityClaims(fresh,entry.hiddenParts,rebindAddonMasks);
    bool ok=true;
    const auto sharedWithOtherPart=[&](uint32_t key,const auto& ownsPart){
        return std::any_of(fresh.claims.begin(),fresh.claims.begin()+fresh.claimCount,[&](const WardrobeClaim& part){
            return part.key==key&&!ownsPart(part);
        });
    };
    const auto belongs=[&](const WardrobeClaim& claim){
        if(hideBase&&claim.kind==WardrobePartKind::Underwear){
            if(!sharedWithOtherPart(claim.key,[](const auto& part){return part.kind==WardrobePartKind::Underwear;}))return true;
            ok=false;
        }
        if(suppressed&&suppressed->count(claim.prefab)&&!protectedOutfitPart(claim.prefab)){
            if(!sharedWithOtherPart(claim.key,[&](const auto& part){return suppressed->count(part.prefab)!=0;}))return true;
            ok=false;
        }
        if(entry.installed&&entry.current==claim.prefab)return !providerReady&&hideAll;
        const auto slot=slotFor(fresh,claim,profile);
        const bool hideWithOutfit=hideAll&&(!underwear||!underwear->count(claim.prefab));
        return !slot.empty()&&(hideWithOutfit||(allowSlots&&hiddenSlots.count(slot))||(allowed&&!allowed->count(claim.prefab)));
    };
    std::vector<WardrobeVisibilityClaim> targets;
    for(uint32_t i=0;i<fresh.claimCount;++i)
        if(belongs(fresh.claims[i]))targets.push_back(wardrobeVisibilityClaim(fresh,fresh.claims[i]));
    for(auto it=entry.hiddenParts.begin();it!=entry.hiddenParts.end();){
        if(std::find(targets.begin(),targets.end(),*it)!=targets.end()){++it;continue;}
        if(sharedWithOtherPart(it->key,[&](const auto& part){return it->prefab==part.prefab;})){
            it=entry.hiddenParts.erase(it);
            continue;
        }
        if(visibilityConfirmed(fresh.renderer,it->key,true))it=entry.hiddenParts.erase(it);
        else {ok=false;++it;}
    }
    for(const auto& claim:targets){
        bool visible=false;
        if(!readOutfitPartVisibility(fresh.renderer,claim.key,visible)){ok=false;continue;}
        if(!visible)continue;
        if(visibilityConfirmed(fresh.renderer,claim.key,false)){
            if(std::find(entry.hiddenParts.begin(),entry.hiddenParts.end(),claim)==entry.hiddenParts.end())entry.hiddenParts.push_back(claim);
        }else ok=false;
    }
    return ok;
}
bool applyCombatVisibility(const WardrobeSnapshot& snapshot,Owned& entry,const SlotProfile& profile,bool hideAll,bool editingCombat,uint64_t now){
    std::array<uint8_t,16> choices{};
    if(!savedBarberLayoutReady||!captureBarberMeshChoices(snapshot,base,choices))return false;
    const auto borrowed=barberCatalog.combatFallbacks(choices);
    OutfitParts present;
    for(uint32_t i=0;i<snapshot.claimCount;++i)
        if(borrowed.count(snapshot.claims[i].prefab))present.insert(snapshot.claims[i].prefab);
    if(present!=entry.combatFallbacks){
        entry.combatFallbacks=std::move(present);
        entry.combatFallbackChanged=now;
    }
    if(entry.combatFallbackChanged&&now-entry.combatFallbackChanged<250)return false;
    const bool masked=applySlotVisibility(snapshot,entry,profile,hideAll&&!editingCombat,!editingCombat,nullptr,true,&entry.combatFallbacks);
    if(!editingCombat)confirmOutfitHidden(snapshot,hideAll,masked,false);
    return masked;
}
OutfitSlots observedDefaultSlots(const WardrobeSnapshot& snapshot){
    OutfitSlots slots;
    const auto data=ptr(snapshot.node+0x40);const auto count=num(snapshot.node+0x48),capacity=num(snapshot.node+0x4c);
    if(!data||!count||count>64||count>capacity)return {};
    for(uint32_t i=0;i<count;++i){
        char name[192]{};
        const auto wrapper=ptr(data+i*sizeof(WardrobeRecord));
        if(!text(wrapper,name,sizeof name)||!defaultOutfitPart(snapshot.witch,name))continue;
        auto slot=outfitSlotForPart(name);
        if(slot.empty())slot="Armour";
        slots[slot].insert(name);
    }
    if(ptr(snapshot.node+0x40)!=data||num(snapshot.node+0x48)!=count)return {};
    return slots;
}
const OutfitSlots& defaultSlots(const WardrobeSnapshot& snapshot){
    static const OutfitSlots empty;
    auto route=std::string(snapshot.appearance);
    std::replace(route.begin(),route.end(),'\\','/');
    for(auto& ch:route)if(ch>='A'&&ch<='Z')ch+=char('a'-'A');
    if(route.rfind("character/appearance/",0)!=0||route.find("..")!=route.npos)return empty;
    auto [entry,first]=defaultOutfits.try_emplace(route);
    const auto now=GetTickCount64();
    auto& retry=defaultRetry[entry->first];
    if((first||entry->second.empty())&&now>=retry){
        retry=now+2000;
        auto& runtime=Runtime::get();
        auto manifest=runtime.dependencyStatus()->manifest.path;
        if(manifest.empty())manifest=runtime.folder/L"dmm/manifest.tsv";
        const auto mounted=readMountedRoutes(manifest);
        const auto active=mounted.find(route);
        if(active!=mounted.end())entry->second=readDefaultWardrobeSlots(active->second,snapshot.witch);
        if(entry->second.empty())
            entry->second=readDefaultWardrobeSlots(manifest.parent_path()/L"files"/route,snapshot.witch);
    }
    if(entry->second.empty()&&snapshot.defaultGarment&&!snapshot.customGarment){
        auto observed=observedDefaultSlots(snapshot);
        for(const auto& [slot,parts]:observed)for(const auto& part:parts)
            if(wardrobePartKind(snapshot.witch,part.c_str())==WardrobePartKind::DefaultGarment){
                entry->second=std::move(observed);
                return entry->second;
            }
    }
    return entry->second;
}
bool queueSlotLook(const WardrobeSnapshot& snapshot,Owned& entry,const OutfitSlots& worn,const OutfitSlots& target,
                   SlotOutfit selected,uint64_t generation,const OutfitSlots& original,std::string& status){
    OutfitSlots availableTarget=target;
    entry.missingSavedSlot.clear();
    if(selected==SlotOutfit::Saved){
        const auto wornParts=outfitSlotParts(worn);
        auto requested=outfitSlotParts(target);
        for(const auto& part:wornParts)requested.erase(part);
        const auto mounted=registeredParts(requested);
        for(const auto& [slot,parts]:target){
            const auto missing=std::find_if(parts.begin(),parts.end(),[&](const auto& part){
                if(!requested.count(part)||mounted.count(part))return false;
                WardrobeRecord record;
                return !readyRecord(snapshot,part.c_str(),record);
            });
            if(missing==parts.end())continue;
            if(entry.missingSavedSlot.empty())entry.missingSavedSlot=slot+" ("+*missing+")";
            const auto current=worn.find(slot);
            if(current==worn.end())availableTarget.erase(slot);
            else availableTarget[slot]=current->second;
        }
    }
    const bool includeRetainedParts=selected==SlotOutfit::Saved||selected==SlotOutfit::Barber;
    const auto changes=outfitSlotChanges(worn,availableTarget,includeRetainedParts);
    if(changes.add.empty()&&changes.remove.empty()){
        entry.slotAttempt=generation;
        entry.slotDeadline=0;entry.slotTarget=outfitSlotParts(availableTarget);
        entry.slotTargetValid=true;
        status=entry.missingSavedSlot.empty()?"Outfit / active":"Outfit / waiting for "+entry.missingSavedSlot;
        return true;
    }
    const auto registry=registeredParts(changes.add);
    std::vector<uintptr_t> removals;
    for(const auto& name:changes.remove){
        bool found=false;
        for(uint32_t i=0;i<snapshot.claimCount;++i){
            const auto& claim=snapshot.claims[i];
            char nativeName[192]{};
            if(name!=claim.prefab||!text(claim.wrapper,nativeName,sizeof nativeName)||name!=nativeName)continue;
            found=true;
            if(std::find(removals.begin(),removals.end(),claim.wrapper)==removals.end())removals.push_back(claim.wrapper);
        }
        if(!found){status="Outfit / waiting for equipped part "+name;return false;}
    }
    WardrobeRecord prototype;
    bool hasPrototype=false;
    for(const auto* look:{&worn,&original}){
        for(const auto& [slot,parts]:*look){
            for(const auto& name:parts)if(readyRecord(snapshot,name.c_str(),prototype)){hasPrototype=true;break;}
            if(hasPrototype)break;
        }
        if(hasPrototype)break;
    }
    if(!hasPrototype)hasPrototype=readyRecord(snapshot,nullptr,prototype);
    std::vector<WardrobeRecord> records;
    for(const auto& name:changes.add){
        WardrobeRecord record;
        if(!readyRecord(snapshot,name.c_str(),record)){
            const auto registered=registry.find(name);
            if(registered==registry.end()){status="Outfit / part unavailable: "+name;return false;}
            if(!hasPrototype){status="Outfit / waiting for native part template";return false;}
            record=prototype;
            const auto wrapper=registered->second;
            std::memcpy(record.bytes.data(),&wrapper,sizeof wrapper);
            if(!isSimpleWardrobeRecord(record,base)){status="Outfit / unsupported part: "+name;return false;}
        }
        records.push_back(record);
    }
    WardrobeSnapshot fresh;
    if(!sameOutfitSnapshot(snapshot,fresh)){status="Outfit / waiting for stable assembly";return false;}
    if(!refreshApplyAppearance(true)){status=detail;return false;}
    if(!slotTransportReady){status=slotTransportIssue;return false;}
    std::vector<const WardrobeRecord*> additions;
    for(const auto& record:records)additions.push_back(&record);
    const auto result=invokeWardrobeRecipe(snapshot.node,removals,additions,base,{retain,release,destroy,pipeline,nullptr,copyPart});
    entry.slotAttempt=generation;
    if(result!=WardrobeCallResult::Completed){
        entry.cleanupFailed=result==WardrobeCallResult::CleanupFailed;
        entry.slotAttempt=0;
        entry.slotRetryAt=GetTickCount64()+2000;
        status=entry.cleanupFailed?"Outfit / waiting for a new render instance after record cleanup failed":
            "Outfit / retrying after the game rejected the last change";
        LOG_ERR("Native slot switch failed for character=%d, result=%d.",snapshot.witch,int(result));
        return false;
    }
    entry.slotTarget=outfitSlotParts(availableTarget);entry.slotDeadline=GetTickCount64()+5000;
    entry.slotTargetValid=true;
    status="Outfit / waiting for assembly";
    LOG("Wardrobe slots queued witch=%d profile=%d remove=%zu add=%zu.",snapshot.witch,int(selected),removals.size(),additions.size());
    return true;
}
void updateSlotLook(const WardrobeSnapshot& snapshot,Owned& entry,const OutfitSlots& current,const SlotProfile& profile,const OutfitSlots* saved,bool masked,std::string& status){
    auto& edit=savedEdits[snapshot.witch];
    const auto selected=edit.open||edit.restoring?SlotOutfit::Saved:profile.selected;
    const auto now=GetTickCount64();
    const auto generation=slotGeneration[snapshot.witch];
    if(entry.slotRequest!=generation){
        entry.slotRequest=generation;
        entry.slotRetry={};
        entry.slotRetryAt=0;
        entry.missingSavedSlot.clear();
    }
    if(selected==SlotOutfit::Saved&&!entry.slotDeadline&&!entry.missingSavedSlot.empty()&&now>=entry.slotRetryAt){
        entry.slotAttempt=0;
        entry.slotRetry={};
    }
    if(selected==SlotOutfit::Game){status="Outfit / following game";return;}
    if(entry.cleanupFailed){status="Outfit / waiting for a new render instance after record cleanup failed";return;}
    if(selected==SlotOutfit::Barber&&!wornOutfits[snapshot.witch].barberConfirmed){
        status="Outfit / waiting to verify the current Barber look";
        return;
    }
    if(entry.slotAttempt!=generation){
        if(now<entry.slotRetryAt)return;
        entry.slotRetryAt=now+1000;
        entry.slotDeadline=0;entry.slotTarget.clear();entry.slotTargetValid=false;
        OutfitSlots target;
        switch(selected){
            case SlotOutfit::Barber:target=profile.barber;break;
            case SlotOutfit::Saved:target=saved?*saved:profile.saved;break;
            default:target=defaultSlots(snapshot);break;
        }
        if(target.empty()&&!(selected==SlotOutfit::Saved&&(edit.open||edit.restoring||profile.hasSaved))){
            status=selected==SlotOutfit::Default?"Default outfit / waiting for appearance data":"Outfit / waiting for selected slots";return;
        }
        if(queueSlotLook(snapshot,entry,current,target,selected,generation,defaultSlots(snapshot),status)&&!entry.slotDeadline){edit.restore.clear();edit.restoring=false;}
        return;
    }
    auto worn=outfitSlotParts(current);
    if(selected==SlotOutfit::Saved&&masked){
        for(auto part=worn.begin();part!=worn.end();)
            if(!entry.slotTarget.count(*part))part=worn.erase(part);else++part;
    }
    const bool active=worn==entry.slotTarget;
    if(selected==SlotOutfit::Barber&&entry.slotTargetValid&&!entry.slotDeadline){
        entry.slotTarget=outfitSlotParts(profile.barber);
        status=worn==entry.slotTarget?"Outfit / active":"Outfit / following Barber";
        return;
    }
    const bool settled=now-wornOutfits[snapshot.witch].outfitChangedAt>=750;
    if(entry.slotTargetValid&&entry.slotRetry.tryAgain(now,active,settled,entry.slotDeadline)){
        if(selected==SlotOutfit::Barber){
            std::string missing,extra;
            for(const auto& part:entry.slotTarget)if(!worn.count(part))missing+=" "+part;
            for(const auto& part:worn)if(!entry.slotTarget.count(part))extra+=" "+part;
            LOG("Barber assembly mismatch witch=%d. Missing:%s. Extra:%s.",snapshot.witch,
                missing.empty()?" none":missing.c_str(),extra.empty()?" none":extra.c_str());
        }
        entry.slotAttempt=0;
        entry.slotRetryAt=now+250;
        status="Outfit / restoring selected profile";
        LOG("Wardrobe retry witch=%d profile=%d attempt=%u.",snapshot.witch,int(selected),entry.slotRetry.attempts);
        return;
    }
    if(!entry.slotDeadline){
        if(entry.slotTargetValid)status=!active?"Outfit changed in game":
            entry.missingSavedSlot.empty()?"Outfit / active":"Outfit / waiting for "+entry.missingSavedSlot;
        return;
    }
    if(active){
        edit.restore.clear();
        edit.restoring=false;
        entry.slotDeadline=0;
        Runtime::get().requestShapeReapply(snapshot.witch);
        status="Outfit / active";
        LOG("Native outfit slots acknowledged witch=%d.",snapshot.witch);
    }else if(now>=entry.slotDeadline&&status!="Outfit / assembly unconfirmed"){
        status="Outfit / assembly unconfirmed";
        for(const auto& part:entry.slotTarget)if(!worn.count(part))
            LOG("Wardrobe witch=%d missing from assembly: %s.",snapshot.witch,part.c_str());
        for(const auto& part:worn)if(!entry.slotTarget.count(part))
            LOG("Wardrobe witch=%d still present after assembly: %s.",snapshot.witch,part.c_str());
    }
}
void tickSlotLook(const WardrobeSnapshot& snapshot,Owned& entry,const OutfitSlots& current,const SlotProfile& profile,const OutfitSlots* saved,bool masked,std::string& status){
    const auto previous=status;
    const auto& edit=savedEdits[snapshot.witch];
    const auto selected=edit.open||edit.restoring?SlotOutfit::Saved:profile.selected;
    updateSlotLook(snapshot,entry,current,profile,saved,masked,status);
    if(status!=previous&&selected!=SlotOutfit::Game)
        LOG("Wardrobe witch=%d profile=%d: %s.",snapshot.witch,int(selected),status.c_str());
}
std::string defaultGarment(const WardrobeSnapshot& snapshot){for(uint32_t i=0;i<snapshot.claimCount;++i)if(snapshot.claims[i].kind==WardrobePartKind::DefaultGarment)return snapshot.claims[i].prefab;return {};}
WardrobeCapabilities capabilities(){return {engineReady&&writesAllowed&&slotTransportReady&&slotStoreReady};}
bool checkApplyAppearance(std::string& issue){
    const auto* layout=loadedGameLayout();
    if(!layout){
        HexWardrobeApplyContinuation=nullptr;
        issue="This Crimson Desert build has no validated native outfit application layout.";
        return false;
    }
    constexpr size_t replayedPrologueSize=15;
    const unsigned char pipelineTail[]={0x48,0x89,0x4c,0x24,0x08,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,
        0x48,0x8d,0xac,0x24,0x90,0xe4,0xff,0xff,0xb8,0x70,0x1c,0x00,0x00,0xe8};
    auto* entry=reinterpret_cast<unsigned char*>(base+layout->applyAppearance);
    auto* continuation=entry+replayedPrologueSize;
    unsigned char actual[sizeof pipelineTail]{};
    if(!creation::ExecutableAddress(continuation)||!creation::Read(continuation,actual,sizeof actual)||
        std::memcmp(actual,pipelineTail,sizeof actual)!=0){
        HexWardrobeApplyContinuation=nullptr;
        issue=appearance::entryIssue(entry,"The game's outfit application continuation");
        return false;
    }
    if(!HexWardrobeApplyContinuation)LOG("Wardrobe outfit writes use Hex Appeal's native outfit application continuation.");
    HexWardrobeApplyContinuation=continuation;
    return true;
}
bool checkOutfitCleanup(std::string& issue){
    const unsigned char destroyBytes[]={0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
    const unsigned char destroyTail[]={0x8b,0xf9,0x48,0x83,0xc1,0x60,0x48,0x89,0x4c,0x24,0x38,0x48,0x8d,0x15};
    const unsigned char releaseBytes[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20};
    const unsigned char releaseTail[]={0x5f,0x8b,0x43,0x10,0x85,0xc0,0x78,0x58,0xb8,0xff,0xff,0xff,0xff,0xf0,0x0f,0xc1};
    auto* destroy=reinterpret_cast<void*>(base+gameLayout().destroyRecord);
    auto* release=reinterpret_cast<void*>(base+gameLayout().releaseWrapper);
    if(appearance::checkedGameEntry(destroy,destroyBytes,16,destroyTail)==appearance::GameEntry::Unavailable)
        issue=appearance::entryIssue(destroy,"The game's outfit record cleanup");
    else if(appearance::checkedGameEntry(release,releaseBytes,32,releaseTail)==appearance::GameEntry::Unavailable)
        issue=appearance::entryIssue(release,"The game's outfit part reference release");
    return issue.empty();
}
void reportWardrobeReadiness(){
    auto& runtime=Runtime::get();
    const bool visibilityReady=underwearStatus(-1).ready;
    if(writesAllowed&&engineReady&&slotTransportReady&&slotStoreReady){
        runtime.reportCompatibility(Feature::Wardrobe,CompatibilityState::Ready,
            visibilityReady?"Native outfit and renderer visibility checks passed.":"Native outfit switching is available.");
    }else{
        runtime.reportCompatibility(Feature::Wardrobe,
            nextPipelineProbe?CompatibilityState::Waiting:CompatibilityState::Blocked,
            visibilityReady?detail+" Outfit visibility is available independently.":detail);
    }
}
void refreshWardrobeStorage(uint64_t now){
    if(now<nextStorageProbe||(slotStoreReady&&selectionsReady))return;
    nextStorageProbe=now+2000;
    const auto& profiles=Runtime::get().profiles;
    if(profiles.empty())return;
    const auto root=profiles.parent_path();
    if(!slotStoreReady){
        const auto path=root/L"wardrobe-slots.txt";
        std::string issue;
        slotStoreReady=slotStore.load(path,issue);
        if(!slotStoreReady)issue+="\nFile: "+path.u8string();
        if(!issue.empty()&&issue!=slotStoreIssue)LOG("Wardrobe slots: %s",issue.c_str());
        if(slotStoreReady)for(auto& generation:slotGeneration)++generation;
        slotStoreIssue=std::move(issue);
    }
    if(!selectionsReady){
        const auto path=root/L"wardrobe.ini";
        std::string issue;
        selectionsReady=selections.load(path,issue);
        if(selectionsReady)armed=selections.snapshot().generations;
        else issue+="\nFile: "+path.u8string();
        if(!issue.empty()&&issue!=selectionIssue)LOG_ERR("%s",issue.c_str());
        selectionIssue=std::move(issue);
    }
}
bool refreshApplyAppearance(bool beforeWrite){
    const auto now=GetTickCount64();
    if(!nextPipelineProbe)return engineReady;
    if(!beforeWrite&&now<nextPipelineProbe)return engineReady;
    nextPipelineProbe=now+1000;
    refreshWardrobeStorage(now);
    std::string issue;
    const bool pipelineReady=checkApplyAppearance(issue);
    const bool copyReady=checkSlotTransport();
    if(slotTransportReady!=copyReady)LOG("Wardrobe native slot copy route ready=%d.",copyReady);
    slotTransportReady=copyReady;
    engineReady=pipelineReady&&checkOutfitCleanup(issue);
    detail=!engineReady?issue:!copyReady?slotTransportIssue:!slotStoreReady?slotStoreIssue:
        "Native Wardrobe ready. Save equipped parts or choose a supported outfit.";
    reportWardrobeReadiness();
    return engineReady;
}
uint32_t refreshDelay(){const auto ini=Runtime::get().folder/L"HexAppeal.ini";return std::clamp<uint32_t>(GetPrivateProfileIntW(L"Wardrobe",L"RefreshDelayMs",500,ini.c_str()),100,10000);}
}
bool initializeNativeWardrobe(std::string& error){
    auto& runtime=Runtime::get();std::lock_guard<std::mutex> lock(mutex);
    engineReady=false;
    nextPipelineProbe=0;
    base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(runtime.profiles.empty()){error="Wardrobe settings folder is unavailable.";detail=error;return false;}
    std::string settingsError;
    selectionsReady=false;
    slotStoreReady=false;
    nextStorageProbe=0;
    slotGeneration.fill(1);
    refreshWardrobeStorage(GetTickCount64());
    if(!initializeWardrobeAPI(runtime.profiles.parent_path()/L"wardrobe-controls.ini",settingsError))LOG_ERR("%s",settingsError.c_str());
    const auto ini=runtime.folder/L"HexAppeal.ini";
    writesAllowed=GetPrivateProfileIntW(L"Development",L"EnableWardrobeWrites",1,ini.c_str())!=0;
    traceBarberOutfit=GetPrivateProfileIntW(L"Development",L"TraceBarberOutfit",0,ini.c_str())!=0;
    configureSlots(ini);
    std::string issue;
    engineReady=checkApplyAppearance(issue)&&checkOutfitCleanup(issue);
    if(writesAllowed)nextPipelineProbe=GetTickCount64()+1000;
    detail=!writesAllowed?"HexAppeal.ini has [Development] EnableWardrobeWrites=0.":
        !engineReady?issue:!slotTransportReady?slotTransportIssue:!slotStoreReady?slotStoreIssue:
        "Native Wardrobe ready. Save equipped parts or choose a supported outfit.";
    const bool visibilityReady=underwearStatus(-1).ready;
    if(!engineReady&&!visibilityReady&&!nextPipelineProbe){error=detail;return false;}
    armed=selections.snapshot().generations;
    LOG("Saved Wardrobe choices queued; application waits for live witch ownership, outfit records and dependencies.");
    reportWardrobeReadiness();
    return true;
}
void reportWardrobeUnavailable(const std::string& error){
    std::lock_guard<std::mutex> lock(mutex);engineReady=false;nextPipelineProbe=0;detail=error;
    Runtime::get().reportCompatibility(Feature::Wardrobe,CompatibilityState::Blocked,error);
    const auto& profiles=Runtime::get().profiles;std::string ignored;
    if(!profiles.empty())selections.load(profiles.parent_path()/L"wardrobe.ini",ignored);
}
void tickNativeWardrobePipeline(){
    std::lock_guard<std::mutex> lock(mutex);
    refreshApplyAppearance(false);
}
void observeNativeWardrobeInteraction(uintptr_t actor,int witch) noexcept {
    if(witch<0||witch>=int(wornOutfits.size())||ticking)return;
    try{
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock||!writesAllowed)return;
        WardrobeSnapshot snapshot;
        const char* issue=nullptr;
        if(!captureObservedWitch(actor,witch,snapshot,&issue)){
            witchDetail[witch]=issue;
            return;
        }
        const auto now=GetTickCount64();
        refreshSpouseWardrobe(now,true);
        const auto current=wornSlots(snapshot,slotStore.profile(witch));
        rememberWorn(snapshot,current,now,now,true);
    }catch(...){LOG_ERR("Wardrobe could not capture the outfit for the targeted native interaction.");}
}
void tickNativeWardrobe(uintptr_t actor,int witch,uint64_t observedAt){
    if(witch<0||witch>5||ticking)return;
    struct TickGuard {
        TickGuard(){ticking=true;}
        ~TickGuard(){ticking=false;}
    } guard;
    const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);
    if(!writesAllowed||armed[witch]!=selected.generations[witch])return;
    const bool switching=engineReady;
    const auto choiceId=wardrobeOutfitId(selected.choices[witch]);bool isDefault=choiceId=="hexappeal/default";
    auto& status=witchDetail[witch];
    WardrobeSnapshot snapshot;
    const char* issue=nullptr;
    if(!captureObservedWitch(actor,witch,snapshot,&issue)){
        status=issue;
        return;
    }
    std::string desired,providerError;
    WardrobeProviderOutfit provider;
    const bool providerReady=isDefault||(switching&&resolveWardrobeProvider(choiceId,witch,provider,providerError));
    if(providerReady&&!isDefault)desired=provider.prefab;
    const auto has=[&](const std::string& name){for(uint32_t i=0;i<snapshot.claimCount;++i)if(name==snapshot.claims[i].prefab)return true;return false;};
    const auto now=GetTickCount64();
    if(owned.find(actor)==owned.end()&&owned.size()>=64){
        unsigned discarded=0;
        for(auto other=owned.begin();other!=owned.end();){
            auto& candidate=other->second;
            const bool hasOutfitState=candidate.installed||candidate.pending||candidate.unacknowledged||!candidate.hiddenParts.empty();
            const WardrobeInstance instance{other->first,candidate.control,candidate.renderer,candidate.node,candidate.witch};
            if(!canDiscardWardrobeInstance(instance,base,now-candidate.lastSeen,hasOutfitState)){
                ++other;
                continue;
            }
            other=owned.erase(other);
            ++discarded;
        }
        if(discarded)LOG("Released %u inactive Wardrobe entries.",discarded);
        if(owned.size()>=64){status="Waiting for an outfit instance to become available.";return;}
    }
    auto& entry=owned[actor];
    if(entry.node!=snapshot.node||entry.control!=snapshot.control||entry.renderer!=snapshot.renderer||entry.witch!=witch){
        entry={};entry.node=snapshot.node;entry.control=snapshot.control;entry.renderer=snapshot.renderer;entry.witch=witch;entry.observedAt=now;appliedGeneration[witch]=0;
    }
    if(entry.lastSeen&&!wardrobeActorRecentlySeen(observedAt,entry.lastSeen)){
        entry.slotAttempt=0;entry.slotRequest=0;entry.attempted=0;
        if((entry.pending||entry.unacknowledged)&&!has(entry.requested)&&has(entry.removed)){
            entry.pending=false;entry.unacknowledged=false;
        }
        LOG("Wardrobe rechecking profile after witch=%d returned to view.",witch);
    }
    entry.lastSeen=observedAt;
    refreshSpouseWardrobe(now);
    const bool nudeRequested=(Runtime::get().outfitHiddenMask.load()&(1u<<witch))!=0;
    if(spouseState.combatEditorOpen&&spouseState.combatEditorNpc&&
       spouseState.combatEditorNpc==wornOutfits[witch].spouseNpc&&snapshot.actor==wornOutfits[witch].actor){
        applyCombatVisibility(snapshot,entry,slotStore.profile(witch),false,true,now);
        status="Outfit / Combat Gear preview";
        return;
    }
    const auto& observed=wornOutfits[witch];
    if(observed.actor==actor&&observed.spouseNpc&&
       (spouseState.barberOpen||now<spouseEditorSettlesAt)){
        if(spouseState.editorOpen()&&status!="Outfit / waiting for Spouse Overhaul editor")
            LOG("Wardrobe witch=%d waiting for Spouse Overhaul editor (barber=%d, combat=%d, npc=%u).",
                witch,spouseState.barberOpen,spouseState.combatEditorOpen,spouseState.combatEditorNpc);
        status="Outfit / waiting for Spouse Overhaul editor";
        return;
    }
    defaultSlots(snapshot);
    if(traceBarberOutfit)observeBarberOutfit(snapshot,entry);
    if(GetTickCount64()-entry.observedAt<250){status="Outfit / waiting for witch";return;}
    const auto current=wornSlots(snapshot,slotStore.profile(witch));
    const auto primary=rememberWorn(snapshot,current,now,observedAt);
    const auto& slotProfile=slotStore.profile(witch);
    if(!primary){
        if(!entry.hiddenParts.empty())applySlotVisibility(snapshot,entry,slotProfile,false,false);
        return;
    }
    if(settlingSpouseGear(wornOutfits[witch].spouseNpc,now)){
        status="Outfit / waiting for Spouse Overhaul gear";
        return;
    }
    if(spouseState.usesCombatLook(wornOutfits[witch].spouseNpc)){
        applyCombatVisibility(snapshot,entry,slotProfile,nudeRequested,false,now);
        status="Outfit / following Spouse Overhaul Combat Gear";
        return;
    }
    entry.combatFallbacks.clear();
    entry.combatFallbackChanged=0;
    auto& edit=savedEdits[witch];
    const bool nativeSlots=edit.open||edit.restoring||(!entry.installed&&(isDefault||!providerReady));
    const bool slotProfileActive=switching&&nativeSlots&&!entry.pending&&!entry.unacknowledged&&slotStoreReady;
    const auto* saved=slotProfileActive?(edit.open?&edit.draft:edit.restoring?&edit.restore:
        slotProfile.selected==SlotOutfit::Saved&&slotProfile.hasSaved?&slotProfile.saved:nullptr):nullptr;
    WardrobeItemLook itemLook;
    if(saved&&!edit.restoring){
        const bool hideUnderwear=(baseUnderwearPreference().hiddenMask&(1u<<witch))!=0;
        itemLook=resolveWardrobeItems(*saved,edit.open?edit.options:slotProfile.itemOptions,nudeRequested,hideUnderwear);
        OutfitParts variants;
        for(const auto& [slot,parts]:itemLook.slots)if(parts!=saved->at(slot))
            variants.insert(parts.begin(),parts.end());
        if(variants!=entry.itemVariants||(!entry.missingItemVariants.empty()&&now-entry.itemVariantsChecked>=1500)){
            const auto registered=registeredParts(variants);
            entry.itemVariants=variants;
            entry.missingItemVariants.clear();
            for(const auto& part:variants)if(!registered.count(part))entry.missingItemVariants.insert(part);
            entry.itemVariantsChecked=now;
        }
        for(auto& [slot,parts]:itemLook.slots){
            const auto missing=std::find_if(parts.begin(),parts.end(),[&](const auto& part){return entry.missingItemVariants.count(part)!=0;});
            if(missing==parts.end())continue;
            const auto& anchor=*saved->at(slot).begin();
            if(itemLook.hidden.erase(*missing))itemLook.hidden.insert(anchor);
            if(itemLook.underwear.erase(*missing))itemLook.underwear.insert(anchor);
            parts=saved->at(slot);
        }
        saved=&itemLook.slots;
    }
    if(saved&&*saved!=entry.itemTarget){
        entry.itemTarget=*saved;
        ++slotGeneration[witch];
    }else if(!saved)entry.itemTarget.clear();
    const auto allowed=saved?outfitSlotParts(*saved):OutfitParts{};
    const bool emptyProfile=saved&&allowed.empty();
    const bool savedReady=saved&&outfitPartsPresent(outfitSlotParts(current),allowed);
    const bool masked=applySlotVisibility(snapshot,entry,slotProfile,nudeRequested,true,savedReady?&allowed:nullptr,
        providerReady,savedReady?&itemLook.hidden:nullptr,savedReady?&itemLook.underwear:nullptr);
    confirmOutfitHidden(snapshot,nudeRequested,masked,emptyProfile);
    if(!switching)return;
    if(slotProfileActive){
        if(emptyProfile){
            if(masked&&edit.restoring){edit.restore.clear();edit.restoring=false;}
            status=masked?"Outfit / active (empty)":"Outfit / waiting to hide inactive clothing";
            return;
        }
        tickSlotLook(snapshot,entry,current,slotProfile,saved,savedReady&&masked,status);
        if(saved&&!entry.missingItemVariants.empty()&&status=="Outfit / active")
            status="Outfit / an item variant is unavailable. Using the equipped item.";
        if(savedReady&&!masked&&!itemLook.hidden.empty()&&!nudeRequested)
            status="Equipped item / waiting for independent visibility";
        if(nudeRequested&&!masked)status="Hide outfit / waiting for visible slots";
        return;
    }
    if(entry.refresh.pending()&&entry.refreshOutfit==choiceId)entry.refresh.generation=selected.generations[witch];else entry.refresh.sync(selected.generations[witch]);
    if(entry.pending||entry.unacknowledged){
        const bool observed=has(entry.requested)&&!has(entry.removed);
        if(observed){entry.pending=false;entry.unacknowledged=false;entry.installed=entry.requested!=entry.original;entry.current=entry.installed?entry.requested:"";const bool transition=entry.refresh.acknowledged(entry.installed,GetTickCount64());appliedGeneration[witch]=entry.refresh.pending()?0:entry.attempted;if(transition)entry.attempted=0;status=entry.refresh.pending()?"Outfit / refreshing":"Outfit / active";if(!entry.refresh.pending()){Runtime::get().requestShapeReapply(witch);LOG("Wardrobe final assembly acknowledged; saved shape refit queued for witch=%d.",witch);}LOG("Wardrobe switch acknowledged witch=%d actor=%p node=%p prefab=%s.",witch,reinterpret_cast<void*>(actor),reinterpret_cast<void*>(snapshot.node),entry.requested.c_str());}
        else if(entry.pending&&int32_t(GetTickCount()-entry.deadline)>=0){entry.pending=false;entry.unacknowledged=true;status="Outfit / waiting for a stable assembly";LOG("Wardrobe switch unconfirmed witch=%d.",witch);}
        if(entry.unacknowledged&&has(entry.removed)&&!has(entry.requested)){
            entry.unacknowledged=false;
            entry.attempted=0;
            entry.appearanceRetryAt=now+2000;
        }
        return;
    }
    if(!providerReady){
        appliedGeneration[witch]=0;
        if(status!=providerError)LOG("Wardrobe witch=%d provider %s unavailable: %s.",witch,choiceId.c_str(),providerError.c_str());
        status=providerError;
        return;
    }
    if(entry.cleanupFailed){status="Outfit / waiting for a new render instance after record cleanup failed";return;}
    if(now<entry.appearanceRetryAt)return;
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
    WardrobeSnapshot fresh;if(!captureObservedWitch(actor,witch,fresh)||fresh.node!=snapshot.node||fresh.claimData!=snapshot.claimData||fresh.claimCount!=snapshot.claimCount)return;
    bool sourceStillPresent=false;for(uint32_t i=0;i<fresh.claimCount;++i){const auto& claim=fresh.claims[i];sourceStillPresent|=sourceName==claim.prefab;if(std::strstr(claim.prefab,"_ub_")&&claim.kind!=WardrobePartKind::DefaultGarment&&(!entry.installed||entry.current!=claim.prefab))return;}
    if(!sourceStillPresent)return;
    if(!restoreOutfitVisibility(reinterpret_cast<void*>(actor))){status="Outfit / waiting to restore garment visibility";return;}
    entry.attempted=selected.generations[witch];
    if(!refreshApplyAppearance(true)){status=detail;return;}
    entry.refresh.applyingSelected();
    if(entry.refresh.phase==WardrobeRefreshCycle::Selected)LOG("Wardrobe refresh restore witch=%d delay_ms=%u observed_wait_ms=%llu.",witch,entry.refresh.delayMs,static_cast<unsigned long long>(GetTickCount64()-(entry.refresh.restoreAt-entry.refresh.delayMs)));
    const auto result=invokeWardrobeSwap(snapshot.node,record,addWrapper,base,{retain,release,destroy,pipeline});
    LOG("Wardrobe switch call witch=%d actor=%p node=%p source=%s target=%s result=%d thread=%u.",witch,reinterpret_cast<void*>(actor),reinterpret_cast<void*>(snapshot.node),source,target,int(result),GetCurrentThreadId());
    if(result==WardrobeCallResult::CleanupFailed){
        entry.cleanupFailed=true;
        status="Outfit / waiting for a new render instance after record cleanup failed";
        return;
    }
    if(result!=WardrobeCallResult::Completed&&result!=WardrobeCallResult::CallFailed){
        entry.attempted=0;entry.appearanceRetryAt=now+2000;
        status="Outfit / retrying with fresh equipped records";
        return;
    }
    entry.original=original;entry.removed=sourceName;entry.requested=targetName;
    entry.pending=true;entry.deadline=GetTickCount()+5000;status="Saved / waiting for outfit assembly";
}
WardrobeEditorStatus wardrobeEditorStatus(int target){
    if(target>=firstAddonCharacter){
        auto addon=addonWardrobeStatus(target);
        WardrobeEditorStatus result;
        result.available.switchReady=addon.canSwitch;
        result.caption=addon.detail;result.detail=std::move(addon.detail);
        result.warning=!addon.canSwitch;
        result.slotSelection=addon.selected;result.savedOutfitAvailable=addon.hasSaved;
        result.visibleSlots=addon.actorSeen&&!addon.slots.empty();result.slots=std::move(addon.slots);
        result.savedSlots=std::move(addon.savedSlots);result.draftSlots=std::move(addon.draftSlots);
        result.hiddenSlots=std::move(addon.hiddenSlots);result.editingSaved=addon.editing;
        return result;
    }
    const auto selected=selections.snapshot();std::lock_guard<std::mutex> lock(mutex);WardrobeEditorStatus result;result.available=capabilities();result.detail=detail;
    if(target< -1||target>5){result.available={};return result;}result.selected=selected.choices[target<0?0:target];
    result.slotSelection=slotStore.profile(target<0?0:target).selected;
    if(target<0)for(size_t i=0;i<selected.choices.size();++i){
        const auto id=wardrobeOutfitId(selected.choices[i]);
        result.mixedOutfit|=id!=wardrobeOutfitId(result.selected)||
            (id=="hexappeal/default"&&slotStore.profile(int(i)).selected!=result.slotSelection);
    }
    if(target>=0){
        const auto& profile=slotStore.profile(target);
        const auto& worn=wornOutfits[target];
        const bool seen=wardrobeActorRecentlySeen(GetTickCount64(),worn.seen);
        result.visibleSlots=seen&&!worn.slots.empty();
        if(result.visibleSlots)result.slots=worn.slots;
        result.savedSlots=profile.saved;
        result.editingSaved=savedEdits[target].open;
        if(result.editingSaved)result.draftSlots=savedEdits[target].draft;
        result.hiddenSlots=profile.hidden;
        if(result.slotSelection==SlotOutfit::Game&&result.visibleSlots&&!profile.barber.empty()&&
           outfitSlotParts(worn.slots)==outfitSlotParts(profile.barber))result.slotSelection=SlotOutfit::Barber;
        result.barberOutfitAvailable=slotStoreReady&&!profile.barber.empty();
        result.spouseCombatActive=seen&&spouseState.usesCombatLook(worn.spouseNpc);
        result.savedOutfitAvailable=slotStoreReady&&profile.hasSaved;
    }
    if(result.available.switchReady&&slotTransportReady&&slotStoreReady){
        result.warning=false;
        if(target>=0){
            result.detail=armed[target]!=selected.generations[target]?"Outfit / awaiting selection":
                witchDetail[target].empty()?"Outfit / waiting for witch":witchDetail[target];
        }else{
            unsigned active=0;for(size_t i=0;i<6;++i)if(armed[i]==selected.generations[i]&&appliedGeneration[i]==selected.generations[i])++active;
            result.detail="Outfits / "+std::to_string(active)+" active, "+std::to_string(6-active)+" waiting";
        }
    }
    result.caption=result.detail;
    if(result.selected.outfit==WitchOutfit::Legacy&&result.available.switchReady){
        result.caption="Saved legacy outfit missing. Native profiles are available.";
        result.warning=true;
    }
    for(int witch=0;witch<6;++witch)if((target<0||target==witch)&&!choiceErrors[witch].empty()){
        result.caption="Selection not saved";result.detail=choiceErrors[witch];result.warning=true;break;
    }
    return result;
}
static bool requestSlotOutfitLocked(int witch,SlotOutfit outfit,std::string& error){
    if(!slotStoreReady){error=slotStoreIssue;return false;}
    std::array<SlotOutfit,6> previous{};
    for(int i=0;i<int(previous.size());++i)previous[i]=slotStore.profile(i).selected;
    if(!slotStore.select(witch,outfit,error))return false;
    if(!selections.change(witch,WardrobeField::Outfit,0,error)){
        std::string ignored;
        for(int i=0;i<int(previous.size());++i)
            if((witch<0||witch==i)&&!slotStore.select(i,previous[i],ignored))LOG_ERR("%s",ignored.c_str());
        return false;
    }
    const auto selected=selections.snapshot();
    for(int i=0;i<int(previous.size());++i)if(witch<0||witch==i){
        armed[i]=selected.generations[i];
        choiceErrors[i].clear();
        savedEdits[i].restore.clear();
        savedEdits[i].restoring=false;
        ++slotGeneration[i];
    }
    return true;
}
bool requestSlotOutfit(int witch,SlotOutfit outfit,std::string& error){
    if(witch>=firstAddonCharacter)return selectAddonOutfit(witch,outfit,error);
    if(witch< -1||witch>=int(wornOutfits.size())||(witch<0&&outfit!=SlotOutfit::Game&&outfit!=SlotOutfit::Default)){
        error="Choose one character for this profile.";return false;
    }
    std::lock_guard<std::mutex> lock(mutex);
    return requestSlotOutfitLocked(witch,outfit,error);
}
bool openSavedWardrobe(int witch,std::string& error){
    if(witch>=firstAddonCharacter)return openAddonWardrobe(witch,error);
    std::lock_guard<std::mutex> lock(mutex);
    if(witch<0||witch>=int(wornOutfits.size())){error="Choose one character.";return false;}
    auto& edit=savedEdits[witch];
    if(edit.open)return true;
    if(!slotStoreReady){error=slotStoreIssue;return false;}
    if(!capabilities().switchReady){error=detail;return false;}
    if(!slotTransportReady){error=slotTransportIssue;return false;}
    const auto& worn=wornOutfits[witch];
    if(!wardrobeActorRecentlySeen(GetTickCount64(),worn.seen)){
        error=witchDetail[witch].empty()?"Bring the character into view so Wardrobe can capture the outfit.":witchDetail[witch];
        return false;
    }
    if(spouseState.usesCombatLook(worn.spouseNpc)){
        error="Spouse Overhaul controls Combat Gear.";return false;
    }
    const auto& profile=slotStore.profile(witch);
    edit={true,false,worn.slots,profile.hasSaved?profile.saved:worn.slots,{},profile.itemOptions};
    ++slotGeneration[witch];
    witchDetail[witch]="Saved Wardrobe / preview";
    return true;
}
bool applySavedWardrobe(int witch,std::string& error){
    if(witch>=firstAddonCharacter)return saveAddonWardrobe(witch,error);
    std::lock_guard<std::mutex> lock(mutex);
    if(witch<0||witch>=int(savedEdits.size())||!savedEdits[witch].open){
        error="Open Saved Wardrobe to apply changes.";return false;
    }
    auto& edit=savedEdits[witch];
    const auto before=selections.snapshot().choices[witch];
    const bool provider=before.outfit==WitchOutfit::Provider;
    if(provider&&!selections.change(witch,WardrobeField::Outfit,0,error))return false;
    if(!slotStore.save(witch,SlotOutfit::Saved,edit.draft,error,true,&edit.options)){
        if(provider){
            std::string restoreError;
            if(!selections.providerOutfit(witch,before.provider,restoreError))
                error+=" Could not restore the previous provider selection: "+restoreError;
            armed[witch]=selections.snapshot().generations[witch];
        }
        return false;
    }
    if(provider)armed[witch]=selections.snapshot().generations[witch];
    edit={};
    ++slotGeneration[witch];
    return true;
}
bool clearSavedWardrobeDraft(int witch,std::string& error){
    if(witch>=firstAddonCharacter)return clearAddonWardrobe(witch,error);
    std::lock_guard<std::mutex> lock(mutex);
    if(witch<0||witch>=int(savedEdits.size())||!savedEdits[witch].open){
        error="Open Saved Wardrobe to clear its slots.";return false;
    }
    savedEdits[witch].draft.clear();
    savedEdits[witch].options.clear();
    ++slotGeneration[witch];
    return true;
}
void cancelSavedWardrobe(int witch){
    if(witch>=firstAddonCharacter){cancelAddonWardrobe(witch);return;}
    std::lock_guard<std::mutex> lock(mutex);
    if(witch<0||witch>=int(savedEdits.size())||!savedEdits[witch].open)return;
    auto& edit=savedEdits[witch];
    if(slotStore.profile(witch).selected==SlotOutfit::Game){
        edit.restore=std::move(edit.before);
        edit.restoring=true;
    }
    edit.open=false;
    edit.before.clear();
    edit.draft.clear();
    edit.options.clear();
    ++slotGeneration[witch];
}
bool equipWardrobeItem(int witch,const std::string& slot,const OutfitParts& parts,std::string& error){
    if(witch>=firstAddonCharacter)return equipAddonItem(witch,slot,parts,error);
    std::lock_guard<std::mutex> lock(mutex);
    if(witch<0||witch>=int(wornOutfits.size())){error="Choose one character.";return false;}
    if(!slotStoreReady){error=slotStoreIssue;return false;}
    if(!capabilities().switchReady){error=detail;return false;}
    if(!slotTransportReady){error=slotTransportIssue;return false;}
    if(!validOutfitSlot(slot)||parts.empty()||std::any_of(parts.begin(),parts.end(),[&](const auto& part){
        return !validOutfitPart(part)||protectedOutfitPart(part)||outfitSlotForPart(part)!=slot;
    })){error="This item has no checked appearance for that slot.";return false;}
    if(registeredParts(parts).size()!=parts.size()){
        error="This item's appearance is unavailable in the current game mount.";return false;
    }
    auto& edit=savedEdits[witch];
    if(!edit.open){error="Open [HA] Wardrobe to equip items.";return false;}
    auto outfit=edit.draft;
    for(auto it=outfit.begin();it!=outfit.end();){
        auto& previous=it->second;
        for(auto part=previous.begin();part!=previous.end();){
            if(it->first==slot||outfitSlotForPart(*part)==slot)part=previous.erase(part);else++part;
        }
        if(previous.empty())it=outfit.erase(it);else++it;
    }
    outfit[slot]=parts;
    if(outfitSlotParts(outfit).size()>24){error="This outfit has too many equipped parts.";return false;}
    edit.draft=std::move(outfit);
    ++slotGeneration[witch];
    witchDetail[witch]="Outfit / waiting for assembly";
    return true;
}
bool setWardrobeSlotHidden(int witch,const std::string& slot,bool hidden,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    if(const auto* addon=characterAddon(witch);addon&&characterAddonEnabled(witch)){
        auto& session=addonSession(witch,*addon);
        if(!session.loaded){error=session.storageIssue;return false;}
        return session.store.setHidden(0,slot,hidden,error);
    }
    if(!slotStoreReady){error=slotStoreIssue;return false;}
    return slotStore.setHidden(witch,slot,hidden,error);
}
bool unequipWardrobeSlot(int target,const std::string& slot,std::string& error){
    if(target>=firstAddonCharacter)return clearAddonSlot(target,slot,error);
    std::lock_guard<std::mutex> lock(mutex);
    if(target<0||target>=int(savedEdits.size())||!validOutfitSlot(slot)||!savedEdits[target].open){
        error="Open [HA] Wardrobe to unequip items.";
        return false;
    }
    if(savedEdits[target].draft.erase(slot)){
        ++slotGeneration[target];
        witchDetail[target]="Outfit / waiting for assembly";
    }
    return true;
}
std::vector<WardrobeItemControl> equippedWardrobeItemControls(int witch,bool preview){
    std::lock_guard<std::mutex> lock(mutex);
    if(const auto* addon=characterAddon(witch);addon&&characterAddonEnabled(witch)){
        const auto& session=addonSession(witch,*addon);
        const auto& profile=session.store.profile(0);
        if(!session.loaded||preview!=session.edit.open||(!preview&&profile.selected!=SlotOutfit::Saved))return {};
        return wardrobeItemControls(preview?session.edit.draft:profile.saved,preview?session.edit.options:profile.itemOptions);
    }
    if(witch<0||witch>=int(savedEdits.size())||!slotStoreReady)return {};
    const auto& edit=savedEdits[witch];
    const auto& profile=slotStore.profile(witch);
    if(preview)return edit.open?wardrobeItemControls(edit.draft,edit.options):std::vector<WardrobeItemControl>{};
    if(edit.open||profile.selected!=SlotOutfit::Saved||
       wardrobeOutfitId(selections.snapshot().choices[witch])!="hexappeal/default")return {};
    return wardrobeItemControls(profile.saved,profile.itemOptions);
}
bool setEquippedWardrobeItemControl(int witch,bool preview,const std::string& outfit,const std::string& control,uint32_t value,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    const auto* addon=characterAddon(witch);
    if(addon?!characterAddonEnabled(witch)||!engineReady:
        witch<0||witch>=int(savedEdits.size())||!slotStoreReady||!capabilities().switchReady){
        error="Saved Wardrobe is unavailable.";return false;
    }
    auto* session=addon?&addonSession(witch,*addon):nullptr;
    auto& edit=session?session->edit:savedEdits[witch];
    auto& store=session?session->store:slotStore;
    const int index=session?0:witch;
    const auto& profile=store.profile(index);
    if(session&&!session->loaded){error=session->status;return false;}
    if(preview!=edit.open||(!preview&&(profile.selected!=SlotOutfit::Saved||
       (!session&&wardrobeOutfitId(selections.snapshot().choices[witch])!="hexappeal/default")))){
        error="Select the equipped item in [HA] Wardrobe first.";return false;
    }
    const auto controls=wardrobeItemControls(preview?edit.draft:profile.saved,preview?edit.options:profile.itemOptions);
    const auto match=std::find_if(controls.begin(),controls.end(),[&](const auto& row){return row.outfit==outfit&&row.control.id==control;});
    if(match==controls.end()||!match->available||value>=(match->control.type==HA_TOGGLE?2:match->control.options.size())){
        error="This equipped item option is unavailable.";return false;
    }
    if(value==match->value)return true;
    auto options=preview?edit.options:profile.itemOptions;
    if(value==match->control.defaultValue)options.erase({outfit,control});
    else options[{outfit,control}]=match->control.type==HA_TOGGLE?std::to_string(value):match->control.options[value].id;
    if(preview)edit.options=std::move(options);
    else if(!store.save(index,SlotOutfit::Saved,profile.saved,error,false,&options))return false;
    if(session)++session->generation;
    else ++slotGeneration[witch];
    return true;
}

namespace {
bool addonSupportsPart(const CharacterAddon& addon,const std::string& slot,const std::string& part){
    return validOutfitPart(part)&&!protectedOutfitPart(part)&&
        (outfitSlotForPart(part)==slot||std::any_of(addon.items.begin(),addon.items.end(),[&](const auto& item){
            return item.prefab==part&&item.slot==slot;
        }));
}
bool addonSupportsSlots(const CharacterAddon& addon,const OutfitSlots& slots){
    for(const auto& [slot,parts]:slots)
        for(const auto& part:parts)
            if(!addonSupportsPart(addon,slot,part))return false;
    return true;
}
bool addonCanEdit(const AddonSession& session){
    return session.loaded&&session.checked&&writesAllowed&&engineReady&&slotTransportReady&&
        wardrobeActorRecentlySeen(GetTickCount64(),session.seen);
}
bool addonCanEdit(const AddonSession& session,std::string& error){
    if(addonCanEdit(session))return true;
    if(!session.loaded)error=session.storageIssue;
    else if(!writesAllowed||!engineReady)error=detail;
    else if(!slotTransportReady)error=slotTransportIssue;
    else if(!wardrobeActorRecentlySeen(GetTickCount64(),session.seen))error="The live appearance has not been observed. Bring the character into view.";
    else error=session.status.empty()?"The current appearance has no usable equipment slots.":session.status;
    return false;
}
bool equipAddonParts(AddonSession& session,const std::string& slot,const OutfitParts& parts,std::string& error){
    if(!addonCanEdit(session,error)||!session.edit.open){if(error.empty())error="Open [HA] Wardrobe to equip items.";return false;}
    if(registeredParts(parts).size()!=parts.size()){error="This item is missing from the active game mount.";return false;}
    auto next=session.edit.draft;
    next[slot]=parts;
    if(outfitSlotParts(next).size()>24){error="This outfit has too many parts.";return false;}
    session.edit.draft=std::move(next);
    ++session.generation;
    return true;
}
bool addonBaseUnderwear(const WardrobeSnapshot& snapshot,AddonSession& session,bool exposed,bool hide,bool enabled){
    if((!session.underwearPresent||!exposed)&&session.shownBase.empty()){
        session.externalUnderwear=false;
        return hide&&session.underwearPresent;
    }
    const bool externalAllowed=Runtime::get().allowExternalUnderwear.load();
    const bool forceShow=enabled&&exposed&&!hide&&!externalAllowed;
    WardrobeSnapshot fresh;
    if(!sameOutfitSnapshot(snapshot,fresh))return true;
    auto& shown=session.shownBase;
    discardReplacedVisibilityClaims(fresh,shown);
    const auto protectedKey=[&](uint32_t key){
        return std::any_of(fresh.claims.begin(),fresh.claims.begin()+fresh.claimCount,[&](const auto& part){
            return part.key==key&&part.kind!=WardrobePartKind::Underwear;
        });
    };
    if(!forceShow){
        for(auto it=shown.begin();it!=shown.end();){
            if(protectedKey(it->key)||visibilityConfirmed(fresh.renderer,it->key,false))it=shown.erase(it);
            else ++it;
        }
    }
    bool rules=false;
    for(uint32_t i=0;enabled&&i<fresh.claimCount;++i){
        const auto& claim=fresh.claims[i];
        if(claim.kind!=WardrobePartKind::Underwear||protectedKey(claim.key))continue;
        const auto token=wardrobeVisibilityClaim(fresh,claim);
        const auto ownsHide=std::find(session.live.hiddenParts.begin(),session.live.hiddenParts.end(),token)!=session.live.hiddenParts.end();
        bool visible=false;
        if(!readOutfitPartVisibility(fresh.renderer,claim.key,visible))continue;
        if(exposed&&!visible&&!ownsHide){rules=true;
            if(forceShow&&visibilityConfirmed(fresh.renderer,claim.key,true)&&std::find(shown.begin(),shown.end(),token)==shown.end())
                shown.push_back(token);
        }
    }
    session.externalUnderwear=enabled&&externalAllowed&&rules;
    return hide&&!session.externalUnderwear;
}
}

void tickAddonWardrobe(uintptr_t actor,int target,uint64_t observedAt){
    const auto* addon=characterAddon(target);
    if(!addon||ticking)return;
    struct Guard {Guard(){ticking=true;}~Guard(){ticking=false;}} guard;
    std::lock_guard<std::mutex> lock(mutex);
    if(!writesAllowed)return;
    const bool enabled=characterAddonEnabled(target);
    if(!enabled&&addonSessions.find(target)==addonSessions.end())return;
    auto& session=addonSession(target,*addon);
    if(!session.loaded)return;
    const auto now=GetTickCount64();
    if(session.actor!=actor&&wardrobeActorRecentlySeen(now,session.seen))return;
    WardrobeSnapshot snapshot;
    const char* issue=nullptr;
    if(!captureWardrobeSnapshot(actor,target,base,snapshot,addon->appearance.c_str(),&issue)){
        session.checked=false;
        session.status=issue;
        return;
    }
    auto& live=session.live;
    if(session.actor!=actor||live.node!=snapshot.node||live.renderer!=snapshot.renderer||live.control!=snapshot.control){
        if(session.actor!=actor){
            if(session.edit.open||session.edit.restoring){session.edit={};++session.generation;}
            session.followKnown=false;
            session.follow.clear();
        }
        live={};
        session.shownBase.clear();
        session.externalUnderwear=false;
        live.node=snapshot.node;
        live.renderer=snapshot.renderer;
        live.control=snapshot.control;
        live.witch=target;
        live.observedAt=now;
        session.actor=actor;
    }
    session.seen=observedAt;
    if(now-live.observedAt<250){session.checked=false;session.status="Waiting for the outfit to load.";return;}
    const auto& profile=session.store.profile(0);
    session.worn=wornSlots(snapshot,profile);
    const bool commonUnderwear=addonVenusBodyActive(target);
    session.underwearPresent=commonUnderwear?addonVenusUnderwearPresent(target):std::any_of(snapshot.claims.begin(),snapshot.claims.begin()+snapshot.claimCount,
        [](const auto& claim){return claim.kind==WardrobePartKind::Underwear;});
    session.checked=enabled&&addonSupportsSlots(*addon,session.worn);
    auto& status=session.status;
    auto& edit=session.edit;
    const auto current=outfitSlotParts(session.worn);
    if(session.checked&&!edit.open&&!edit.restoring&&(!session.followKnown||profile.selected==SlotOutfit::Game)){
        session.follow=session.worn;
        session.followKnown=true;
    }
    const bool hideOutfit=enabled&&profile.hideOutfit;
    const bool hideUnderwear=enabled&&addonUnderwearHidden(target)&&(!commonUnderwear||!underwearOwnership(target).delegated);
    const bool expose=hideOutfit||(profile.selected==SlotOutfit::Saved&&profile.hasSaved&&profile.saved.empty());
    const bool baseHidden=!commonUnderwear&&addonBaseUnderwear(snapshot,session,expose,hideUnderwear,enabled);
    const auto* saved=edit.open?&edit.draft:!edit.restoring&&profile.selected==SlotOutfit::Saved&&profile.hasSaved?&profile.saved:nullptr;
    WardrobeItemLook itemLook;
    if(enabled&&saved){
        itemLook=resolveWardrobeItems(*saved,edit.open?edit.options:profile.itemOptions,hideOutfit,hideUnderwear);
        OutfitParts variants;
        for(const auto& [slot,parts]:itemLook.slots)if(parts!=saved->at(slot))variants.insert(parts.begin(),parts.end());
        if(variants!=live.itemVariants||(!live.missingItemVariants.empty()&&now-live.itemVariantsChecked>=1500)){
            const auto available=registeredParts(variants);
            live.itemVariants=variants;
            live.missingItemVariants.clear();
            for(const auto& part:variants)if(!available.count(part))live.missingItemVariants.insert(part);
            live.itemVariantsChecked=now;
        }
        for(auto& [slot,parts]:itemLook.slots){
            if(parts==saved->at(slot))continue;
            const auto missing=std::find_if(parts.begin(),parts.end(),[&](const auto& part){
                return live.missingItemVariants.count(part)||protectedOutfitPart(part)||
                    (!outfitSlotForPart(part).empty()&&outfitSlotForPart(part)!=slot);
            });
            if(missing==parts.end())continue;
            if(itemLook.hidden.erase(*missing))itemLook.hidden.insert(*saved->at(slot).begin());
            if(itemLook.underwear.erase(*missing))itemLook.underwear.insert(*saved->at(slot).begin());
            parts=saved->at(slot);
        }
    }
    const auto mode=edit.open||edit.restoring?SlotOutfit::Saved:profile.selected;
    const auto* desired=edit.restoring?&edit.restore:mode==SlotOutfit::Default?&addon->defaults:
        saved?&itemLook.slots:nullptr;
    const auto wanted=desired?outfitSlotParts(*desired):OutfitParts{};
    const bool supportedLook=desired&&addonSupportsSlots(*addon,saved?*saved:*desired);
    // The game can reattach inherited clothing. Remove Wardrobe-equipped extras
    // through the slot transport so a cancelled unequip can reassemble them.
    const bool maskExtras=enabled&&saved&&supportedLook&&
        canMaskInheritedOutfitParts(current,wanted,outfitSlotParts(addon->defaults),outfitSlotParts(session.follow));
    OutfitParts hidden=std::move(itemLook.hidden);
    if(maskExtras){
        std::set_difference(current.begin(),current.end(),wanted.begin(),wanted.end(),std::inserter(hidden,hidden.end()));
    }
    if(hideOutfit)for(const auto& part:current)if(!itemLook.underwear.count(part))hidden.insert(part);
    if(enabled)for(const auto& [slot,parts]:session.worn)if(profile.hidden.count(slot))
        hidden.insert(parts.begin(),parts.end());
    const bool visibility=applySlotVisibility(snapshot,live,profile,false,false,nullptr,true,&hidden,&itemLook.underwear,baseHidden,true);
    if(commonUnderwear)
        setOutfitHiddenActor(snapshot.actor,snapshot.renderer,target,enabled&&expose,
            visibility,session.underwearPresent,enabled&&expose&&!hideOutfit);
    if(!session.checked&&!edit.restoring){
        status=enabled?"The current look has an unsupported equipment slot.":"Character patch disabled.";
        return;
    }
    if(mode==SlotOutfit::Game){
        status=visibility?"Following game.":"Waiting for independent outfit visibility.";
        return;
    }
    if(!supportedLook){
        status="The saved outfit references equipment from an earlier version of this add-on.";
        return;
    }
    if(current==wanted||maskExtras){
        live.slotDeadline=0;
        if(edit.restoring&&visibility)edit={};
        live.slotRetry.tryAgain(now,true,true,0);
        status=visibility?"Outfit ready.":"Waiting for independent outfit visibility.";
        return;
    }
    if(!engineReady||!slotTransportReady){
        status=engineReady?slotTransportIssue:detail;
        return;
    }
    if(live.slotRequest!=session.generation){
        live.slotRequest=session.generation;
        live.slotRetry={};
        live.slotRetryAt=0;
        live.slotDeadline=0;
    }
    if(live.slotDeadline&&now<live.slotDeadline&&live.slotTarget==wanted){
        status="Waiting for outfit assembly.";
        return;
    }
    if(live.cleanupFailed){status="Waiting for a new render instance after record cleanup failed.";return;}
    if(now<live.slotRetryAt)return;
    if(!live.missingSavedSlot.empty()&&now>=live.slotRetryAt)live.slotRetry={};
    if(!live.slotRetry.tryAgain(now,false,true,0)){status="The new outfit could not be verified.";return;}
    live.slotRetryAt=now+1000;
    queueSlotLook(snapshot,live,session.worn,*desired,mode,session.generation,addon->defaults,status);
}

AddonWardrobeStatus addonWardrobeStatus(int target){
    std::lock_guard<std::mutex> lock(mutex);
    const auto* addon=characterAddon(target);
    if(!addon)return {};
    const auto& session=addonSession(target,*addon);
    const auto& profile=session.store.profile(0);
    std::string status=session.status;
    if(session.loaded&&!wardrobeActorRecentlySeen(GetTickCount64(),session.seen))
        status="Waiting for "+addon->label+"'s supported appearance.";
    const bool actorSeen=wardrobeActorRecentlySeen(GetTickCount64(),session.seen);
    const bool enabled=characterAddonEnabled(target);
    if(!enabled)status="Character patch disabled.";
    else if(actorSeen&&session.checked&&(!writesAllowed||!engineReady||!slotTransportReady))
        status=!writesAllowed||!engineReady?detail:slotTransportIssue;
    const bool savedAvailable=profile.hasSaved&&addonSupportsSlots(*addon,profile.saved);
    return {std::move(status),actorSeen?session.worn:OutfitSlots{},profile.saved,session.edit.draft,profile.hidden,profile.selected,
            enabled&&addonCanEdit(session),session.edit.open,profile.hideOutfit,savedAvailable,
            enabled&&session.loaded&&writesAllowed&&engineReady&&slotTransportReady,
            actorSeen&&session.underwearPresent,actorSeen,actorSeen&&session.externalUnderwear};
}

bool selectAddonOutfit(int target,SlotOutfit outfit,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    const auto* addon=characterAddon(target);
    if(!addon||!characterAddonEnabled(target)||(outfit!=SlotOutfit::Default&&outfit!=SlotOutfit::Game&&outfit!=SlotOutfit::Saved)){
        error="Choose an installed character and a supported outfit.";return false;
    }
    auto& session=addonSession(target,*addon);
    if(!session.loaded){error=session.storageIssue;return false;}
    if(outfit==SlotOutfit::Saved&&!addonSupportsSlots(*addon,session.store.profile(0).saved)){
        error="Rebuild the saved outfit with equipment supported by this add-on.";return false;
    }
    const auto previous=session.store.profile(0).selected;
    if(!session.store.select(0,outfit,error))return false;
    session.edit={};
    if(outfit==SlotOutfit::Game&&previous!=SlotOutfit::Game&&session.followKnown&&
       wardrobeActorRecentlySeen(GetTickCount64(),session.seen)){
        session.edit.restore=session.follow;
        session.edit.restoring=true;
    }
    ++session.generation;
    return true;
}
bool setAddonOutfitHidden(int target,bool hidden,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    const auto* addon=characterAddon(target);
    if(!addon||!characterAddonEnabled(target)){error="Choose an enabled character.";return false;}
    auto& session=addonSession(target,*addon);
    if(!session.loaded){error=session.storageIssue;return false;}
    return session.store.setOutfitHidden(0,hidden,error);
}
bool openAddonWardrobe(int target,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    const auto* addon=characterAddon(target);
    if(!addon){error="Choose an installed character.";return false;}
    auto& session=addonSession(target,*addon);
    if(!addonCanEdit(session,error))return false;
    if(session.edit.open)return true;
    const auto& profile=session.store.profile(0);
    const bool savedAvailable=profile.hasSaved&&addonSupportsSlots(*addon,profile.saved);
    session.edit={true,false,session.worn,savedAvailable?profile.saved:session.worn,{},
        savedAvailable?profile.itemOptions:WardrobeItemOptions{}};
    ++session.generation;
    return true;
}
bool equipAddonItem(int target,const std::string& slot,const OutfitParts& parts,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    const auto* addon=characterAddon(target);
    if(!addon||!characterAddonEnabled(target)||!validOutfitSlot(slot)||parts.empty()||
       std::any_of(parts.begin(),parts.end(),[&](const auto& part){
           return !addonSupportsPart(*addon,slot,part);
       })){
        error="This item has no checked appearance for that slot.";
        return false;
    }
    return equipAddonParts(addonSession(target,*addon),slot,parts,error);
}
bool clearAddonSlot(int target,const std::string& slot,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    const auto* addon=characterAddon(target);
    if(!addon||!validOutfitSlot(slot)){error="Choose an outfit slot for an installed character.";return false;}
    auto& session=addonSession(target,*addon);
    if(!addonCanEdit(session,error)||!session.edit.open){if(error.empty())error="Open the wardrobe editor first.";return false;}
    if(session.edit.draft.erase(slot))++session.generation;
    return true;
}
bool saveAddonWardrobe(int target,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    const auto* addon=characterAddon(target);
    if(!addon){error="Choose an installed character.";return false;}
    auto& session=addonSession(target,*addon);
    if(!addonCanEdit(session,error)||!session.edit.open){if(error.empty())error="Open the wardrobe editor first.";return false;}
    if(!session.store.save(0,SlotOutfit::Saved,session.edit.draft,error,true,&session.edit.options))return false;
    session.edit={};
    ++session.generation;
    return true;
}
bool clearAddonWardrobe(int target,std::string& error){
    std::lock_guard<std::mutex> lock(mutex);
    const auto* addon=characterAddon(target);
    if(!addon||!characterAddonEnabled(target)){error="Choose an enabled character.";return false;}
    auto& session=addonSession(target,*addon);
    if(!addonCanEdit(session,error)||!session.edit.open){if(error.empty())error="Open the wardrobe editor first.";return false;}
    session.edit.draft.clear();
    session.edit.options.clear();
    ++session.generation;
    return true;
}
void cancelAddonWardrobe(int target){
    std::lock_guard<std::mutex> lock(mutex);
    const auto found=addonSessions.find(target);
    if(found==addonSessions.end()||!found->second.edit.open)return;
    auto& session=found->second;
    const bool restoreGame=session.store.profile(0).selected==SlotOutfit::Game;
    auto previous=std::move(session.edit.before);
    session.edit={};
    if(restoreGame){
        session.edit.restore=std::move(previous);
        session.edit.restoring=true;
    }
    ++session.generation;
}

bool changeWardrobeChoice(int target,WardrobeField field,int value,std::string& error){
    if(!selections.change(target,field,value,error)){
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
    if(!engineReady||!writesAllowed){error=detail;return false;}
    unsigned queued=0;for(auto& row:owned){auto& entry=row.second;if(entry.witch<0||entry.witch>5||(witch>=0&&entry.witch!=witch)||wardrobeOutfitId(selected.choices[entry.witch])!=id||(!entry.installed&&entry.refresh.phase==WardrobeRefreshCycle::Idle))continue;entry.refreshOutfit=id;entry.refresh.request(selected.generations[entry.witch],GetTickCount64(),refreshDelay());++queued;}
    if(!queued){error="No owned selected replacement is visible to refresh. Its next assembly will use the current resources.";return false;}return true;
}
bool readOwnedOutfitVisibility(uintptr_t actor,int witch,OutfitVisibilityPolicy& policy){
    policy={};if(witch<0||witch>5||ticking)return false;
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
    if(!lock)return false;
    const auto selected=selections.snapshot();
    const auto found=owned.find(actor);if(found==owned.end())return true;const auto& entry=found->second;
    if(!entry.installed||entry.pending||entry.unacknowledged||entry.refresh.pending()||entry.witch!=witch)return true;
    WardrobeSnapshot snapshot;if(!captureObservedWitch(actor,witch,snapshot)||snapshot.node!=entry.node||snapshot.control!=entry.control||snapshot.renderer!=entry.renderer)return true;
    bool present=false;for(uint32_t i=0;i<snapshot.claimCount;++i)if(entry.current==snapshot.claims[i].prefab)present=true;
    if(!present)return true;
    policy.owned=true;policy.owner=entry.node;policy.prefab=entry.current;
    if(!engineReady||!writesAllowed)return true;
    WardrobeProviderOutfit provider;std::string error;
    if(!resolveWardrobeProvider(wardrobeOutfitId(selected.choices[witch]),witch,provider,error)||provider.prefab!=entry.current)return true;
    policy.enabled=true;policy.replaceBaseUnderwear=(provider.policyFlags&HA_REPLACE_BASE_UNDERWEAR)!=0;
    for(const auto& part:provider.visibilityParts)policy.parts.push_back({part.part,part.visible});
    return true;
}
}
