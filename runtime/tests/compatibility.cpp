#include "runtime.h"
#include "native_interface.h"
#include <iostream>
#include <stdexcept>
#include <thread>

extern "C" __declspec(dllexport) int CompatibilityFixture(int value){return value+37;}
extern "C" {__declspec(dllexport) int CompatibilityData=7;}
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){try{
    using namespace hex;
    Runtime runtime;
    check(runtime.featureCompatibility(Feature::Shaping).allows(true),"initial check permits file-backed configuration");
    runtime.reportCompatibility(Feature::Shaping,CompatibilityState::Ready,"Native checks passed for an unverified version.");
    check(runtime.featureCompatibility(Feature::Shaping).allows(true),"working unknown version remains usable");
    check(!runtime.featureCompatibility(Feature::Shaping).allows(false),"a previous success cannot override missing files");
    runtime.reportCompatibility(Feature::Hex,CompatibilityState::Blocked,"Unknown preset key: Shape.Missing");
    check(!runtime.featureCompatibility(Feature::Hex).allows(true),"readable but incompatible preset is blocked");
    check(runtime.featureCompatibility(Feature::Shaping).allows(true),"preset failure does not disable base shapes");
    check(runtime.featureCompatibility(Feature::Hex).detail=="Unknown preset key: Shape.Missing","actual failure reason retained");
    runtime.reportCompatibility(Feature::Hex,CompatibilityState::Ready,"Updated external preset parsed.");
    check(runtime.featureCompatibility(Feature::Hex).allows(true),"fixed dependency recovers after a passing probe");
    runtime.reportCompatibility(Feature::Shaping,CompatibilityState::Blocked,"Native interface missing.");
    check(runtime.featureCompatibility(Feature::Wardrobe).state==CompatibilityState::Blocked&&runtime.featureCompatibility(Feature::Underwear).detail.find("Native interface missing")!=std::string::npos,"uninitialized dependent features explain their upstream block");
    runtime.reportCompatibility(Feature::Shaping,CompatibilityState::Ready,"Native checks passed.");
    check(runtime.featureCompatibility(Feature::Wardrobe).state==CompatibilityState::Waiting,"upstream recovery resumes pending feature checks");
    runtime.reportCompatibility(Feature::Count,CompatibilityState::Ready,"invalid");
    check(runtime.featureCompatibility(Feature::Count).state==CompatibilityState::Blocked,"invalid feature cannot grant capability");
    std::thread writer([&]{for(int i=0;i<2000;++i)runtime.reportCompatibility(Feature::Wardrobe,i%2?CompatibilityState::Ready:CompatibilityState::Blocked,i%2?"ready":"blocked");});
    for(int i=0;i<2000;++i){const auto value=runtime.compatibilityStatus()[size_t(Feature::Wardrobe)];check(value.state==CompatibilityState::Waiting||(value.state==CompatibilityState::Ready&&value.detail=="ready")||(value.state==CompatibilityState::Blocked&&value.detail=="blocked"),"feature snapshot is atomic");}
    writer.join();
    const auto module=GetModuleHandleW(nullptr);std::string error;
    const auto address=pluginExport(module,"CompatibilityFixture",error);
    check(address&&reinterpret_cast<int(*)(int)>(address)(5)==42,"named versioned export resolves independently of an RVA");
    check(!pluginExport(module,"MissingFixture",error)&&error.find("MissingFixture")!=std::string::npos,"missing export gives a specific blocking reason");
    check(!pluginExport(module,"CompatibilityData",error)&&error.find("not executable")!=std::string::npos,"data export cannot be called");
    check(!executablePluginFunction(GetModuleHandleW(L"kernel32.dll"),reinterpret_cast<const void*>(address)),"foreign owner rejected");
    check(!executablePluginFunction(module,reinterpret_cast<const void*>(1)),"unreadable pointer rejected");
    check(!pluginExport(nullptr,"CompatibilityFixture",error),"missing module rejected");
    for(int i=1;i<argc;++i){const auto plugin=LoadLibraryExW(argv[i],nullptr,DONT_RESOLVE_DLL_REFERENCES);check(plugin!=nullptr,"map isolated installed plugin copy");const char* symbol=i==1?"InstantSurgeryDeformPoseV1":"TwoReasonsEditorPoseAppliedV1";check(pluginExport(plugin,symbol,error)!=nullptr,"installed versioned export has executable module ownership");FreeLibrary(plugin);}
    std::cout<<"PASS: capability recovery, unknown-version use, isolated blocking reasons, atomic snapshots and executable named exports.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
