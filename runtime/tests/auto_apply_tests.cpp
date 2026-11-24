#include "runtime.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>
void checkInterfacePreferences(const std::filesystem::path& profiles);
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static uint64_t sequence=0;static int pollState=0,commits=0;static std::vector<HexAppealProfileV1> supplied;
static int ready(char*,uint32_t){return 1;}
static int begin(uint32_t,uint64_t* id){*id=++sequence;supplied.clear();return 1;}
static int prepare(uint64_t,const HexAppealTargetV1*,const HexAppealProfileV1* profile){supplied.push_back(*profile);return 1;}
static int commit(uint64_t){++commits;return 1;}
static void abortTx(uint64_t){}
static int poll(uint64_t,char*,uint32_t){return pollState;}
static HexAppealBackendV1 backend{sizeof(HexAppealBackendV1),HEX_APPEAL_BACKEND_ABI,255,"TEST ONLY - no game deformation",ready,begin,prepare,commit,abortTx,poll};
int wmain(int argc,wchar_t** argv){try{
    if(argc==3&&std::wstring_view(argv[1])==L"--interface-preferences"){
        checkInterfacePreferences(argv[2]);
        std::cout<<"PASS: default input blocking, legacy migration, independent visibility, pins, outfit range and graphics diagnostics.\n";
        return 0;
    }
    require(argc==2,"profile path");
    for(int i=0;i<9;++i){int held=-1;float x=float(i%3-1)+.006f,y=float(i/3-1);hex::snapShapePad(x,y,500,held);require(held==i&&x==float(i%3-1)&&y==float(i/3-1),"all nine combinations have a two-pixel detent");x+=.01f;hex::snapShapePad(x,y,500,held);require(held==i,"small detent hysteresis prevents jitter");x+=.016f;hex::snapShapePad(x,y,500,held);require(held<0,"detents release after three pixels");}
    {int held=-1;float x=0.f,y=.39f;hex::snapShapePad(x,y,500,held);require(held<0&&y==.39f,"no sticky bands between the nine detents");}
    for(float x:{-1.f,-.61f,0.f,.37f,1.f})for(float y:{-1.f,-.42f,0.f,.83f,1.f}){
        hex::Profile p;hex::moveShapePad(p,x,y);require(std::fabs(hex::regionCoordinate(p.bottom,p.bottomBlend)-x)<1e-6f&&std::fabs(hex::regionCoordinate(p.top,p.topBlend)-y)<1e-6f,"pad coordinates must round-trip independently");
        require(p.height==0&&p.weight==0,"pad preserves neutral independent height and weight");p.height=.3f;p.weight=.8f;hex::moveShapePad(p,-x,-y);require(p.height==.3f&&p.weight==.8f,"pad does not overwrite independent proportions");
    }
    auto& r=hex::Runtime::get();r.preview=true;r.native.backend=&backend;r.profiles=argv[1];r.model.selectTarget(0);
    r.model.draft.top=hex::Preset::Hex;r.model.draft.topBlend=.42f;r.selectionChanged();r.tickAuto(10000,true);
    require(commits==1&&r.model.pendingTransaction&&supplied.size()==1&&supplied[0].topBlend==.42f,"edit saves and starts native transaction without Apply");
    hex::Model disk;std::string error;require(disk.load(r.profiles,error)&&disk.saved[0].topBlend==.42f,"auto save is durable");
    for(int i=1;i<=20;++i){r.model.draft.bottom=hex::Preset::Flat;r.model.draft.bottomBlend=float(i)/20;r.selectionChanged();r.tickAuto(10000+i*5);}
    r.tickAuto(10101,true);require(commits==1&&r.model.saved[0].bottomBlend==1,"drag edits coalesce behind the pending transaction");
    pollState=1;r.tickAuto(10200);require(commits==2&&supplied.size()==1&&supplied[0].bottomBlend==1&&supplied[0].topBlend==.42f,"latest edit reaches backend after previous acknowledgement");
    r.tickAuto(10300);require(r.model.draftIsApplied(),"old acknowledgement cannot mark a newer selection active");
    r.model.selectTarget(3);r.model.draft.bottom=hex::Preset::Hex;r.model.draft.bottomBlend=.23f;r.selectionChanged();
    r.model.selectTarget(1);r.model.draft.top=hex::Preset::Flat;r.selectionChanged();r.tickAuto(10400,true);
    require(supplied.size()==2,"per-witch edits survive navigation and batch together");
    require(disk.load(r.profiles,error)&&disk.saved[3].bottomBlend==.23f&&disk.saved[0].topBlend==.42f,"auto save preserves other witches");
    const auto legacy=r.profiles.parent_path()/L"legacy.ini";std::ofstream out(legacy);out<<"[HexAppeal]\nVersion=1\n[Lyselia]\nTop=1\nTopBlend=0.42\n";out.close();
    require(disk.load(legacy,error)&&disk.saved[0].height==0&&disk.saved[0].weight==0&&disk.saved[0].topBlend==.42f,"legacy profiles keep neutral independent proportions");
    r.tickAuto(10500,true);r.queuedMask=3;r.model.saved[0]=hex::Profile{};r.model.saved[0].top=hex::Preset::Hex;r.model.saved[1]=hex::Profile{};
    hex::DependencyStatus dependencies;dependencies.scanned=true;r.publishDependencies(dependencies);r.shapingAvailable=true;r.hexAvailable=false;r.weightAvailable=false;
    r.tickAuto(10600,true);require(supplied.size()==1&&supplied[0].top==0&&r.queuedMask==1,"missing Hex holds that witch while a valid witch applies");
    r.hexAvailable=true;r.tickAuto(10700,true);require(supplied.size()==1&&supplied[0].top==1&&r.queuedMask==0,"held profile resumes after dependency returns");
    r.queuedMask=1;r.shapingAvailable=false;const auto before=commits;r.tickAuto(10800,true);require(commits==before&&r.queuedMask==1,"missing base preserves queue without native apply");
    r.shapingAvailable=true;r.queuedMask=0;pollState=0;
    const auto savedBefore=r.model.saved;std::ifstream oldFile(r.profiles,std::ios::binary);const std::string fileBefore((std::istreambuf_iterator<char>(oldFile)),{});oldFile.close();
    const auto refitsBefore=commits;r.requestShapeReapply(3);r.requestShapeReapply(3);r.requestShapeReapply(-1);r.requestShapeReapply(6);r.tickAuto(10900);
    require(commits==refitsBefore+1&&supplied.size()==1&&supplied[0].bottomBlend==.23f,"final outfit assembly requests one fresh fitting transaction using its saved shape");
    r.requestShapeReapply(0);r.requestShapeReapply(0);r.tickAuto(10910);require(commits==refitsBefore+1&&r.queuedMask==1,"refits wait behind an unacknowledged transaction and coalesce by witch");
    pollState=1;r.tickAuto(10920);require(commits==refitsBefore+2&&supplied.size()==1&&supplied[0].top==1,"next fitting request proceeds after acknowledgement");
    r.tickAuto(11000);std::ifstream newFile(r.profiles,std::ios::binary);const std::string fileAfter((std::istreambuf_iterator<char>(newFile)),{});newFile.close();
    require(r.model.saved==savedBefore&&fileAfter==fileBefore,"outfit refits preserve every saved profile and do not rewrite settings");
    r.weightAvailable=true;r.model.saved[0].weight=1;r.queuedMask=1;r.tickAuto(11500,true);
    require(supplied.size()==1&&supplied[0].weight==1&&r.queuedMask==0&&r.dependencyStatus()->scanned,"saved Weight applies without an installed outfit source");
    r.tickAuto(11600,true);
    dependencies.unavailableBodyWitches=1;r.publishDependencies(dependencies);
    const auto replacedProfile=r.model.saved[0];
    r.model.selectTarget(-1);r.model.draft=hex::Profile{};r.model.draft.height=.03f;
    r.selectionChanged();r.tickAuto(11700,true);
    require(supplied.size()==5&&r.model.saved[0]==replacedProfile&&r.model.saved[1].height==.03f,
        "All witches saves and applies to the five supported bodies, preserving the replaced witch profile");
    r.tickAuto(11800,true);
    require(r.model.draftIsApplied(~r.unavailableBodyWitches.load()),"the supported selection can settle as applied");
    r.requestShapeReapply(0);r.tickAuto(11900,true);
    require(r.queuedMask==1&&supplied.size()==5,"a blocked reapply stays queued without touching the replacement body");
    dependencies.unavailableBodyWitches=0;r.publishDependencies(dependencies);r.tickAuto(12500,true);
    require(r.queuedMask==0&&supplied.size()==1&&supplied[0].weight==replacedProfile.weight,"the original profile resumes when the supported body returns");
    std::cout<<"PASS: nine light detents, continuous pad mapping, independent proportions, automatic persistence/application, drag coalescing, cross-witch batches, legacy migration and per-profile dependency hold/resume.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
