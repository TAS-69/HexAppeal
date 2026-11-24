#include "appearance_hooks.h"
#include <cstdio>
#include <cstdlib>

static void check(bool condition,const char* name){if(!condition){std::fprintf(stderr,"FAIL: %s\n",name);std::exit(1);}}
__declspec(noinline) static int destination(int value){return value+37;}
static void jump(unsigned char* from,const void* target){std::memset(from,0x90,16);from[0]=0xff;from[1]=0x25;std::memset(from+2,0,4);std::memcpy(from+6,&target,sizeof target);}
int main(){
    using namespace hex::appearance;
    check(versionWarning("Character Creator","de9b8b3acf79dce35d219cc9daf2f1daf7f6e482950cab389fc2dd4b8f236b32").empty(),"previously verified Character Creator remains recognized");
    check(versionWarning("Character Creator","9d78133641350e0776667efba765538c72cf37620a69f58cd8d9f8c44843f42b").empty(),"live-checked Character Creator 9.1.41 is recognized");
    check(versionWarning("Character Creator","331b0e06c89b347bfd08f83e88e83c2f8ba02c7c358dee36199410dd91cffe71").empty(),"live-checked Character Creator 9.1.42 is recognized");
    check(versionWarning("Character Creator","new").find("Character Creator has not been verified")!=std::string::npos,"unknown Character Creator warns with mod name");
    check(versionWarning("Spouse Overhaul","653a650945ee0fcfd5e3c70bf584b27a860bda88098e58e44abd4715b8df4513").empty(),"Spouse Overhaul 0.5.0 remains recognized");
    check(versionWarning("Spouse Overhaul","d0e4ee5165becebe75bcb24d253de4eef5b8b67ab1444e8420d82b8cc07f75da").empty(),"live-checked Spouse Overhaul 0.6.0 is recognized");
    check(versionWarning("Spouse Overhaul","new").find("Spouse Overhaul has not been verified")!=std::string::npos,"unknown Spouse Overhaul warns with mod name");
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));check(page!=nullptr,"allocate fixture");
    const auto owner=GetModuleHandleW(nullptr);const auto target=reinterpret_cast<void*>(&destination);
    jump(page,page+64);jump(page+64,target);
    const unsigned char stock[]{0x48,0x89,0x5c,0x24};
    std::memcpy(page+32,stock,sizeof stock);
    check(checkedEntry(page+32,stock),"stock write entry remains available");
    check(checkedEntry(page,stock,owner),"known appearance detour preserves a checked write entry");
    check(!checkedEntry(page,stock,GetModuleHandleW(L"kernel32.dll")),"unknown detour cannot enable writes");
    const unsigned char gameTail[]{0x48,0x8b,0xcf,0x45,0x33,0xc0,0x90};
    std::memcpy(page+16,gameTail,sizeof gameTail);
    check(checkedGameEntry(page,stock,16,gameTail)==GameEntry::Detoured,"an unlisted module can share the same callable game entry");
    page[16]=0x90;
    check(checkedGameEntry(page,stock,16,gameTail)==GameEntry::Unavailable,"a changed native continuation is not a compatible hook");
    check(entryIssue(page,"Outfit application").find("Outfit application failed the live instruction check.")!=std::string::npos,
          "a rejected entry reports the exact check");
    jump(page+64,reinterpret_cast<const void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetTickCount64")));
    const auto diagnosticOwner=hookOwnerName(page);
    check(!diagnosticOwner.empty()&&entryIssue(page,"Outfit application").find(diagnosticOwner)!=std::string::npos,
          "a rejected entry names the observed hook destination module");
    jump(page+64,target);
    std::memcpy(page+16,gameTail,sizeof gameTail);
    check(checkedGameEntry(page,stock,16,gameTail)==GameEntry::Detoured,"an executable hook is usable after the route recovers");
    check(knownJump(page,owner,nullptr),"bounded relay to recognized executable module");
    wchar_t image[1024]{};
    check(GetModuleFileNameW(owner,image,_countof(image))>0,"resolve the fixture module image name");
    const auto leaf=std::wcsrchr(image,L'\\');
    check(knownJumpTo(page,leaf?leaf+1:image)&&!knownJumpTo(page,L"unknown.asi"),"bind a relay to the expected image name only");
    check(knownJump(page,nullptr,owner),"second recognized module");
    check(!knownJump(page,GetModuleHandleW(L"kernel32.dll"),nullptr),"unrecognized module rejected");
    check(!knownJump(target,owner,nullptr),"unchanged entry is not accepted as a detour");
    jump(page+64,page);check(!knownJump(page,owner,nullptr),"cyclic relay rejected");
    check(entryIssue(page,"Outfit application").find("circular hook chain")!=std::string::npos,"a rejected chain reports the cycle");
    check(checkedGameEntry(page,stock,16,gameTail)==GameEntry::Unavailable,"an executable cycle is unusable");
    jump(page+64,reinterpret_cast<void*>(1));check(!knownJump(page,owner,nullptr),"invalid destination rejected");
    static unsigned char data[32]{};jump(page,data);check(!knownJump(page,owner,nullptr),"non-executable module data rejected");
    check(checkedGameEntry(page,stock,16,gameTail)==GameEntry::Unavailable,"a relay into non-executable data is rejected");
    jump(page,target);DWORD old{};check(VirtualProtect(page,4096,PAGE_NOACCESS,&old)!=FALSE,"protect fixture");
    check(!knownJump(page,owner,nullptr),"inaccessible entry rejected");VirtualFree(page,0,MEM_RELEASE);
    std::puts("PASS: informational version warnings, executable hooks without a name allowlist, rejection of changed native code, and route recovery.");
}
