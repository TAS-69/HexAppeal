#include "runtime.h"
#include "release_version.h"
#include "core/log.h"
#include "core/settings.h"
#include "core/state.h"
#include <bcrypt.h>
#include <shlobj.h>
#include <fstream>
#include <vector>
#include <cstdio>
#include <mutex>
#include <cstdarg>
#include <algorithm>

static std::mutex logMutex;
static std::mutex interfaceSaveMutex;
static std::filesystem::path logPath;
namespace ml::Log {
void Write(const char* level,const char* fmt,...) {
    if(logPath.empty())return;
    std::lock_guard<std::mutex> guard(logMutex);
    std::ofstream file(logPath,std::ios::app);
    if(!file)return;
    char text[2048];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);
    file<<'['<<level<<"] "<<text<<'\n';
}
}
namespace hex {
FeatureCompatibility Runtime::featureCompatibility(Feature feature){std::lock_guard<std::mutex> lock(compatibilityMutex);return effectiveCompatibility(compatibility,feature);}
CompatibilitySnapshot Runtime::compatibilityStatus(){std::lock_guard<std::mutex> lock(compatibilityMutex);auto result=compatibility;for(size_t i=0;i<result.size();++i)result[i]=effectiveCompatibility(compatibility,Feature(i));return result;}
void Runtime::reportCompatibility(Feature feature,CompatibilityState state,const std::string& detail){std::lock_guard<std::mutex> lock(compatibilityMutex);const auto index=size_t(feature);if(index<compatibility.size())compatibility[index]={state,detail};}
Runtime& Runtime::get(){static Runtime r;return r;}
std::string sha256(const std::filesystem::path& path) {
    std::error_code ec;auto file=std::filesystem::absolute(path,ec).lexically_normal().make_preferred();
    if(ec)return {};
    const auto native=file.wstring();
    if(native.rfind(L"\\\\?\\",0)!=0){
        file=native.rfind(L"\\\\",0)==0?std::filesystem::path(L"\\\\?\\UNC\\"+native.substr(2)):std::filesystem::path(L"\\\\?\\"+native);
    }
    const auto bytes=std::filesystem::file_size(file,ec);
    if(ec||bytes>4ull*1024*1024*1024)return {};
    std::ifstream in(file,std::ios::binary);if(!in)return {};
    std::vector<unsigned char> data(1024*1024,0);
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    unsigned char hash[32]{};BCRYPT_HASH_HANDLE context=nullptr;
    NTSTATUS result=BCryptCreateHash(algorithm,&context,nullptr,0,nullptr,0,0);
    if(result>=0) {
        while(in) {
            in.read(reinterpret_cast<char*>(data.data()),std::streamsize(data.size()));
            const auto count=in.gcount();
            if(count&&(result=BCryptHashData(context,data.data(),ULONG(count),0))<0)break;
        }
        if(result>=0&&!in.bad())result=BCryptFinishHash(context,hash,32,0);else result=-1;
        BCryptDestroyHash(context);
    }
    BCryptCloseAlgorithmProvider(algorithm,0);
    if(result<0)return {};
    std::string output;char part[3];
    for(auto value:hash){snprintf(part,sizeof(part),"%02x",value);output+=part;}
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
        std::error_code ec;std::filesystem::create_directories(profiles.parent_path(),ec);
        if(!ec)logPath=profiles.parent_path()/L"HexAppeal.log";
    }
    const auto ini=folder/L"HexAppeal.ini";
    const auto read=[&](const wchar_t* name,int def){return GetPrivateProfileIntW(L"General",name,def,ini.c_str());};
    key=int(read(L"ActivationKeyVK",VK_F9));if(key<1||key>255)key=VK_F9;
    control=read(L"Control",0)!=0;alt=read(L"Alt",0)!=0;shift=read(L"Shift",0)!=0;
    if(key==72&&control&&alt&&!shift){key=VK_F9;control=false;alt=false;}
    if(!profiles.empty()) {
        const auto ui=profiles.parent_path()/L"interface.ini";
        loadInterface(ui);
        std::string pathError;
        if(!loadDependencyPaths(profiles.parent_path()/L"paths.ini",dependencyPaths,pathError))model.message=pathError;
    }
    ml::Settings::Get().hookXInput=read(L"HookXInput",1)!=0;
    ml::Settings::Get().wrapSwapChain=int(read(L"WrapSwapChain",0));
    std::string error;
    if(!profiles.empty()&&!model.load(profiles,error))model.message="Saved selections could not be read: "+error;
    if(!profiles.empty()&&std::filesystem::exists(profiles)&&error.empty())queuedMask=63u;
    ml::State::Get().blockGameInput=blockGameInput;
    LOG("Hex Appeal " HEX_APPEAL_BUILD_VERSION " initialized. Saved shapes restore automatically when a selected witch is visible.");
}
void Runtime::loadInterface(const std::filesystem::path& ui) {
    const auto value=[&](const wchar_t* name,int fallback){return int(GetPrivateProfileIntW(L"Interface",name,fallback,ui.c_str()));};
    if(std::filesystem::exists(ui)) {key=value(L"ActivationKeyVK",key);if(key<1||key>255)key=VK_F9;control=false;alt=false;shift=false;}
    compact=value(L"Compact",0)!=0;livePreview=value(L"LivePreview",0)!=0;
    blockGameInput=value(L"BlockGameInput",1)!=0;quickPins=uint32_t(value(L"QuickPins",PinUnderwear|PinOutfit))&AllQuickPins;
    uiScalePercent=std::clamp(value(L"ScalePercent",100),80,130);underwearHiddenMask=uint32_t(value(L"UnderwearHiddenMask",0))&63u;
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
        const auto path=modulePath(module);
        const auto digest=sha256(path);
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
    const auto adapter=GetModuleHandleW(L"HexAppeal.InstantSurgeryAdapter.asi");
    if(adapter&&!native.backend) {
        const auto fn=reinterpret_cast<HexAppealGetBackendV1Fn>(GetProcAddress(adapter,"HexAppealGetBackendV1"));
        if(fn) {
            const auto api=fn();
            if(validBackend(api)) {
                HMODULE retained=nullptr;
                if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(fn),&retained))native.backend=api;
            }
        }
    }
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
    Model candidate=model;candidate.saveDraft();std::string error;
    if(!candidate.save(profiles,error)){model.message=error;return false;}
    model=candidate;model.message="Selection saved. No body or outfit change was applied.";return true;
}
void Runtime::selectionChanged() {
    if(!model.draft.valid())return;
    model.saveDraft();queuedMask|=model.target<0?63u:1u<<model.target;
    savePending=true;autoError=false;nextApplyAt=0;
}
void Runtime::requestShapeReapply(int witch) {
    if(witch>=0&&witch<int(witches.size()))requestedRefitMask.fetch_or(1u<<witch);
}
void Runtime::tickAuto(uint64_t now,bool flush) {
    refreshNative();
    if(const auto refit=requestedRefitMask.exchange(0)){queuedMask|=refit;nextApplyAt=0;}
    const auto pending=model.pendingTransaction;model.poll(native.backend);
    if(pending&&!model.pendingTransaction&&model.lastApplyFailed){queuedMask|=model.pendingMask;nextApplyAt=now+1000;autoError=true;}
    if(savePending&&((flush&&!autoError)||now>=nextSaveAt)) {
        std::string error;
        if(profiles.empty()||!model.save(profiles,error)){model.message=error.empty()?"Cannot save profiles":error;autoError=true;nextSaveAt=now+1000;return;}
        savePending=false;nextSaveAt=now+120;autoError=false;
    }
    if(savePending||!queuedMask||model.pendingTransaction||now<nextApplyAt)return;
    std::string error;auto mask=queuedMask;nextApplyAt=now+500;
    const auto dependency=dependencyStatus();
    if(dependency.scanned){
        for(size_t i=0;i<model.saved.size();++i)if(mask&(1u<<i)){
            const auto issue=profileDependencyIssue(model.saved[i],shapingAvailable,hexAvailable,weightAvailable);
            if(!issue.empty()){mask&=~(1u<<i);if(error.empty())error=dependency.game.valid&&dependency.instant.valid&&dependency.venus.valid&&dependency.bodyVerified&&!native.shapingDetail.empty()?native.shapingDetail:issue;}
        }
        if(!mask){model.message=error;autoError=false;return;}
    }
    if(model.applySaved(native.backend,mask,error)){queuedMask&=~mask;autoError=false;nextApplyAt=now+80;}
    else {model.message=error;autoError=validBackend(native.backend);}
    model.applied=model.draftIsApplied();
}
bool Runtime::saveInterface() {
    std::lock_guard<std::mutex> guard(interfaceSaveMutex);
    if(profiles.empty())return false;
    const auto path=profiles.parent_path()/L"interface.ini";
    const auto temp=profiles.parent_path()/L"interface.ini.tmp";
    std::ofstream file(temp,std::ios::binary|std::ios::trunc);
    if(!file){model.message="Interface preferences could not be saved.";return false;}
    file<<"[Interface]\nVersion=1\nActivationKeyVK="<<key<<"\nScalePercent="<<uiScalePercent
        <<"\nCompact="<<compact<<"\nLivePreview="<<livePreview<<"\nBlockGameInput="<<blockGameInput<<"\nQuickPins="<<quickPins<<"\nUnderwearHiddenMask="<<underwearHiddenMask.load()<<"\n";
    file.flush();const bool written=bool(file);file.close();
    if(!written||!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        model.message="Interface preferences could not be saved.";return false;
    }
    return true;
}
bool Runtime::saveUnderwearPreference(uint32_t mask,std::string& error) {
    std::lock_guard<std::mutex> guard(interfaceSaveMutex);
    if(profiles.empty()){error="The settings folder is unavailable.";return false;}
    const auto path=profiles.parent_path()/L"interface.ini";
    const auto temp=profiles.parent_path()/L"interface.ini.underwear.tmp";
    std::error_code ec;const bool exists=std::filesystem::exists(path,ec);
    if(ec){error="Interface preferences could not be read.";return false;}
    std::string pending;bool inInterface=false,inserted=false;
    const auto trim=[](const std::string& text){const auto first=text.find_first_not_of(" \t\r");return first==std::string::npos?std::string{}:text.substr(first,text.find_last_not_of(" \t\r")-first+1);};
    const auto row="UnderwearHiddenMask="+std::to_string(mask&63u)+"\n";
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
            if(inInterface&&equal!=std::string::npos&&_stricmp(trim(text.substr(0,equal)).c_str(),"UnderwearHiddenMask")==0)continue;
            pending+=line+'\n';
        }
        if(input.bad()){error="Interface preferences could not be read completely.";return false;}
    }
    if(!inserted)pending+="[Interface]\nVersion=1\n"+row;
    std::ofstream output(temp,std::ios::binary|std::ios::trunc);output<<pending;output.flush();const bool written=bool(output);output.close();
    if(!written||!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        error="The underwear preference could not be saved atomically.";return false;
    }
    return true;
}
DependencyPaths Runtime::configuredDependencyPaths(){std::lock_guard<std::mutex> guard(dependencyMutex);return dependencyPaths;}
DependencyStatus Runtime::dependencyStatus(){std::lock_guard<std::mutex> guard(dependencyMutex);return dependencies;}
bool Runtime::setDependencyPaths(const DependencyPaths& paths,std::string& error){
    if(profiles.empty()){error="The settings folder is unavailable.";return false;}
    if(!saveDependencyPaths(profiles.parent_path()/L"paths.ini",paths,error))return false;
    {std::lock_guard<std::mutex> guard(dependencyMutex);dependencyPaths=paths;}
    ++dependencyRevision;nextApplyAt=0;return true;
}
void Runtime::publishDependencies(const DependencyStatus& status){std::lock_guard<std::mutex> guard(dependencyMutex);dependencies=status;dependencyScanned=status.scanned;}
}
