#include <cstdint>
#include <cstdio>
#include <cmath>
#include <intrin.h>
extern "C" {
void* HexFrameOriginal=nullptr;
void HexFrameEntry();
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
    puts("PASS: frame dispatch retains mixed GPR/SSE and stack arguments and integer/floating returns across 20000 callbacks.");
}
