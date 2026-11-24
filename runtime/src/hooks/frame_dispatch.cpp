#include "frame_dispatch.h"
#include "game_layout.h"
#include "../appearance_hooks.h"
#include "core/log.h"
#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstring>

extern "C" {
void* HexFrameOriginal=nullptr;
void* HexFrameCallOriginalA=nullptr;
void* HexFrameCallOriginalB=nullptr;
void HexFrameEntry();
void HexFrameCallA();
void HexFrameCallB();
}
namespace {
std::atomic<hex::FrameJob> frameJob{nullptr};
std::atomic<uint32_t> threadId{0};
thread_local bool runningJob=false;
hex::appearance::GameEntry frameEntry(void* p) {
    static const unsigned char expected[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x48,0x08,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    static const unsigned char tail[]={0x48,0x8d,0xa8,0x88,0xfe,0xff,0xff,0x48,0x81,0xec,0x40,0x02,0x00,0x00};
    return hex::appearance::checkedGameEntry(p,expected,22,tail);
}
bool checkedFrameContinuation(const unsigned char* call,unsigned char contextRegister){
    unsigned char bytes[19]{};
    if(!hex::creation::Read(call-4,bytes,sizeof bytes))return false;
    const unsigned char before[]={0x48,0x8b,contextRegister,0x18,0xe8};
    const unsigned char after[]={0x48,0x8b,contextRegister,0x38,0x33,0xd2,0x48,0x8b,0x49,0x30};
    return std::memcmp(bytes,before,sizeof before)==0&&std::memcmp(bytes+9,after,sizeof after)==0;
}
bool installFrameContinuations(unsigned char* game,const hex::GameLayout& layout,std::string& error){
    auto* firstCall=game+layout.frameCalls[0];
    auto* secondCall=game+layout.frameCalls[1];
    if(!layout.frameCalls[0]||!layout.frameCalls[1]){error="This game build has no alternate frame call sites.";return false;}
    // Other mods can own the call or its destination. Hook the following instructions.
    auto* first=firstCall+5;
    auto* second=secondCall+5;
    if(!checkedFrameContinuation(firstCall,0x4e)){
        error=hex::appearance::entryIssue(first,"The first alternate game-thread call site");
        return false;
    }
    if(!checkedFrameContinuation(secondCall,0x4d)){
        error=hex::appearance::entryIssue(second,"The second alternate game-thread call site");
        return false;
    }
    const auto firstResult=MH_CreateHook(first,reinterpret_cast<void*>(&HexFrameCallA),&HexFrameCallOriginalA);
    if(firstResult!=MH_OK){
        error=std::string("The first alternate game-thread hook could not be prepared (")+MH_StatusToString(firstResult)+").";
        return false;
    }
    const bool secondPrepared=MH_CreateHook(second,reinterpret_cast<void*>(&HexFrameCallB),&HexFrameCallOriginalB)==MH_OK;
    if(secondPrepared&&MH_QueueEnableHook(first)==MH_OK&&MH_QueueEnableHook(second)==MH_OK&&MH_ApplyQueued()==MH_OK)return true;
    MH_RemoveHook(first);
    if(secondPrepared)MH_RemoveHook(second);
    error=secondPrepared?"The alternate game-thread hooks could not be enabled.":"The second alternate game-thread hook could not be prepared.";
    return false;
}
}
extern "C" void HexFrameCallback() noexcept {
    const auto job=frameJob.load();
    if(!job||runningJob)return;
    const auto current=GetCurrentThreadId();
    auto expected=threadId.load();
    if(!expected)threadId.compare_exchange_strong(expected,current);
    if(expected&&expected!=current){
        static thread_local uint64_t nextProbe=0;
        const auto now=GetTickCount64();
        if(now<nextProbe)return;
        nextProbe=now+500;
        const auto owner=OpenThread(SYNCHRONIZE,FALSE,expected);
        const bool exited=owner?WaitForSingleObject(owner,0)==WAIT_OBJECT_0:GetLastError()==ERROR_INVALID_PARAMETER;
        if(owner)CloseHandle(owner);
        if(!exited||!threadId.compare_exchange_strong(expected,current))return;
        LOG("Wardrobe dispatch moved to the replacement game thread.");
    }
    runningJob=true;
    job();
    runningJob=false;
}
namespace hex {
bool initializeFrameDispatch(FrameJob callback,std::string& error) {
    static bool installed=false;
    if(!callback){error="Missing game-thread callback";return false;}
    if(installed)return frameJob.load()==callback;
    if(!loadedGameLayout()){error="This Crimson Desert build has no checked game-thread route";return false;}
    auto* game=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto& layout=gameLayout();
    auto* target=game+layout.frameDispatch;
    const auto entry=frameEntry(target);
    frameJob=callback;
    std::string alternateIssue;
    const auto continuations=[&](){
        if(!installFrameContinuations(game,layout,alternateIssue))return false;
        installed=true;
        LOG("Wardrobe dispatch runs after two checked game-thread frame calls.");
        return true;
    };
    if(continuations())return true;
    if(entry!=appearance::GameEntry::Stock)
        error=appearance::entryIssue(target,"The main game-thread update");
    else{
        auto result=MH_CreateHook(target,reinterpret_cast<void*>(&HexFrameEntry),&HexFrameOriginal);
        if(result==MH_OK){
            result=MH_EnableHook(target);
            if(result==MH_OK){installed=true;return true;}
            MH_RemoveHook(target);
        }
        error=std::string("The main game-thread hook could not start (")+MH_StatusToString(result)+").";
        const auto owner=appearance::hookOwnerName(target);
        if(!owner.empty())error+=" The entry currently routes into "+owner+".";
    }
    frameJob=nullptr;
    if(!alternateIssue.empty())error+=" "+alternateIssue;
    return false;
}
uint32_t frameThreadId(){return threadId.load();}
}
