#include "skin_runtime.h"
#include "skin_tone.h"
#include "character_addons.h"
#include "runtime.h"
#include "native_interface.h"
#include "appearance_hooks.h"
#include "core/log.h"
#include <MinHook.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <intrin.h>
#include <memory>
#include <new>
#include <stdexcept>
#include <string_view>

namespace hex {
namespace {
namespace fs=std::filesystem;
struct DmmFile {
    const void* path=nullptr;
    uint64_t size=0;
};
struct DmmPath {
    const char* text;
    uint32_t length;
    int32_t references;
    uint32_t flags,reserved;
};
static_assert(sizeof(DmmPath)==24&&sizeof(DmmFile)==16);
using Lookup=const DmmFile*(*)(const void*);
constexpr size_t lookupOffset=0x9240;
constexpr size_t pathRequestReturn=0x94b1;
constexpr size_t fileCountOffset=0x527f0;
constexpr size_t looseReaderOffset=0x52420;
constexpr size_t looseTableOffset=0x52348;
constexpr const char* dmmDigest="ec0929a7d4697249020ae09704fb96aebdce4db4b201049598045224ec4261af";
constexpr unsigned char lookupPrefix[]{0x48,0x81,0xec,0x88,0,0,0,0x48,0x8b,0x05,0x72,0x7e,0x04,0,0x48,0x33};
constexpr unsigned char lookupTail[]{0xc4,0x48,0x89,0x44,0x24,0x50,0xe8,0x05,0x06,0,0,0x48,0x85,0xc0,0x0f,0x84};

struct CachedFile {
    std::unique_ptr<unsigned char[]> path;
    DmmFile record;
};
struct LocalFile {
    CachedFile main,fallback;
    bool appearance=false,needsRegistry=false;
    int addon=-1;
    bool baseUnderwear=false;
    mutable std::atomic<bool> requested{false};
    mutable std::atomic<bool> fallbackRequested{false};
};
struct Overlay {
    Lookup original=nullptr;
    uintptr_t dmm=0;
    Runtime* runtime=nullptr;
    mutable std::atomic<bool> registryRequested{false};
    std::map<std::string,LocalFile,std::less<>> files;
};
std::atomic<const Overlay*> activeOverlay{nullptr};

bool memoryCopy(const void* source,void* target,size_t bytes){
    __try{std::memcpy(target,source,bytes);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool copyRoute(const void* request,char* route,size_t capacity,size_t& length){
    __try{
        if(!request)return false;
        const auto name=*static_cast<const char* const* const*>(request);
        if(!name||!*name)return false;
        auto* first=*name;
        while(*first=='/'||*first=='\\')++first;
        for(length=0;length+1<capacity;++length){
            auto ch=first[length];
            if(!ch)return true;
            if(ch=='\\')ch='/';
            if(ch>='A'&&ch<='Z')ch+='a'-'A';
            route[length]=ch;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return false;
}
const DmmFile* lookup(const void* request){
    const auto& overlay=*activeOverlay.load(std::memory_order_acquire);
    char route[512];
    size_t length=0;
    if(copyRoute(request,route,sizeof route,length)){
        const std::string_view key(route,length);
        if(const auto found=overlay.files.find(key);found!=overlay.files.end()){
            const auto pathRequest=reinterpret_cast<uintptr_t>(_ReturnAddress())==overlay.dmm+pathRequestReturn;
            if(key==skinRegistryRoute&&pathRequest&&
               !overlay.registryRequested.exchange(true)){
                LOG_OK("Skin tone: the runtime requested the isolated witch skin registry.");
            }
            if(!found->second.needsRegistry||overlay.registryRequested.load()){
                if(pathRequest&&!found->second.requested.exchange(true)){
                    if(found->second.addon>=firstAddonCharacter){
                        recordAddonVenusBody(found->second.addon,found->second.baseUnderwear);
                        LOG_OK("VENUS body loaded for %s.",characterAddon(found->second.addon)->label.c_str());
                    }
                    if(found->second.appearance){
                        ++overlay.runtime->skinToneApplied;
                        LOG_OK("Skin tone: private appearance loaded for %s.",found->first.c_str());
                    }else if(key.rfind("character/descriptors/customization/",0)==0){
                        ++overlay.runtime->skinToneApplied;
                        LOG_OK("Skin tone: customization loaded for %s.",found->first.c_str());
                    }else if(key.find("/head/head/")!=key.npos||key.find("/nude/")!=key.npos||
                             key.rfind("character/texture/hexappeal_skin_",0)==0){
                        LOG_OK("Skin tone: served %s.",found->first.c_str());
                    }
                }
                return &found->second.main.record;
            }
            if(found->second.fallback.record.path){
                if(pathRequest&&!found->second.fallbackRequested.exchange(true)){
                    recordAddonVenusBody(found->second.addon,found->second.baseUnderwear);
                    LOG_OK("VENUS body loaded for %s; waiting for skin registry.",characterAddon(found->second.addon)->label.c_str());
                }
                return &found->second.fallback.record;
            }
        }
    }
    return overlay.original(request);
}

const DmmFile* probeLookup(Lookup original,const void* request){
    __try{return original(request);}
    __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
bool validateRecord(Lookup original,const std::string& route,const fs::path& expected){
    const auto* name=route.c_str();
    const auto* wrapper=&name;
    const auto* result=probeLookup(original,&wrapper);
    DmmFile entry;
    std::error_code ec;
    const auto size=fs::file_size(expected,ec);
    DmmPath path{};
    if(ec||!result||!memoryCopy(result,&entry,sizeof entry)||entry.size!=size||!entry.path||
       !memoryCopy(entry.path,&path,sizeof path)||!path.length||path.length>32767||
       path.references!=-1||path.flags!=0x80000000||path.reserved||
       path.text!=static_cast<const char*>(entry.path)+sizeof(DmmPath))return false;
    std::string utf8(size_t(path.length)+1,'\0');
    if(!memoryCopy(path.text,utf8.data(),utf8.size())||utf8.find('\0')!=path.length)return false;
    const auto normalized=fs::u8path(utf8.c_str()).lexically_normal();
    return CompareStringOrdinal(normalized.c_str(),-1,expected.lexically_normal().c_str(),-1,TRUE)==CSTR_EQUAL;
}
fs::path putInCache(const fs::path& cache,const std::string& route,const std::string& data){
    const auto file=cache/(std::to_string(std::hash<std::string>{}(data))+"-"+fs::path(route).filename().string());
    std::error_code ec;
    const auto existing=fs::file_size(file,ec);
    if(!ec&&existing==data.size()){
        std::ifstream input(file,std::ios::binary);
        std::string bytes(data.size(),'\0');
        if(input.read(bytes.data(),bytes.size())&&bytes==data)return file;
    }
    auto temp=file;
    temp+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
    std::ofstream output(temp,std::ios::binary|std::ios::trunc);
    output.write(data.data(),data.size());
    output.close();
    if(!output||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        fs::remove(temp,ec);
        throw std::runtime_error("Could not prepare the private skin cache.");
    }
    return file;
}
CachedFile cacheFile(const fs::path& directory,const std::string& route,const std::string& data){
    const auto physical=putInCache(directory,route,data).u8string();
    CachedFile output;
    output.path=std::make_unique<unsigned char[]>(sizeof(DmmPath)+physical.size()+1);
    char* name=reinterpret_cast<char*>(output.path.get()+sizeof(DmmPath));
    std::memcpy(name,physical.c_str(),physical.size()+1);
    const auto* header=new(output.path.get()) DmmPath{name,static_cast<uint32_t>(physical.size()),-1,0x80000000,0};
    output.record={header,data.size()};
    return output;
}
DWORD WINAPI prepare(LPVOID location){
    auto& runtime=*static_cast<Runtime*>(location);
    try{
        const auto game=runtime.folder.parent_path();
        auto manifest=runtime.configuredDependencyPaths().manifest;
        if(manifest.empty())manifest=runtime.folder/L"dmm/manifest.tsv";
        else if(manifest.is_relative())manifest=game/manifest;
        bool known=false;
        std::string warning;
        const auto routes=readMountedRoutes(manifest,&known,&warning);
        if(routes.empty())throw std::runtime_error("No readable mounted skin routes were found.");
        if(!known||!warning.empty())LOG("Skin routes will check the active file records from this file list.");
        std::map<std::string,int> addonTargets;
        std::vector<std::string> addonRoutes;
        for(int target:enabledAddonTargets()){
            const auto& route=characterAddon(target)->appearance;
            addonTargets.emplace(route,target);
            addonRoutes.push_back(route);
        }
        const auto addonPlan=prepareAddonVenus(game,routes,addonRoutes);
        SkinToneAssets plan;
        if(runtime.skinToneAtStartup){
            try{
                plan=prepareSkinTone(game,routes,addonPlan.appearances);
                if(plan.files.empty()){
                    runtime.reportSkinToneFailure(plan.source.detail.empty()?"No matching witch skin files could be prepared.":plan.source.detail);
                    LOG("Skin tone: %s",plan.source.detail.c_str());
                }
                for(const auto& reason:plan.skipped)LOG("Skin tone skipped %s",reason.c_str());
            }catch(const std::exception& error){
                runtime.reportSkinToneFailure(error.what());
                LOG("Skin tone: %s",error.what());
            }
        }
        if(!addonPlan.appearances.empty()){
            plan.sources.push_back(venusBodyModelRoute);
            plan.files.insert(addonPlan.appearances.begin(),addonPlan.appearances.end());
        }
        if(plan.files.empty())return 0;
        HMODULE module=nullptr;
        for(unsigned waited=0;waited<150&&!module;++waited){
            module=GetModuleHandleW(L"DmmRuntime.asi");
            if(!module)Sleep(20);
        }
        wchar_t modulePath[32768]{};
        if(!module)throw std::runtime_error("DMM did not load its file router.");
        if(!GetModuleFileNameW(module,modulePath,32768)||sha256(modulePath)!=dmmDigest)
            LOG("Unverified DMM runtime version. Checking the mounted skin file records.");
        const auto base=reinterpret_cast<uintptr_t>(module);
        auto* entry=reinterpret_cast<void*>(base+lookupOffset);
        const auto lookupReady=[&]{
            return executablePluginFunction(module,entry)&&
                appearance::checkedGameEntry(entry,lookupPrefix,16,lookupTail)!=appearance::GameEntry::Unavailable;
        };
        bool validated=false;
        std::string rejectedRoute;
        for(unsigned waited=0;waited<100&&!validated;++waited){
            uintptr_t reader=0;
            size_t count=0;
            if(lookupReady()&&memoryCopy(reinterpret_cast<const void*>(base+looseReaderOffset),&reader,sizeof reader)&&reader==base+looseTableOffset&&
               memoryCopy(reinterpret_cast<const void*>(base+fileCountOffset),&count,sizeof count)&&count>0){
                validated=std::all_of(plan.sources.begin(),plan.sources.end(),[&](const auto& route){
                    const auto sample=routes.find(route);
                    if(sample!=routes.end()&&validateRecord(reinterpret_cast<Lookup>(entry),sample->first,sample->second))return true;
                    rejectedRoute=route;
                    return false;
                });
            }
            if(!validated)Sleep(15);
        }
        if(!validated){
            if(!lookupReady())throw std::runtime_error(appearance::entryIssue(entry,"DMM's mounted file lookup"));
            if(rejectedRoute.empty())throw std::runtime_error("DMM did not initialize its mounted file table in time.");
            throw std::runtime_error("DMM's live file record did not match the mounted skin route "+rejectedRoute+".");
        }
        const auto cache=runtime.profiles.parent_path()/L"Skin cache";
        fs::create_directories(cache);
        auto overlay=std::make_unique<Overlay>();
        overlay->dmm=base;
        overlay->runtime=&runtime;
        for(const auto& [route,data]:plan.files){
            auto& file=overlay->files.try_emplace(route).first->second;
            file.main=cacheFile(cache,route,data);
            file.appearance=std::find(plan.appearances.begin(),plan.appearances.end(),route)!=plan.appearances.end();
            file.needsRegistry=file.appearance&&plan.files.count(skinRegistryRoute);
            if(auto target=addonTargets.find(route);target!=addonTargets.end()&&addonPlan.appearances.count(route)){
                file.addon=target->second;
                file.baseUnderwear=addonPlan.hasBaseUnderwear;
                if(file.needsRegistry)file.fallback=cacheFile(cache,route,addonPlan.appearances.at(route));
            }
        }
        bool attached=false;
        for(unsigned attempt=0;attempt<3&&!attached;++attempt){
            if(attempt)Sleep(120);
            void* original=nullptr;
            if(!lookupReady()||MH_CreateHook(entry,reinterpret_cast<void*>(&lookup),&original)!=MH_OK)continue;
            overlay->original=reinterpret_cast<Lookup>(original);
            activeOverlay.store(overlay.get(),std::memory_order_release);
            attached=MH_EnableHook(entry)==MH_OK;
            if(!attached){
                activeOverlay.store(nullptr,std::memory_order_release);
                MH_RemoveHook(entry);
            }
        }
        if(!attached){
            auto issue=std::string("The isolated skin lookup hook could not start after three attempts.");
            const auto owner=appearance::hookOwnerName(entry);
            if(!owner.empty())issue+=" DMM's file lookup currently routes into "+owner+".";
            throw std::runtime_error(issue);
        }
        overlay.release();
        runtime.skinTonePrepared=static_cast<unsigned>(plan.appearances.size()+plan.tones.size());
        if(!plan.appearances.empty())
            LOG_OK("Skin tone: prepared %zu witch appearances and %zu private body prefabs.",
                plan.appearances.size(),plan.privateBodies);
        if(!plan.tones.empty()){
            const auto brightness=plan.source.brightness?std::to_string(*plan.source.brightness):"original";
            const auto colour=plan.source.colour?std::to_string(*plan.source.colour):"original";
            LOG_OK("Skin tone: prepared %zu witch customizations. Damiane brightness %s, colour %s.",
                plan.tones.size(),brightness.c_str(),colour.c_str());
        }
    }catch(const std::exception& error){
        if(runtime.skinToneAtStartup)runtime.reportSkinToneFailure(error.what());
        LOG("Appearance routes: %s",error.what());
    }
    return 0;
}
}

void startSkinTone(){
    auto& runtime=Runtime::get();
    if((!runtime.skinToneAtStartup&&enabledAddonTargets().empty())||runtime.profiles.empty())return;
    if(const auto thread=CreateThread(nullptr,0,prepare,&runtime,0,nullptr))CloseHandle(thread);
    else if(runtime.skinToneAtStartup)runtime.reportSkinToneFailure("Windows could not start the skin file preparation thread.");
}
}
