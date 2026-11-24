#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <intrin.h>
#include <Windows.h>
#include <MinHook.h>
extern "C" {
void* HexFrameOriginal=nullptr;
void* HexFrameCallOriginalA=nullptr;
void* HexFrameCallOriginalB=nullptr;
void* HexWardrobeHudOriginal=nullptr;
void* HexWardrobeApplyContinuation=nullptr;
alignas(4) volatile LONG HexWardrobeHideHud=0;
void HexFrameEntry();
void HexFrameCallA();
void HexFrameCallB();
void HexWardrobeHudEntry();
uint64_t HexWardrobeApplyStock(uint64_t node,uint64_t removals,uint64_t additions,uint8_t flag);
}
volatile uint64_t calls=0;
volatile double sideEffect=0;
extern "C" __declspec(noinline) void HexFrameCallback() noexcept {
    ++calls;sideEffect=std::sqrt(double(calls)+.125)*1.23456;
    const auto value=_mm_set1_ps(float(sideEffect));
    volatile float buffer[4];_mm_storeu_ps(const_cast<float*>(buffer),value);
    _ReadWriteBarrier();
}
__declspec(noinline) uint64_t integers(uint64_t a,double b,float c,uint64_t d,uint64_t fifth,uint64_t sixth) {
    return a^uint64_t(b*1000)^uint64_t(c*10000)^(d<<7)^(fifth<<17)^(sixth<<33);
}
__declspec(noinline) double floats(double a,float b,double c,float d,double fifth) {
    return a+double(b)*3+c*5+double(d)*7+fifth*11;
}
static bool checkedCallSites(){
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if(!page)return false;
    const unsigned char function[]={0x48,0x8d,0x41,0x25,0xc3}; // return first argument + 37
    std::memcpy(page+128,function,sizeof function);
    const unsigned char caller[]={0x48,0x83,0xec,0x28,0xe8,0,0,0,0,0x48,0x83,0xc4,0x28,0xc3};
    const auto writeCaller=[&](int offset,int target){
        std::memcpy(page+offset,caller,sizeof caller);
        const int32_t distance=target-offset-9;
        std::memcpy(page+offset+5,&distance,sizeof distance);
    };
    writeCaller(0,128);
    writeCaller(64,128);
    const unsigned char hudFunction[]={0x44,0x89,0xc8,0x44,0x29,0xc0,0xc3}; // return last - first
    std::memcpy(page+256,hudFunction,sizeof hudFunction);
    const unsigned char outfitContinuation[]={0x48,0x8b,0x44,0x24,0x18,0x4c,0x0f,0xb6,0x54,0x24,0x20,
        0x4c,0x31,0xd0,0x48,0x31,0xc8,0x48,0x31,0xd0,0xc3};
    std::memcpy(page+320,outfitContinuation,sizeof outfitContinuation);
    HexWardrobeApplyContinuation=page+320;
    writeCaller(192,256);
    FlushInstructionCache(GetCurrentProcess(),page,4096);
    using Caller=uint64_t(*)(uint64_t);
    const auto first=reinterpret_cast<Caller>(page);
    const auto second=reinterpret_cast<Caller>(page+64);
    const auto hud=reinterpret_cast<unsigned(*)(void*,void*,unsigned,unsigned)>(page+192);
    unsigned char firstCall[5],secondCall[5];
    std::memcpy(firstCall,page+4,sizeof firstCall);
    std::memcpy(secondCall,page+68,sizeof secondCall);
    bool passed=first(5)==42&&second(10)==47&&hud(nullptr,nullptr,3,9)==6&&
        HexWardrobeApplyStock(0x102030405ull,0x794260125ull,0x12278fa13ull,0xa9)==
            (0x102030405ull^0x794260125ull^0x12278fa13ull^0xa9)&&MH_Initialize()==MH_OK;
    if(passed){
        passed=MH_CreateHook(page+9,reinterpret_cast<void*>(&HexFrameCallA),&HexFrameCallOriginalA)==MH_OK&&
            MH_CreateHook(page+73,reinterpret_cast<void*>(&HexFrameCallB),&HexFrameCallOriginalB)==MH_OK&&
            MH_QueueEnableHook(page+9)==MH_OK&&MH_QueueEnableHook(page+73)==MH_OK&&MH_ApplyQueued()==MH_OK&&
            std::memcmp(page+4,firstCall,sizeof firstCall)==0&&std::memcmp(page+68,secondCall,sizeof secondCall)==0&&
            first(5)==42&&second(10)==47&&calls==20002&&std::isfinite(sideEffect);
        if(passed)passed=MH_CreateHook(page+196,reinterpret_cast<void*>(&HexWardrobeHudEntry),&HexWardrobeHudOriginal)==MH_OK&&
            MH_EnableHook(page+196)==MH_OK&&hud(nullptr,nullptr,3,9)==6;
        InterlockedExchange(&HexWardrobeHideHud,1);
        if(passed)passed=hud(nullptr,nullptr,3,9)==0;
        InterlockedExchange(&HexWardrobeHideHud,0);
        if(passed)passed=MH_DisableHook(page+196)==MH_OK&&hud(nullptr,nullptr,3,9)==6;
        MH_Uninitialize();
    }
    HexWardrobeApplyContinuation=nullptr;
    VirtualFree(page,0,MEM_RELEASE);
    return passed;
}
int main() {
    for(uint64_t i=1;i<10001;++i){
        HexFrameOriginal=reinterpret_cast<void*>(&integers);
        const auto invoke=reinterpret_cast<decltype(&integers)>(&HexFrameEntry);
        if(invoke(i,2.125f,3.625f,i+12,0x123456789,i*101)!=integers(i,2.125f,3.625f,i+12,0x123456789,i*101))return 1;
        HexFrameOriginal=reinterpret_cast<void*>(&floats);
        const auto invokeFloat=reinterpret_cast<decltype(&floats)>(&HexFrameEntry);
        if(invokeFloat(i*.25f,2.5f,3.75,4.125f,7.25)!=floats(i*.25f,2.5f,3.75,4.125f,7.25))return 2;
    }
    if(calls!=20000||!std::isfinite(sideEffect))return 3;
    if(!checkedCallSites())return 4;
    puts("PASS: frame dispatch and outfit application preserve native arguments. HUD suppression affects only the selected call.");
}
