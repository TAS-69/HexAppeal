#include "frame_dispatch.h"
#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstring>

extern "C" {
void* HexFrameOriginal=nullptr;
void HexFrameEntry();
}
namespace {
std::atomic<hex::FrameJob> frameJob{nullptr};
std::atomic<uint32_t> threadId{0};
bool signature(void* p) {
    static const unsigned char expected[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x48,0x08,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    __try{return memcmp(p,expected,sizeof(expected))==0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
extern "C" void HexFrameCallback() noexcept {
    const auto current=GetCurrentThreadId();uint32_t expected=0;
    threadId.compare_exchange_strong(expected,current);
    if(threadId.load()!=current)return;
    const auto job=frameJob.load();if(job)job();
}
namespace hex {
bool initializeFrameDispatch(FrameJob callback,std::string& error) {
    if(!callback){error="Missing game-thread callback";return false;}
    auto* target=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+0x414c490;
    if(!signature(target)){error="The game-thread entry differs from the inspected build";return false;}
    frameJob=callback;
    if(MH_CreateHook(target,reinterpret_cast<void*>(&HexFrameEntry),&HexFrameOriginal)!=MH_OK){frameJob=nullptr;error="Game-thread dispatch could not be prepared";return false;}
    if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);frameJob=nullptr;error="Game-thread dispatch could not start";return false;}
    return true;
}
uint32_t frameThreadId(){return threadId.load();}
}
