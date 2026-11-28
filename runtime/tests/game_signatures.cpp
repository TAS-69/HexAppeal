// Signature lookup against the installed CrimsonDesert.exe, mapped as an image.
// On the build the signatures were made from, every address must match the
// address table exactly; on another build, every required address must resolve.
// On the checked Steam 2.06 build, every 2.06 address must also be the checked
// one, and a copy with another build identity must resolve the same addresses
// as an unverified 2.06 build.
#include "game_signatures.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ml::Log { void Write(const char*,const char*,...){} }

static void check(bool condition,const char* message){
    if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
// Copies a mapped image page by page; pages that cannot be read stay zero.
static bool copyImage(const unsigned char* image,unsigned char* copy,size_t size){
    for(size_t at=0;at<size;at+=4096){
        __try{std::memcpy(copy+at,image+at,size-at<4096?size-at:4096);}
        __except(EXCEPTION_EXECUTE_HANDLER){}
    }
    return true;
}
// The addresses checked in game on the Steam 2.06 executable.
static void checkSteam206(const hex::SignatureResult& result,const char* label){
    char message[256];
    const auto expect=[&](uintptr_t value,uintptr_t checked,const char* field){
        std::snprintf(message,sizeof message,"%s: %s resolves to the checked 2.06 address (%llx, expected %llx)",label,field,
            static_cast<unsigned long long>(value),static_cast<unsigned long long>(checked));
        check(value==checked,message);
    };
    const auto& e=result.expanded;
    expect(e.poseUpload,0x324fd80,"poseUpload");
    expect(e.menuChoose,0x1325f40,"menuChoose");expect(e.menuCommit,0x1326390,"menuCommit");
    expect(e.menuDisplay,0x131dd80,"menuDisplay");expect(e.menuRefresh,0x131da90,"menuRefresh");
    expect(e.menuResolve,0x7819d0,"menuResolve");expect(e.menuRelease,0x16e3b60,"menuRelease");expect(e.menuClose,0x131bac0,"menuClose");
    expect(e.widgetFind,0x3a75cf0,"widgetFind");expect(e.widgetResolve,0x738010,"widgetResolve");expect(e.widgetSet,0x777a70,"widgetSet");
    expect(e.chooseCaller,0x131b192,"chooseCaller");expect(e.commitDisplay,0x1326eb1,"commitDisplay");
    expect(e.closeEvent,0x734fdd4,"closeEvent");expect(e.cursorClass,0x5c7f9d8,"cursorClass");
    const uintptr_t alternate[]{0xb3e5a5,0xb3e7bc,0xb3f655,0xc5de0d};
    for(size_t i=0;i<4;++i)expect(result.interaction.alternateFocusLists[i],alternate[i],"alternate focus caller");
    expect(result.layout.mergeAppearance,0x30e2040,"mergeAppearance");expect(result.layout.applyAppearance,0x30d9510,"applyAppearance");
    expect(result.interaction.build,0x2430620,"ia_build");expect(result.interaction.records,0x73f21f0,"ia_records");
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
    if(result.layout.expandedRecords){
        if(!result.missing.empty())std::fprintf(stderr,"missing: %s\n",result.missing.c_str());
        check(result.missing.empty(),"every 2.06 address resolves by signature");
    }
    bool restamped=false;
    if(result.checkedBuild){
        check(result.layout.expandedRecords,"the checked Steam 2.06 build uses the 2.06 record layout");
        checkSteam206(result,"Steam 2.06");
        // Another store's link of the same code has another stamp and checksum. It must be
        // recognised as 2.06 by its code, resolve the same addresses, and not count as checked.
        IMAGE_DOS_HEADER dos{};std::memcpy(&dos,image,sizeof dos);
        IMAGE_NT_HEADERS64 nt{};std::memcpy(&nt,image+dos.e_lfanew,sizeof nt);
        const size_t size=nt.OptionalHeader.SizeOfImage;
        auto* copy=static_cast<unsigned char*>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        check(copy&&copyImage(image,copy,size),"copy the mapped game image");
        auto* header=reinterpret_cast<IMAGE_NT_HEADERS64*>(copy+dos.e_lfanew);
        header->FileHeader.TimeDateStamp=0x6b07e5c4;header->OptionalHeader.CheckSum^=0x5a5a5a5a;
        const auto other=hex::resolveBySignature(copy);
        if(!other.ok)std::fprintf(stderr,"restamped lookup stopped at %s\n",other.detail.c_str());
        check(other.ok,"a 2.06 build with another identity resolves every required address");
        check(other.layout.expandedRecords&&!other.checkedBuild,"a 2.06 build with another identity is recognised by code and is not the checked build");
        if(!other.missing.empty())std::fprintf(stderr,"missing: %s\n",other.missing.c_str());
        check(other.missing.empty(),"a 2.06 build with another identity resolves every 2.06 address");
        checkSteam206(other,"restamped 2.06");
        check(std::memcmp(&other.interaction,&result.interaction,sizeof other.interaction)==0&&
              other.layout.frameDispatch==result.layout.frameDispatch&&other.layout.actor==result.layout.actor&&
              other.layout.characterInfo==result.layout.characterInfo&&other.layout.hudCall==result.layout.hudCall,
              "a 2.06 build with another identity resolves the same layout");
        VirtualFree(copy,0,MEM_RELEASE);
        restamped=true;
    }
    // Garbage is not an image.
    static const unsigned char junk[4096]{};
    check(!hex::resolveBySignature(junk).ok,"a buffer that is not a PE image is refused");
    std::printf("PASS: %zu of %zu game addresses resolved by signature in %.2f s%s%s.\n",result.found,result.total,seconds,
                compared?" and match the address table":result.checkedBuild?"; every 2.06 address is the checked Steam one":" (different game build: table comparison skipped)",
                restamped?"; a restamped copy resolves the same addresses as an unverified 2.06 build":"");
    FreeLibrary(mapped);
    return 0;
}
