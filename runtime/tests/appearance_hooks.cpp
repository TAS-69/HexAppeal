#include "appearance_hooks.h"
#include <cstdio>
#include <cstdlib>

static void check(bool condition,const char* name){if(!condition){std::fprintf(stderr,"FAIL: %s\n",name);std::exit(1);}}
__declspec(noinline) static int destination(int value){return value+37;}
static void jump(unsigned char* from,const void* target){std::memset(from,0x90,16);from[0]=0xff;from[1]=0x25;std::memset(from+2,0,4);std::memcpy(from+6,&target,sizeof target);}
int main(){
    using namespace hex::appearance;
    check(versionWarning("Character Creator","known","known").empty(),"verified version has no warning");
    check(versionWarning("Character Creator","new","known").find("Character Creator has not been verified")!=std::string::npos,"unknown version warns with mod name");
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));check(page!=nullptr,"allocate fixture");
    const auto owner=GetModuleHandleW(nullptr);const auto target=reinterpret_cast<void*>(&destination);
    jump(page,page+64);jump(page+64,target);
    check(knownJump(page,owner,nullptr),"bounded relay to recognized executable module");
    check(knownJump(page,nullptr,owner),"second recognized module");
    check(!knownJump(page,GetModuleHandleW(L"kernel32.dll"),nullptr),"unrecognized module rejected");
    check(!knownJump(target,owner,nullptr),"unchanged entry is not accepted as a detour");
    jump(page+64,page);check(!knownJump(page,owner,nullptr),"cyclic relay rejected");
    jump(page+64,reinterpret_cast<void*>(1));check(!knownJump(page,owner,nullptr),"invalid destination rejected");
    static unsigned char data[32]{};jump(page,data);check(!knownJump(page,owner,nullptr),"non-executable module data rejected");
    jump(page,target);DWORD old{};check(VirtualProtect(page,4096,PAGE_NOACCESS,&old)!=FALSE,"protect fixture");
    check(!knownJump(page,owner,nullptr),"inaccessible entry rejected");VirtualFree(page,0,MEM_RELEASE);
    std::puts("PASS: appearance version warnings and bounded executable hook ownership.");
}
