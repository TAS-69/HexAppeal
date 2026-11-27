#include "game_signatures.h"
#include "core/log.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

namespace hex {
namespace {
enum class Method{Code,Reference,Rtti,Optional};
enum class Expect{Code,ReadOnly,Writable};
struct Signature{
    const char* field;Method method;Expect expect;bool call;const char* pattern;
    int offset,disp,end;const char* rtti;uintptr_t sourceRva;
};
constexpr Signature signatures[]={
#include "game_signatures_table.inc"
};
constexpr uint32_t sourceStamp=0x6afdc0cf;  // the build the table was generated from

// 2.06 changes the appearance record ABI as well as the signatures below.
// Keep these signatures tied to the executable checked for that ABI.
constexpr Signature expandedRecordSignatures[]={
    {"mergeAppearance",Method::Code,Expect::Code,false,"48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 38 E2 FF FF B8 C8 1E 00 00",0,0,0,nullptr,0x30e2040},
    {"copyPart",Method::Code,Expect::Code,false,"48 89 5C 24 18 48 89 4C 24 08 55 56 57 41 56 41 57 48 83 EC 20 4C 8B F2 48 8B F1 45 33 FF 4C 89 39 48 8B 02",0,0,0,nullptr,0x578600},
    {"destroyRecord",Method::Code,Expect::Code,false,"48 89 5C 24 18 48 89 4C 24 08 57 48 83 EC 20 48 8B F9 48 83 C1 68 48 89 4C 24 38 48 8D 15",0,0,0,nullptr,0x567060},
    {"copyRecord",Method::Code,Expect::Code,false,"48 89 5C 24 18 48 89 4C 24 08 55 56 57 41 56 41 57 48 83 EC 20 4C 8B F2 48 8B F1 33 ED 48 89 29 48 8B 0A",0,0,0,nullptr,0x57a1c0},
    {"applyAppearance",Method::Code,Expect::Code,false,"48 89 5C 24 10 44 88 4C 24 20 4C 89 44 24 18 48 89 4C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 90 E2 FF FF B8 70 1E 00 00",0,0,0,nullptr,0x30d9510},
    {"appearance",Method::Rtti,Expect::ReadOnly,false,nullptr,0,0,0,".?AVAppearanceTableData@pa@@",0},
    {"emptyWrapper",Method::Reference,Expect::Writable,false,"48 89 0E 4C 8D 3D ?? ?? ?? ?? 49 3B CF 74 0B 8B 41 10 85 C0 78 04 F0 FF 41 10",0,6,10,nullptr,0x70d4348},
    {"characterInfo",Method::Reference,Expect::Writable,false,"BE FF FF FF FF 45 8B 4E 04 4C 8B 15 ?? ?? ?? ?? 41 83 7A 4C 00 74 4A 41 8B 4A 48 85 C9 74 42 33 D2",0,12,16,nullptr,0x73f2178},
    {"ia_build",Method::Code,Expect::Code,false,"48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 20 44 88 44 24 18 55 41 54 41 55 41 56 41 57 48 8D 6C 24 C9 48 81 EC E0 00 00 00",0,0,0,nullptr,0x2430620},
    {"ia_records",Method::Reference,Expect::Writable,false,"C6 45 17 00 EB 72 8B 0B 4C 8B 15 ?? ?? ?? ?? 41 83 7A 4C 00 74 52 45 8B 42 48 45 85 C0 74 49",0,11,15,nullptr,0x73f21f0},
    {"ia_reserve",Method::Reference,Expect::Code,false,"E8 ?? ?? ?? ?? 8B 47 08 48 6B C8 38 48 03 0F 48 8B D3 E8",0,1,5,nullptr,0xb455a0},
    {"ia_emptyLabel",Method::Reference,Expect::Writable,false,"48 89 0E 4C 8D 3D ?? ?? ?? ?? 49 3B CF 74 0B 8B 41 10 85 C0 78 04 F0 FF 41 10",0,6,10,nullptr,0x70d4348},
    {"ia_focus0",Method::Code,Expect::Code,false,"E8 ?? ?? ?? ?? 90 48 8B 8D 50 06 00 00 48 8B 01 B2 01 FF 50 20 45 32 FF 48 8B 7D 80",5,0,0,nullptr,0xb3e359},
    {"ia_focus1",Method::Code,Expect::Code,false,"E8 ?? ?? ?? ?? 90 48 8B 8D B0 0E 00 00 48 8B 01 B2 01 FF 50 20 83 7D 58 00 74 33 48 8D 45 50",5,0,0,nullptr,0xc5cec9},
    {"ia_focus2",Method::Code,Expect::Code,false,"E8 ?? ?? ?? ?? 90 48 8B 8D D0 0D 00 00 48 8B 01 B2 01 FF 50 20 83 7D 88 00 74 3E 48 8D 45 80",5,0,0,nullptr,0xc52761},
};

const Signature& signatureFor(const Signature& stock,bool expanded){
    if(expanded)for(const auto& sig:expandedRecordSignatures)
        if(std::strcmp(stock.field,sig.field)==0)return sig;
    return stock;
}

bool readRaw(const unsigned char* from,void* to,size_t size){
    __try{std::memcpy(to,from,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}

struct Section{uint32_t rva=0,size=0;bool code=false,readOnly=false,writable=false;};
struct Image{
    const unsigned char* base=nullptr;
    uintptr_t imageBase=0;
    uint32_t stamp=0,size=0,checksum=0;
    std::vector<Section> sections;
    const Section* at(uint32_t rva) const{
        for(const auto& s:sections)if(rva>=s.rva&&rva-s.rva<s.size)return &s;
        return nullptr;
    }
};
bool readImage(const unsigned char* base,Image& image){
    if(!base)return false;
    IMAGE_DOS_HEADER dos{};
    if(!readRaw(base,&dos,sizeof dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0||dos.e_lfanew>0xf00)return false;
    IMAGE_NT_HEADERS64 nt{};
    if(!readRaw(base+dos.e_lfanew,&nt,sizeof nt)||nt.Signature!=IMAGE_NT_SIGNATURE||
       nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||
       nt.FileHeader.SizeOfOptionalHeader<sizeof nt.OptionalHeader)return false;
    const auto sectionCount=nt.FileHeader.NumberOfSections;
    const auto sectionOffset=static_cast<size_t>(dos.e_lfanew)+offsetof(IMAGE_NT_HEADERS64,OptionalHeader)+nt.FileHeader.SizeOfOptionalHeader;
    if(!sectionCount||sectionCount>96||sectionOffset+sectionCount*sizeof(IMAGE_SECTION_HEADER)>nt.OptionalHeader.SizeOfHeaders||
       nt.OptionalHeader.SizeOfHeaders>nt.OptionalHeader.SizeOfImage)return false;
    image.base=base;image.imageBase=nt.OptionalHeader.ImageBase;image.stamp=nt.FileHeader.TimeDateStamp;
    image.size=nt.OptionalHeader.SizeOfImage;image.checksum=nt.OptionalHeader.CheckSum;
    for(unsigned i=0;i<sectionCount;++i){
        IMAGE_SECTION_HEADER section{};
        if(!readRaw(base+sectionOffset+i*sizeof section,&section,sizeof section))return false;
        const auto c=section.Characteristics;
        Section s;s.rva=section.VirtualAddress;s.size=section.Misc.VirtualSize;
        if(s.rva>image.size||s.size>image.size-s.rva)return false;
        const bool x=(c&IMAGE_SCN_MEM_EXECUTE)!=0,w=(c&IMAGE_SCN_MEM_WRITE)!=0,r=(c&IMAGE_SCN_MEM_READ)!=0;
        s.code=x&&!w;s.readOnly=r&&!w&&!x;s.writable=w&&!x;
        image.sections.push_back(s);
    }
    return true;
}

struct Pattern{unsigned char bytes[192]{};bool wild[192]{};size_t length=0,key=0;};
bool parse(const char* text,Pattern& p){
    std::istringstream in(text);std::string token;
    while(in>>token){
        if(p.length>=sizeof p.bytes)return false;
        if(token=="??")p.wild[p.length]=true;
        else{unsigned value=0;if(std::sscanf(token.c_str(),"%2x",&value)!=1)return false;p.bytes[p.length]=uint8_t(value);}
        ++p.length;
    }
    return p.length>0;
}
// The search keys on the pattern's rarest byte in the code being scanned.
void chooseKey(Pattern& p,const std::array<uint32_t,256>& frequency){
    uint32_t best=UINT32_MAX;
    for(size_t i=0;i<p.length;++i)if(!p.wild[i]&&frequency[p.bytes[i]]<best){best=frequency[p.bytes[i]];p.key=i;}
}
// Raw memory work stays in small functions so SEH can contain a fault.
// Counts matches up to 'limit'; 'at' receives the first.
int scanRaw(const unsigned char* begin,size_t size,const Pattern* p,size_t* at,int limit){
    __try{
        if(size<p->length)return 0;
        int found=0;const auto key=p->bytes[p->key];
        const unsigned char* cursor=begin+p->key;
        const unsigned char* last=begin+(size-p->length)+p->key;
        while(cursor<=last){
            cursor=static_cast<const unsigned char*>(std::memchr(cursor,key,size_t(last-cursor)+1));
            if(!cursor)break;
            const auto* start=cursor-p->key;bool ok=true;
            for(size_t i=0;i<p->length;++i)if(!p->wild[i]&&start[i]!=p->bytes[i]){ok=false;break;}
            if(ok){if(!found)*at=size_t(start-begin);if(++found>=limit)return found;}
            ++cursor;
        }
        return found;
    }__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
bool countBytes(const unsigned char* begin,size_t size,uint32_t* frequency){
    __try{for(size_t i=0;i<size;++i)++frequency[begin[i]];return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Complete object locator for 'descriptor' at offset 0: signature 1 and a self RVA.
int findLocator(const unsigned char* begin,uint32_t rva,size_t size,uint32_t descriptor,uint32_t* found){
    __try{
        int count=0;
        for(size_t i=0;i+24<=size;i+=4){
            uint32_t v[6];std::memcpy(v,begin+i,sizeof v);
            if(v[3]==descriptor&&v[0]==1&&v[1]==0&&v[5]==rva+uint32_t(i)){*found=rva+uint32_t(i);if(++count>1)return count;}
        }
        return count;
    }__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
int findPointer(const unsigned char* begin,uint32_t rva,size_t size,uint64_t value,uint32_t* found){
    __try{
        int count=0;
        for(size_t i=0;i+8<=size;i+=8){
            uint64_t v;std::memcpy(&v,begin+i,8);
            if(v==value){*found=rva+uint32_t(i);if(++count>1)return count;}
        }
        return count;
    }__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}

struct Resolver{
    const Image& image;
    std::array<uint32_t,256> frequency{};
    bool counted=false;
    explicit Resolver(const Image& img):image(img){}
    // Exactly one match across the code sections; returns its RVA.
    bool unique(const char* text,uint32_t& rva,std::string& why){
        Pattern p;if(!parse(text,p)){why="malformed pattern";return false;}
        if(!counted){for(const auto& s:image.sections)if(s.code)countBytes(image.base+s.rva,s.size,frequency.data());counted=true;}
        chooseKey(p,frequency);
        int total=0;
        for(const auto& s:image.sections){
            if(!s.code)continue;
            size_t at=0;const int n=scanRaw(image.base+s.rva,s.size,&p,&at,2);
            if(n<0){why="unreadable code";return false;}
            if(n>0){total+=n;rva=s.rva+uint32_t(at);}
            if(total>1){why="pattern matches more than once";return false;}
        }
        if(total==0){why="pattern not found";return false;}
        return true;
    }
    bool rtti(const char* name,uint32_t& vtable,std::string& why){
        // Type descriptor: 16 bytes before its NUL-terminated name, first field in the image.
        Pattern p;const size_t length=std::strlen(name)+1;
        if(length>sizeof p.bytes){why="class name too long";return false;}
        std::memcpy(p.bytes,name,length);p.length=length;p.key=0;
        uint32_t descriptor=0;int count=0;
        for(const auto& s:image.sections){
            if(!s.readOnly&&!s.writable)continue;  // data only; never the protected code section
            size_t from=0;
            while(from<s.size){
                size_t at=0;const int n=scanRaw(image.base+s.rva+from,s.size-from,&p,&at,1);
                if(n<=0)break;
                const uint32_t candidate=s.rva+uint32_t(from+at);
                uint64_t vft=0;
                if(candidate>=16&&readRaw(image.base+candidate-16,&vft,8)&&vft-image.imageBase<image.size){
                    descriptor=candidate-16;if(++count>1){why="class name found more than once";return false;}
                }
                from+=at+1;
            }
        }
        if(count!=1){why="class name not found";return false;}
        uint32_t locator=0;count=0;
        for(const auto& s:image.sections){
            if(!s.readOnly)continue;
            uint32_t found=0;const int n=findLocator(image.base+s.rva,s.rva,s.size,descriptor,&found);
            if(n<0){why="unreadable data";return false;}
            if(n>0){count+=n;locator=found;}
        }
        if(count!=1){why=count?"more than one object locator":"object locator not found";return false;}
        uint32_t slot=0;count=0;
        for(const auto& s:image.sections){
            if(!s.readOnly)continue;
            uint32_t found=0;const int n=findPointer(image.base+s.rva,s.rva,s.size,image.imageBase+locator,&found);
            if(n<0){why="unreadable data";return false;}
            if(n>0){count+=n;slot=found;}
        }
        if(count!=1){why=count?"more than one vtable":"vtable not found";return false;}
        vtable=slot+8;return true;
    }
    bool placed(uint32_t rva,Expect expect) const{
        const auto* s=image.at(rva);if(!s)return false;
        return expect==Expect::Code?s->code:expect==Expect::ReadOnly?s->readOnly:s->writable;
    }
    bool resolve(const Signature& sig,uint32_t& rva,std::string& why){
        switch(sig.method){
        case Method::Code:{
            uint32_t match=0;if(!unique(sig.pattern,match,why))return false;
            rva=match+uint32_t(sig.offset);break;}
        case Method::Reference:{
            uint32_t match=0;if(!unique(sig.pattern,match,why))return false;
            int32_t disp=0;if(!readRaw(image.base+match+sig.disp,&disp,4)){why="unreadable reference";return false;}
            rva=uint32_t(int64_t(match)+sig.end+disp);break;}
        case Method::Rtti:
            if(!rtti(sig.rtti,rva,why))return false;break;
        case Method::Optional:
            rva=0;return true;
        }
        if(!placed(rva,sig.expect)){why="address is in the wrong kind of section";return false;}
        if(sig.call){uint8_t op=0;if(!readRaw(image.base+rva,&op,1)||op!=0xE8){why="not a call instruction";return false;}}
        return true;
    }
};

bool calls(const Image& image,uintptr_t site,uintptr_t target){
    unsigned char instruction[5]{};
    if(site>image.size||sizeof instruction>image.size-site||
       !readRaw(image.base+site,instruction,sizeof instruction)||instruction[0]!=0xe8)return false;
    int32_t displacement=0;
    std::memcpy(&displacement,instruction+1,sizeof displacement);
    return int64_t(site)+int64_t(sizeof instruction)+displacement==int64_t(target);
}

uintptr_t* slot(GameLayout& layout,InteractionAddresses& ia,const char* field){
    const std::pair<const char*,uintptr_t*> slots[]={
        {"frameDispatch",&layout.frameDispatch},{"visibilityObserver",&layout.visibilityObserver},
        {"visibilitySet",&layout.visibilitySet},{"visibilityGet",&layout.visibilityGet},{"internString",&layout.internString},
        {"rebuildAppearance",&layout.rebuildAppearance},{"mergeAppearance",&layout.mergeAppearance},{"copyPart",&layout.copyPart},
        {"releaseWrapper",&layout.releaseWrapper},{"destroyRecord",&layout.destroyRecord},{"copyRecord",&layout.copyRecord},
        {"applyAppearance",&layout.applyAppearance},{"actor",&layout.actor},{"control",&layout.control},{"skinned",&layout.skinned},
        {"skinGlobal",&layout.skinGlobal},{"node",&layout.node},{"controller",&layout.controller},{"loaderGlobal",&layout.loaderGlobal},
        {"appearance",&layout.appearance},{"emptyWrapper",&layout.emptyWrapper},{"emptyRecord",&layout.emptyRecord},
        {"actorManagerGlobal",&layout.actorManagerGlobal},{"actorManager",&layout.actorManager},{"actorContainer",&layout.actorContainer},
        {"playerActor",&layout.playerActor},{"frameCalls0",&layout.frameCalls[0]},{"frameCalls1",&layout.frameCalls[1]},
        {"hudCall",&layout.hudCall},{"hudSubmit",&layout.hudSubmit},{"characterInfo",&layout.characterInfo},
        {"ia_build",&ia.build},{"ia_evaluate",&ia.evaluate},{"ia_select",&ia.select},{"ia_records",&ia.records},
        {"ia_reserve",&ia.reserve},{"ia_copy",&ia.copy},{"ia_makeLabel",&ia.makeLabel},{"ia_copyLabel",&ia.copyLabel},
        {"ia_emptyLabel",&ia.emptyLabel},{"ia_focus0",&ia.focusLists[0]},{"ia_focus1",&ia.focusLists[1]},{"ia_focus2",&ia.focusLists[2]}};
    for(const auto& [name,pointer]:slots)if(std::strcmp(name,field)==0)return pointer;
    return nullptr;
}

GameLayout resolvedLayout{};
InteractionAddresses resolvedInteraction{};

SignatureResult scanGameFile(const Image& loaded){
    SignatureResult result;
    wchar_t path[32768]{};
    const auto length=GetModuleFileNameW(nullptr,path,32768);
    if(!length||length>=32768){
        result.detail="the game executable path is unavailable";
        return result;
    }
    const HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE){
        result.detail="the game executable could not be opened";
        return result;
    }
    // A read-only image view excludes runtime modifications from other plugins.
    constexpr DWORD imageNoExecute=0x11000000;
    HANDLE section=CreateFileMappingW(file,nullptr,PAGE_READONLY|imageNoExecute,0,0,nullptr);
    if(!section)section=CreateFileMappingW(file,nullptr,PAGE_READONLY|SEC_IMAGE,0,0,nullptr);
    const auto* view=section?static_cast<const unsigned char*>(MapViewOfFile(section,FILE_MAP_READ,0,0,0)):nullptr;
    try{
        Image onDisk;
        if(!readImage(view,onDisk))result.detail="the game executable could not be mapped";
        else if(onDisk.stamp!=loaded.stamp||onDisk.size!=loaded.size||onDisk.checksum!=loaded.checksum)
            result.detail="the game executable on disk is a different build from the running game";
        else result=resolveBySignature(view);
    }catch(...){
        if(view)UnmapViewOfFile(view);
        if(section)CloseHandle(section);
        CloseHandle(file);
        throw;
    }
    if(view)UnmapViewOfFile(view);
    if(section)CloseHandle(section);
    CloseHandle(file);
    return result;
}

DWORD WINAPI resolveRunningGame(LPVOID){
    try{
        Image image;
        if(!readImage(reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr)),image)){
            LOG_ERR("[layout] The game image could not be read; game addresses stay unavailable.");
        }else{
            LOG("[layout] Checking game addresses by code signature for build %08X.",image.stamp);
            const auto started=std::chrono::steady_clock::now();
            const auto result=scanGameFile(image);
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
            if(result.ok){
                resolvedLayout=result.layout;
                resolvedInteraction=result.interaction;
                signatureGameLayout().store(&resolvedLayout,std::memory_order_release);
                LOG_OK("[layout] Found %zu of %zu game addresses by code signature in %.1f s.",result.found,result.total,seconds);
            }else{
                LOG_ERR("[layout] Signature lookup stopped at %s. Features that need game addresses stay unavailable on this build.",result.detail.c_str());
            }
        }
    }catch(...){
        gameLayoutSettled().store(true,std::memory_order_release);
        LOG_ERR("[layout] Signature lookup could not complete. Game addresses stay unavailable.");
        return 0;
    }
    gameLayoutSettled().store(true,std::memory_order_release);
    return 0;
}

}

SignatureResult resolveBySignature(const unsigned char* base){
    SignatureResult result;Image image;
    if(!readImage(base,image)){result.detail="not a 64-bit PE image";return result;}
    Resolver resolver(image);
    const bool expanded=image.stamp==expandedRecordBuild&&image.size==expandedRecordImageSize&&image.checksum==expandedRecordChecksum;
    for(const auto& stock:signatures){
        const auto& sig=signatureFor(stock,expanded);
        ++result.total;
        uint32_t rva=0;std::string why;
        if(!resolver.resolve(sig,rva,why)){
            if(sig.method==Method::Optional)continue;
            result.detail=std::string(sig.field)+" ("+why+")";
            return result;
        }
        if(sig.method!=Method::Optional)++result.found;
        if(expanded&&sig.sourceRva&& &sig!=&stock&&rva!=sig.sourceRva){
            result.detail=std::string(sig.field)+" (signature differs from the checked 2.06 address)";
            return result;
        }
        *slot(result.layout,result.interaction,sig.field)=rva;
    }
    result.layout.timeStamp=image.stamp;result.layout.imageSize=image.size;result.layout.checkSum=image.checksum;
    result.layout.digest="";
    result.layout.expandedRecords=expanded;
    if(!calls(image,result.layout.frameCalls[0],result.layout.frameDispatch)||
       !calls(image,result.layout.frameCalls[1],result.layout.frameDispatch)||
       !calls(image,result.layout.hudCall,result.layout.hudSubmit)){
        result.detail="the resolved frame or HUD call does not target its checked function";
        return result;
    }
    if(expanded)for(const auto site:result.interaction.focusLists){
        if(site<5||!calls(image,site-5,result.interaction.build)){
            result.detail="a 2.06 interaction caller does not target the checked builder";
            return result;
        }
    }
    if(expanded){
        // These builders feed the focus result list and the visible action picker.
        constexpr uintptr_t alternativeCallers[]{0xb3e5a5,0xb3e7bc,0xb3f655,0xc5de0d};
        for(size_t i=0;i<std::size(alternativeCallers);++i){
            const auto site=alternativeCallers[i];
            if(calls(image,site-5,result.interaction.build))result.interaction.alternateFocusLists[i]=site;
        }
    }
    bool compared=false;
    if(!signaturesMatchSource(result,compared,result.detail))return result;
    if(compared)
        if(const auto* known=findGameLayout(image.stamp,image.size,image.checksum))
            result.layout.appearance=known->appearance;
    result.ok=true;
    result.detail="all required addresses found";
    return result;
}

bool signaturesMatchSource(const SignatureResult& result,bool& compared,std::string& detail){
    compared=result.layout.timeStamp==sourceStamp;
    if(!compared)return true;
    auto layout=result.layout;auto interaction=result.interaction;
    for(const auto& sig:signatures){
        const auto value=*slot(layout,interaction,sig.field);
        if(sig.method==Method::Optional&&value==0)continue;
        if(value!=sig.sourceRva){
            char text[160];std::snprintf(text,sizeof text,"%s resolved to %llx, expected %llx",sig.field,
                static_cast<unsigned long long>(value),static_cast<unsigned long long>(sig.sourceRva));
            detail=text;return false;
        }
    }
    return true;
}

void startGameLayoutResolution(const std::filesystem::path& ini){
    const bool force=GetPrivateProfileIntW(L"Development",L"ForceSignatureLayout",0,ini.c_str())!=0;
    ignoreGameLayoutTable().store(force,std::memory_order_release);
    if(!force&&tableGameLayout())return;
    gameLayoutSettled().store(false,std::memory_order_release);
    if(const auto thread=CreateThread(nullptr,0,resolveRunningGame,nullptr,0,nullptr))CloseHandle(thread);
    else{gameLayoutSettled().store(true,std::memory_order_release);LOG_ERR("[layout] Windows could not start the address lookup.");}
}

const InteractionAddresses* signatureInteraction(){
    return signatureGameLayout().load(std::memory_order_acquire)?&resolvedInteraction:nullptr;

}
}
