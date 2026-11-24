// Signature lookup against the installed CrimsonDesert.exe, mapped as an image.
// On the build the signatures were made from, every address must match the
// address table exactly; on another build, every required address must resolve.
#include "game_signatures.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace ml::Log { void Write(const char*,const char*,...){} }

static void check(bool condition,const char* message){
    if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int wmain(int argc,wchar_t** argv){
    check(argc==2,"the test needs CrimsonDesert.exe");
    const HMODULE mapped=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    check(mapped!=nullptr,"map the game executable as an image");
    const auto* image=reinterpret_cast<const unsigned char*>(reinterpret_cast<uintptr_t>(mapped)&~uintptr_t(3));
    const auto started=std::chrono::steady_clock::now();
    const auto result=hex::resolveBySignature(image);
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    if(!result.ok)std::fprintf(stderr,"lookup stopped at %s\n",result.detail.c_str());
    check(result.ok,"every required game address resolves by signature");
    bool compared=false;std::string detail;
    const bool same=hex::signaturesMatchSource(result,compared,detail);
    if(!same)std::fprintf(stderr,"%s\n",detail.c_str());
    check(same,"signature lookup reproduces the address table");
    if(compared){
        // The address table and the signatures must describe the same build.
        const auto* known=hex::findGameLayout(result.layout.timeStamp,result.layout.imageSize,result.layout.checkSum);
        check(known!=nullptr,"the source build is in the address table");
        check(known->frameDispatch==result.layout.frameDispatch&&known->actor==result.layout.actor&&
              known->loaderGlobal==result.layout.loaderGlobal&&known->frameCalls[1]==result.layout.frameCalls[1]&&
              known->characterInfo==result.layout.characterInfo&&known->appearance==result.layout.appearance,
              "table and signature layouts agree, including the checked appearance type");
    }
    // Garbage is not an image.
    static const unsigned char junk[4096]{};
    check(!hex::resolveBySignature(junk).ok,"a buffer that is not a PE image is refused");
    std::printf("PASS: %zu of %zu game addresses resolved by signature in %.2f s%s.\n",result.found,result.total,seconds,
                compared?" and match the address table":" (different game build: table comparison skipped)");
    FreeLibrary(mapped);
    return 0;
}
