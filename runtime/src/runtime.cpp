#include "runtime.h"
#include "game_layout.h"
#include "graphics_diagnostics.h"
#include "release_version.h"
#include "core/log.h"
#include "core/settings.h"
#include "core/state.h"
#include <bcrypt.h>
#include <shlobj.h>
#include <TlHelp32.h>
#include <fstream>
#include <vector>
#include <cstdio>
#include <mutex>
#include <cstdarg>
#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>

static std::mutex logMutex;
static std::mutex interfaceSaveMutex;
static std::filesystem::path logPath;
static std::string lastLoggedGraphics;
// First 8 hex digits of this ASI's SHA-256, so a log or report names the exact build.
static std::string buildId;
static std::string buildSuffix(){return buildId.empty()?std::string{}:" (build "+buildId+")";}
static std::string logTime(bool filename=false){
    SYSTEMTIME now{};GetSystemTime(&now);char formatted[32]{};
    snprintf(formatted,sizeof formatted,filename?"%04u%02u%02u-%02u%02u%02u-%03uZ":"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
        unsigned(now.wYear),unsigned(now.wMonth),unsigned(now.wDay),unsigned(now.wHour),unsigned(now.wMinute),unsigned(now.wSecond),unsigned(now.wMilliseconds));
    return formatted;
}
static const char* compatibilityLabel(hex::CompatibilityState state){
    switch(state){case hex::CompatibilityState::Ready:return "Ready";case hex::CompatibilityState::Blocked:return "Blocked";default:return "Waiting";}
}
namespace ml::Log {
void OpenSession(const std::filesystem::path& path)
{
    {
        const std::lock_guard<std::mutex> guard(logMutex);
        logPath = path;
        auto archive = path.parent_path() / path.stem();
        archive += "-" + logTime(true) + "-" + std::to_string(GetCurrentProcessId()) + path.extension().string();
        // Move without replacement so previous sessions remain available.
        MoveFileExW(path.c_str(), archive.c_str(), 0);
        lastLoggedGraphics.clear();
    }
    RefreshGraphics();
}

void Write(const char* level,const char* fmt,...) {
    std::lock_guard<std::mutex> guard(logMutex);
    if(logPath.empty())return;
    std::ofstream file(logPath,std::ios::app);
    if(!file)return;
    char text[2048];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);
    file<<'['<<logTime()<<"] ["<<level<<"] "<<text<<'\n';
}

void RefreshGraphics()
{
    try {
        // The automatic log leaves out the library list; saved reports include it.
        auto summary = hex::graphicsSummary(false);
        const std::lock_guard<std::mutex> guard(logMutex);
        if (logPath.empty() || summary == lastLoggedGraphics) return;
        std::ofstream file(logPath, std::ios::app);
        if (!file) return;
        if (lastLoggedGraphics.empty()) {
            if (file.tellp() > 0) file << '\n';
            file << "Hex Appeal " HEX_APPEAL_BUILD_VERSION " session" << buildSuffix() << "\n" << logTime() << "\n\n";
        } else {
            file << "\n[" << logTime() << "] [info ] Graphics diagnostics updated.\n";
        }
        file << summary << '\n';
        if (file) lastLoggedGraphics = std::move(summary);
    } catch (const std::exception&) {
        // A failed diagnostic read must leave startup and presentation retries available.
    }
}
}
namespace hex {
FeatureCompatibility Runtime::featureCompatibility(Feature feature){std::lock_guard<std::mutex> lock(compatibilityMutex);return effectiveCompatibility(compatibility,feature);}
CompatibilitySnapshot Runtime::compatibilityStatus(){std::lock_guard<std::mutex> lock(compatibilityMutex);auto result=compatibility;for(size_t i=0;i<result.size();++i)result[i]=effectiveCompatibility(compatibility,Feature(i));return result;}
void Runtime::reportCompatibility(Feature feature,CompatibilityState state,const std::string& detail){
    {
        std::lock_guard<std::mutex> lock(compatibilityMutex);
        const auto index=size_t(feature);
        if(index>=compatibility.size()||(compatibility[index].state==state&&compatibility[index].detail==detail))return;
        compatibility[index]={state,detail};
    }
    const auto* level=state==CompatibilityState::Blocked?"error":state==CompatibilityState::Ready?"ok":"info";
    ml::Log::Write(level,"%s / %s: %s",featureName(feature),compatibilityLabel(state),detail.c_str());
}
void Runtime::reportSkinToneFailure(const std::string& detail){
    std::lock_guard<std::mutex> lock(compatibilityMutex);
    skinToneError=detail;
    skinToneFailed=true;
}
std::string Runtime::skinToneFailure(){
    std::lock_guard<std::mutex> lock(compatibilityMutex);
    return skinToneError;
}

std::string Runtime::diagnosticReport(){
    const auto dependency=dependencyStatus();const auto& deps=*dependency;
    const auto checks=compatibilityStatus();
    std::ostringstream out;
    out<<"Hex Appeal " HEX_APPEAL_BUILD_VERSION " diagnostics"<<buildSuffix()<<"\n"<<logTime()<<"\n\n"
       <<graphicsSummary()<<"\nFeature checks\n";
    for(size_t i=0;i<checks.size();++i)out<<featureName(Feature(i))<<" / "<<compatibilityLabel(checks[i].state)<<" — "<<checks[i].detail<<'\n';
    const auto range=outfitRangeMeters.load();
    const auto* layout=loadedGameLayout();
    out<<"Outfit update range / ";
    if(range&&layout&&layout->actorManagerGlobal)out<<range<<" m (loaded actors)\n";
    else out<<"Game default\n";
    out<<"\nDependencies and paths\n";
    const std::pair<const char*,const DependencyFile*> files[]={
        {"Crimson Desert",&deps.game},{"Desert Mod Manager",&deps.manager},{"DMM active file list",&deps.manifest},
        {"VENUS Body for Damiane",&deps.venus},{"Instant Surgery",&deps.instant},{"Hex's Hentai Preset / selected preset",&deps.hex},
        {"Character Creator Enhanced",&deps.creator},{"Two Reasons Body Physics",&deps.physics}};
    for(const auto& [name,file]:files){
        out<<name<<" / "<<(file->valid?(file->verified?"verified":"found"):"unavailable")<<" — "<<file->detail<<'\n';
        if(!file->path.empty())out<<"  "<<file->path.u8string()<<'\n';
    }
    out<<"\nBody and visibility by witch\n"<<deps.bodyDetail<<'\n';
    if(!deps.bodyNote.empty())out<<deps.bodyNote<<'\n';
    out<<"Witch skin match: "<<(skinToneAtStartup?(skinToneFailed.load()?"unavailable":"on"):"off")
       <<" / "<<skinToneApplied.load()<<" of "<<skinTonePrepared.load()<<" skin routes loaded\n";
    if(skinToneFailed){const auto issue=skinToneFailure();if(!issue.empty())out<<"  "<<issue<<'\n';}
    const auto outfitMask=outfitHiddenMask.load(),underwearMask=underwearHiddenMask.load();
    const bool allowExternal=allowExternalUnderwear.load();
    const auto observedExternal=underwearObservedExternalMask.load();
    out<<"Allow external underwear control (all witches): "<<(allowExternal?"on":"off")<<'\n';
    for(size_t i=0;i<witches.size();++i){
        const uint32_t bit=1u<<i;
        out<<witches[i].name<<" / Body "<<(!deps.canShapeBody()||deps.unavailableBodyWitches&bit?"unavailable":"eligible")
            <<" / Hide outfit "<<(outfitMask&bit?"on":"off")<<" / Hide base underwear "<<(underwearMask&bit?"on":"off");
        const auto& source=deps.underwearSources[i];
        const bool external=(observedExternal|deps.confirmedUnderwearRules|deps.missingUnderwearMeshMask)&bit;
        if(!source.empty()||external)
            out<<" / Underwear provider: "<<(source.empty()?"observed game or outfit rule":source)
               <<" ("<<(allowExternal&&external?"external control":external?"Hex Appeal control":"mounted candidate")<<')';
        if(deps.missingUnderwearMeshMask&bit)out<<" / Base mesh removed from mounted assets";
        out<<'\n';
        if(!deps.unavailableBodyDetails[i].empty())out<<"  "<<deps.unavailableBodyDetails[i]<<'\n';
    }
    out<<"\nMounted source folders and direct file counts\n";
    if(deps.mountedMods.empty())out<<"No source folders attributable from the active file list.\n";
    for(const auto& [name,count]:deps.mountedMods)out<<name<<" ("<<count<<")\n";
    out<<"\nRelevant mounted resource routes\n";
    for(const auto& route:deps.relevantRoutes)out<<route.resource<<"\n  "<<(route.sourceName.empty()?"Source unknown":route.sourceName)<<" — "<<route.sourceFile.u8string()<<'\n';
    if(!deps.managerMountLog.empty()){
        out<<"\nLast DMM mount log (may describe a later mount attempt)\n"<<deps.managerMountLog.u8string()<<'\n';
        for(const auto& finding:deps.managerFindings)out<<finding<<'\n';
    }
    out<<"\nLoaded ASI plugins\n";
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
    if(snapshot!=INVALID_HANDLE_VALUE){
        MODULEENTRY32W module{};module.dwSize=sizeof module;
        if(Module32FirstW(snapshot,&module))do{
            const auto path=std::filesystem::path(module.szExePath);
            if(_wcsicmp(path.extension().c_str(),L".asi")==0)out<<path.filename().u8string()<<" — "<<path.u8string()<<'\n';
        }while(Module32NextW(snapshot,&module));
        CloseHandle(snapshot);
    }else out<<"Plugin enumeration unavailable (Windows error "<<GetLastError()<<").\n";
    out<<"\nLast editor message\n"<<model.message<<"\n"<<native.shapingDetail<<'\n';
    return out.str();
}

std::string Runtime::recentLog(){
    std::lock_guard<std::mutex> guard(logMutex);
    if(logPath.empty())return {};
    std::ifstream input(logPath,std::ios::binary|std::ios::ate);
    if(!input)return {};
    const auto end=input.tellg();
    if(end<=0)return {};
    constexpr std::streamoff limit=32768;
    const auto start=std::max<std::streamoff>(0,std::streamoff(end)-limit);
    input.seekg(start);
    std::string tail((std::istreambuf_iterator<char>(input)),{});
    if(start&&tail.find('\n')!=std::string::npos)tail.erase(0,tail.find('\n')+1);
    return tail;
}

bool Runtime::saveDiagnosticReport(std::filesystem::path& path,std::string& error){
    if(profiles.empty()){error="The Hex Appeal settings folder is unavailable.";return false;}
    const auto report=profiles.parent_path()/("HexAppeal-report-"+logTime(true)+".txt");
    const auto file=CreateFileW(report.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){error="The report could not be created (Windows error "+std::to_string(GetLastError())+").";return false;}
    const auto contents=diagnosticReport()+"\nRecent runtime log\n"+recentLog();
    DWORD written=0;
    const bool success=WriteFile(file,contents.data(),DWORD(contents.size()),&written,nullptr)&&written==contents.size();
    CloseHandle(file);
    if(!success){DeleteFileW(report.c_str());error="The report could not be written.";return false;}
    path=report;return true;
}
Runtime& Runtime::get(){static Runtime r;return r;}
namespace {
struct DigestFile {
    HANDLE handle;
    ~DigestFile(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
};
struct DigestIdentity {
    FILE_ID_INFO id{};
    LONGLONG bytes=0,modified=0,changed=0;
    bool operator==(const DigestIdentity& other) const {
        return bytes==other.bytes&&modified==other.modified&&changed==other.changed&&
            id.VolumeSerialNumber==other.id.VolumeSerialNumber&&
            std::memcmp(id.FileId.Identifier,other.id.FileId.Identifier,sizeof id.FileId.Identifier)==0;
    }
};
bool digestIdentity(HANDLE handle,DigestIdentity& identity){
    FILE_BASIC_INFO times{};
    LARGE_INTEGER bytes{};
    if(!GetFileInformationByHandleEx(handle,FileIdInfo,&identity.id,sizeof identity.id)||
       !GetFileInformationByHandleEx(handle,FileBasicInfo,&times,sizeof times)||
       !GetFileSizeEx(handle,&bytes)||!times.ChangeTime.QuadPart)return false;
    identity.bytes=bytes.QuadPart;
    identity.modified=times.LastWriteTime.QuadPart;
    identity.changed=times.ChangeTime.QuadPart;
    return true;
}
}
std::string sha256(const std::filesystem::path& path,bool reuseUnchanged) {
    std::error_code ec;auto file=std::filesystem::absolute(path,ec).lexically_normal().make_preferred();
    if(ec)return {};
    const auto native=file.wstring();
    if(native.rfind(L"\\\\?\\",0)!=0){
        file=native.rfind(L"\\\\",0)==0?std::filesystem::path(L"\\\\?\\UNC\\"+native.substr(2)):std::filesystem::path(L"\\\\?\\"+native);
    }
    const auto openFile=[&](DWORD sharing){
        return CreateFileW(file.c_str(),GENERIC_READ,sharing,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    };
    const auto stable=reuseUnchanged?openFile(FILE_SHARE_READ):INVALID_HANDLE_VALUE;
    const DigestFile input{stable!=INVALID_HANDLE_VALUE?stable:openFile(FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE)};
    constexpr LONGLONG maxBytes=4ll*1024*1024*1024;
    LARGE_INTEGER size{};
    if(input.handle==INVALID_HANDLE_VALUE||!GetFileSizeEx(input.handle,&size)||size.QuadPart<0||size.QuadPart>maxBytes)return {};
    static thread_local std::map<std::filesystem::path,std::pair<DigestIdentity,std::string>> digests;
    DigestIdentity before;
    const bool canReuse=stable!=INVALID_HANDLE_VALUE&&digestIdentity(input.handle,before)&&before.bytes==size.QuadPart;
    if(canReuse){
        const auto cached=digests.find(file);
        if(cached!=digests.end()&&cached->second.first==before)return cached->second.second;
    }
    std::vector<unsigned char> data(static_cast<size_t>(std::clamp<LONGLONG>(size.QuadPart,1,1024*1024)));
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    unsigned char hash[32]{};BCRYPT_HASH_HANDLE context=nullptr;
    NTSTATUS result=BCryptCreateHash(algorithm,&context,nullptr,0,nullptr,0,0);
    if(result>=0) {
        LONGLONG total=0;
        for(;;){
            DWORD count=0;
            if(!ReadFile(input.handle,data.data(),DWORD(data.size()),&count,nullptr)||total+count>maxBytes){result=-1;break;}
            if(!count)break;
            total+=count;
            if((result=BCryptHashData(context,data.data(),count,0))<0)break;
        }
        if(result>=0&&total==size.QuadPart)result=BCryptFinishHash(context,hash,32,0);else result=-1;
        BCryptDestroyHash(context);
    }
    BCryptCloseAlgorithmProvider(algorithm,0);
    if(result<0)return {};
    std::string output;output.reserve(64);char part[3];
    for(auto value:hash){snprintf(part,sizeof(part),"%02x",value);output+=part;}
    if(canReuse){
        DigestIdentity after;
        if(!digestIdentity(input.handle,after)||!(before==after))return {};
        if(digests.size()>=64)digests.clear();
        digests.insert_or_assign(file,std::make_pair(after,output));
    }
    return output;
}
int callMenu(void* fn,unsigned command,unsigned key) {
    if(!fn)return -1;
    __try {return reinterpret_cast<int(*)(unsigned,unsigned)>(fn)(command,key);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return -1;}
}
static std::filesystem::path modulePath(HMODULE module) {
    wchar_t path[32768]{};const auto n=GetModuleFileNameW(module,path,32768);
    return n&&n<32768?std::filesystem::path(path):std::filesystem::path();
}
void Runtime::initialize(HMODULE self) {
    folder=modulePath(self).parent_path();
    wchar_t local[MAX_PATH]{};
    if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local))) {
        profiles=std::filesystem::path(local)/L"HexAppeal"/L"profiles.ini";
        setGraphicsSettingsPath(std::filesystem::path(local)/L"Pearl Abyss"/L"CD"/L"save"/L"user_engine_option_save.xml");
        std::error_code ec;std::filesystem::create_directories(profiles.parent_path(),ec);
        buildId=sha256(modulePath(self),false).substr(0,8);
        if(!ec)ml::Log::OpenSession(profiles.parent_path()/L"HexAppeal.log");
    }
    const auto ini=folder/L"HexAppeal.ini";
    const auto read=[&](const wchar_t* name,int def){return GetPrivateProfileIntW(L"General",name,def,ini.c_str());};
    key=int(read(L"ActivationKeyVK",VK_F9));if(key<1||key>255)key=VK_F9;
    control=read(L"Control",0)!=0;alt=read(L"Alt",0)!=0;shift=read(L"Shift",0)!=0;
    if(key==72&&control&&alt&&!shift){key=VK_F9;control=false;alt=false;}
    if(!profiles.empty()) {
        const auto ui=profiles.parent_path()/L"interface.ini";
        loadInterface(ui);
        skinToneAtStartup=skinToneEnabled;
        std::string pathError;
        if(!loadDependencyPaths(profiles.parent_path()/L"paths.ini",dependencyPaths,pathError))model.message=pathError;
    }
    ml::Settings::Get().hookXInput=read(L"HookXInput",1)!=0;
    ml::Settings::Get().wrapSwapChain=int(read(L"WrapSwapChain",0));
    std::string error;
    if(!profiles.empty()&&!model.load(profiles,error))model.message="Saved selections could not be read: "+error;
    if(!profiles.empty()&&std::filesystem::exists(profiles)&&error.empty())queuedMask=63u;
    ml::State::Get().blockGameInput=blockGameInput;
    if(logPath.empty())OutputDebugStringW(L"Hex Appeal: could not create the LocalAppData log.\n");
    LOG("Hex Appeal " HEX_APPEAL_BUILD_VERSION " initialized. Overlay shortcut VK=%d Ctrl=%d Alt=%d Shift=%d.",
        key,int(control),int(alt),int(shift));
}
void Runtime::loadInterface(const std::filesystem::path& ui) {
    const auto value=[&](const wchar_t* name,int fallback){return int(GetPrivateProfileIntW(L"Interface",name,fallback,ui.c_str()));};
    if(std::filesystem::exists(ui)) {key=value(L"ActivationKeyVK",key);if(key<1||key>255)key=VK_F9;control=false;alt=false;shift=false;}
    compact=value(L"Compact",0)!=0;livePreview=value(L"LivePreview",0)!=0;
    skinToneEnabled=value(L"MatchCustomDamianeSkin",0)!=0;
    blockGameInput=value(L"BlockGameInput",1)!=0;quickPins=uint32_t(value(L"QuickPins",PinUnderwear|PinOutfit))&AllQuickPins;
    uiScalePercent=std::clamp(value(L"ScalePercent",100),minimumUiScalePercent,maximumUiScalePercent);underwearHiddenMask=uint32_t(value(L"UnderwearHiddenMask",0))&63u;
    const auto legacyTakeover=uint32_t(value(L"UnderwearTakeoverMask",0))&63u;
    allowExternalUnderwear=value(L"AllowExternalUnderwear",legacyTakeover==63u?0:1)!=0;
    outfitHiddenMask=uint32_t(value(L"OutfitHiddenMask",value(L"FullNudeMask",0)))&63u;
    outfitRangeMeters=clampOutfitRange(value(L"OutfitRangeMeters",defaultOutfitRange));
    ml::State::Get().blockGameInput=blockGameInput;
}
static HMODULE pinnedInstant=nullptr;
static void* instantMenu=nullptr;
void Runtime::refreshNative() {
    if(preview)return;
    static DWORD last=0;const auto now=GetTickCount();if(last&&now-last<1500)return;last=now;
    const auto module=GetModuleHandleW(L"InstantSurgery.asi");
    native.instantLoaded=module!=nullptr;
    if(module&&module!=pinnedInstant) {
        native.instantSupported=GetProcAddress(module,"ModHubMenuV1")!=nullptr;
        if(native.instantSupported) {
            auto fn=GetProcAddress(module,"ModHubMenuV1");
            HMODULE retained=nullptr;
            if(fn&&GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(fn),&retained)) {pinnedInstant=retained;instantMenu=reinterpret_cast<void*>(fn);}
        }
    }
    native.instantReady=native.instantSupported&&instantMenu&&callMenu(instantMenu,0,0)==1;
    native.detail=!module?"Instant Surgery is not loaded.":!native.instantSupported?"Instant Surgery's editor interface could not be found.":!native.instantReady?"Instant Surgery is loaded; its editor is not ready yet.":"Instant Surgery editor is ready.";
    if(const auto factory=providerFactory.load())native.backend=factory(native.shapingDetail);
}
bool Runtime::openInstant() {
    refreshNative();
    if(!native.instantReady) {model.message=native.detail;return false;}
    if(callMenu(instantMenu,3,0)!=1) {model.message="Instant Surgery did not accept the editor request.";return false;}
    model.message="Instant Surgery opened for the active player. Witch selections were not applied.";
    return true;
}
bool Runtime::save() {
    if(profiles.empty()){model.message="The profile folder is unavailable.";return false;}
    Model candidate=model;candidate.saveDraft(~unavailableBodyWitches.load());std::string error;
    if(!candidate.save(profiles,error)){model.message=error;return false;}
    model=candidate;model.message="Selection saved. No body or outfit change was applied.";return true;
}
void Runtime::selectionChanged() {
    if(!model.draft.valid())return;
    const auto mask=(model.target<0?63u:1u<<model.target)&~unavailableBodyWitches.load();
    if(!mask)return;
    model.saveDraft(mask);queuedMask|=mask;
    savePending=true;autoError=false;nextApplyAt=0;
}
void Runtime::requestShapeReapply(int witch) {
    if(witch>=0&&witch<int(witches.size()))requestedRefitMask.fetch_or(1u<<witch);
}
void Runtime::tickAuto(uint64_t now,bool flush) {
    refreshNative();
    if(const auto refit=requestedRefitMask.exchange(0)){queuedMask|=refit;nextApplyAt=0;}
    if(presetChanged.exchange(false)){
        for(size_t i=0;i<model.saved.size();++i){
            const auto& profile=model.saved[i];
            if((profile.top==Preset::Hex&&profile.topBlend>0)||(profile.bottom==Preset::Hex&&profile.bottomBlend>0))queuedMask|=1u<<i;
        }
        nextApplyAt=0;
    }
    const auto pending=model.pendingTransaction;model.poll(native.backend);
    if(pending&&!model.pendingTransaction&&model.lastApplyFailed){queuedMask|=model.pendingMask;nextApplyAt=now+1000;autoError=true;}
    if(savePending&&((flush&&!autoError)||now>=nextSaveAt)) {
        std::string error;
        if(profiles.empty()||!model.save(profiles,error)){model.message=error.empty()?"Cannot save profiles":error;autoError=true;nextSaveAt=now+1000;return;}
        savePending=false;nextSaveAt=now+120;autoError=false;
    }
    if(savePending||!queuedMask||model.pendingTransaction||now<nextApplyAt)return;
    std::string error;auto mask=queuedMask&~unavailableBodyWitches.load();nextApplyAt=now+500;
    if(!mask)return;
    const auto dependency=dependencyStatus();
    if(dependency->scanned){
        for(size_t i=0;i<model.saved.size();++i)if(mask&(1u<<i)){
            const auto issue=profileDependencyIssue(model.saved[i],shapingAvailable,hexAvailable,weightAvailable);
            if(!issue.empty()){mask&=~(1u<<i);if(error.empty())error=dependency->game.valid&&dependency->instant.valid&&dependency->canShapeBody()&&!native.shapingDetail.empty()?native.shapingDetail:issue;}
        }
        if(!mask){model.message=error;autoError=false;return;}
    }
    if(model.applySaved(native.backend,mask,error)){queuedMask&=~mask;autoError=false;nextApplyAt=now+80;}
    else {model.message=error;autoError=validBackend(native.backend);}
    model.applied=model.draftIsApplied(~unavailableBodyWitches.load());
}
bool Runtime::saveInterface() {
    std::lock_guard<std::mutex> guard(interfaceSaveMutex);
    if(profiles.empty())return false;
    const auto path=profiles.parent_path()/L"interface.ini";
    const auto temp=profiles.parent_path()/L"interface.ini.tmp";
    std::ofstream file(temp,std::ios::binary|std::ios::trunc);
    if(!file){model.message="Interface preferences could not be saved.";return false;}
    file<<"[Interface]\nVersion=1\nActivationKeyVK="<<key<<"\nScalePercent="<<uiScalePercent
        <<"\nCompact="<<compact<<"\nLivePreview="<<livePreview<<"\nBlockGameInput="<<blockGameInput<<"\nQuickPins="<<quickPins<<"\nUnderwearHiddenMask="<<underwearHiddenMask.load()
        <<"\nAllowExternalUnderwear="<<allowExternalUnderwear.load()<<"\nOutfitHiddenMask="<<outfitHiddenMask.load()
        <<"\nOutfitRangeMeters="<<outfitRangeMeters.load()
        <<"\nMatchCustomDamianeSkin="<<skinToneEnabled<<"\n";
    file.flush();const bool written=bool(file);file.close();
    if(!written||!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        model.message="Interface preferences could not be saved.";return false;
    }
    return true;
}
bool Runtime::saveUnderwearPreference(uint32_t mask,std::string& error) {
    return saveVisibilityPreference("UnderwearHiddenMask",mask,error);
}
bool Runtime::saveOutfitHiddenPreference(uint32_t mask,std::string& error) {
    return saveVisibilityPreference("OutfitHiddenMask",mask,error);
}
bool Runtime::saveVisibilityPreference(const char* key,uint32_t mask,std::string& error) {
    std::lock_guard<std::mutex> guard(interfaceSaveMutex);
    if(profiles.empty()){error="The settings folder is unavailable.";return false;}
    const auto path=profiles.parent_path()/L"interface.ini";
    const auto temp=profiles.parent_path()/L"interface.ini.underwear.tmp";
    std::error_code ec;const bool exists=std::filesystem::exists(path,ec);
    if(ec){error="Interface preferences could not be read.";return false;}
    std::string pending;bool inInterface=false,inserted=false;
    const auto trim=[](const std::string& text){const auto first=text.find_first_not_of(" \t\r");return first==std::string::npos?std::string{}:text.substr(first,text.find_last_not_of(" \t\r")-first+1);};
    const auto row=std::string(key)+'='+std::to_string(mask&63u)+"\n";
    if(exists){
        const auto bytes=std::filesystem::file_size(path,ec);if(ec||bytes>1048576){error="Interface preferences are too large or unreadable.";return false;}
        std::ifstream input(path,std::ios::binary);if(!input){error="Interface preferences could not be read.";return false;}
        std::string line;bool first=true;
        while(std::getline(input,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.find('\0')!=std::string::npos){error="Interface preferences use an unsupported encoding.";return false;}
            auto text=trim(line);if(first&&text.compare(0,3,"\xef\xbb\xbf")==0)text=trim(text.substr(3));first=false;
            const auto close=text.find(']');
            if(!text.empty()&&text[0]=='['&&close!=std::string::npos){
                inInterface=_stricmp(trim(text.substr(1,close-1)).c_str(),"Interface")==0;
                pending+=line+'\n';if(inInterface&&!inserted){pending+=row;inserted=true;}continue;
            }
            const auto equal=text.find('=');
            if(inInterface&&equal!=std::string::npos&&_stricmp(trim(text.substr(0,equal)).c_str(),key)==0)continue;
            pending+=line+'\n';
        }
        if(input.bad()){error="Interface preferences could not be read completely.";return false;}
    }
    if(!inserted)pending+="[Interface]\nVersion=1\n"+row;
    std::ofstream output(temp,std::ios::binary|std::ios::trunc);output<<pending;output.flush();const bool written=bool(output);output.close();
    if(!written||!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        error="The visibility preference could not be saved atomically.";return false;
    }
    return true;
}
DependencyPaths Runtime::configuredDependencyPaths(){std::lock_guard<std::mutex> guard(dependencyMutex);return dependencyPaths;}
std::shared_ptr<const DependencyStatus> Runtime::dependencyStatus(){
    std::lock_guard<std::mutex> guard(dependencyMutex);
    return dependencies;
}
bool Runtime::setDependencyPaths(const DependencyPaths& paths,std::string& error){
    if(profiles.empty()){error="The settings folder is unavailable.";return false;}
    if(!saveDependencyPaths(profiles.parent_path()/L"paths.ini",paths,error))return false;
    {std::lock_guard<std::mutex> guard(dependencyMutex);dependencyPaths=paths;}
    ++dependencyRevision;nextApplyAt=0;return true;
}
void Runtime::publishDependencies(const DependencyStatus& status){
    auto snapshot=std::make_shared<const DependencyStatus>(status);
    {
        std::lock_guard<std::mutex> guard(dependencyMutex);
        snapshot.swap(dependencies);
        unavailableBodyWitches=status.unavailableBodyWitches;
        dependencyScanned=status.scanned;
        const auto mounted=status.confirmedUnderwearRules|status.missingUnderwearMeshMask;
        const bool ownersChanged=underwearMountedExternalMask.exchange(mounted)!=mounted;
        const bool meshesChanged=underwearMissingMeshMask.exchange(status.missingUnderwearMeshMask)!=status.missingUnderwearMeshMask;
        if(ownersChanged||meshesChanged)++underwearVersion;
    }
}
}
