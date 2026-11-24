#include "model.h"
#include <iostream>
#include <limits>
#include <fstream>
#include <set>
#include <stdexcept>

static void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
static int began,prepared,committed,aborted,failAt;
static bool commitOK=true,readyOK=true;
static int pollState=1;
static const char* pollReason="";
static std::set<uint32_t> seen;
static int ready(char* reason,uint32_t cap){if(!readyOK&&cap>20)strcpy_s(reason,cap,"Not ready for NPCs.");return readyOK;}
static int begin(uint32_t count,uint64_t* txn){began=int(count);*txn=123;return 1;}
static int prepare(uint64_t txn,const HexAppealTargetV1* target,const HexAppealProfileV1* profile){require(txn==123,"transaction");require(profile->size==sizeof(*profile),"profile ABI");require(target->size==sizeof(*target),"target ABI");++prepared;for(uint32_t i=0;i<target->keyCount;++i)seen.insert(target->characterRecordKeys[i]);return prepared!=failAt;}
static int commit(uint64_t txn){require(txn==123,"commit token");++committed;return commitOK;}
static void abortTx(uint64_t txn){require(txn==123,"abort token");++aborted;}
static int pollTx(uint64_t txn,char* reason,uint32_t cap){require(txn==123,"poll token");if(cap)strcpy_s(reason,cap,pollReason);return pollState;}
static HexAppealBackendV1 backend={sizeof(HexAppealBackendV1),HEX_APPEAL_BACKEND_ABI,255,"TEST ONLY - no game deformation",ready,begin,prepare,commit,abortTx,pollTx};
static void reset(){began=prepared=committed=aborted=failAt=0;commitOK=readyOK=true;pollState=1;pollReason="";backend.capabilities=255;seen.clear();}
int main(int argc,char** argv){try{
    require(argc==2,"output path argument");const auto path=std::filesystem::path(argv[1]);
    hex::Model m;std::string err;
    std::set<std::pair<uint32_t,uint32_t>> combos;
    for(int top=0;top<3;++top)for(int bottom=0;bottom<3;++bottom){require(m.selectCombination(top,bottom),"select combo");combos.emplace(uint32_t(m.draft.top),uint32_t(m.draft.bottom));}
    require(combos.size()==9,"all combinations distinct");
    const auto before=m.draft;require(!m.selectCombination(3,0)&&m.draft==before,"invalid combination rejected");
    require(!m.apply(nullptr,err)&&!m.applied,"no backend cannot apply");
    m.draft.topBlend=std::numeric_limits<float>::quiet_NaN();require(!m.apply(&backend,err),"NaN rejected");m.draft.topBlend=1;
    for(int i=0;i<6;++i){m.selectTarget(i);m.selectCombination(i%3,(i/2)%3);m.draft.topBlend=.75f;m.saveDraft();}
    require(m.save(path,err),"profile save");hex::Model loaded;require(loaded.load(path,err),"profile load");require(m.saved==loaded.saved,"round trip preserves six choices");
    {std::ofstream out(path);out<<"[HexAppeal]\nVersion=1\n[Lyselia]\nTop=99\n";}
    const auto untouched=loaded.saved;require(!loaded.load(path,err)&&loaded.saved==untouched,"corrupt profile does not replace state");
    m.selectTarget(-1);m.selectCombination(1,2);m.draft.autoFit=true;
    reset();require(m.apply(&backend,err)&&m.applied,"valid contract acknowledgement");require(began==6&&prepared==6&&committed==1&&aborted==0,"six-witch batch committed once");require(seen.size()==8&&seen.count(1001091)&&seen.count(1001374),"Bari variants included");
    reset();failAt=3;require(!m.apply(&backend,err)&&!m.applied&&aborted==1&&committed==0,"prepare failure aborts whole batch");
    reset();commitOK=false;require(!m.apply(&backend,err)&&aborted==1&&!m.applied,"commit failure aborts");
    reset();commitOK=false;pollState=-1;pollReason="Saved shapes will apply when the selected witches are visible.";
    require(!m.apply(&backend,err)&&aborted==1&&!m.pendingTransaction&&err==pollReason,
            "explain an offscreen deferral without acknowledging an unrendered shape");
    reset();backend.capabilities&=~HEX_CAP_FLAT_BOUNDED;require(!m.apply(&backend,err)&&began==0,"unbounded Flat rejected before mutation");
    reset();backend.capabilities&=~HEX_CAP_HEX_12_RESOLVED;require(!m.apply(&backend,err)&&began==0,"unresolved Hex macros rejected");
    reset();backend.capabilities&=~HEX_CAP_COORDINATED_BODY_OUTFIT;require(!m.apply(&backend,err)&&began==0,"auto-fit requires coordinated body and outfit update");
    reset();readyOK=false;require(!m.apply(&backend,err)&&began==0,"not-ready provider rejected");
    reset();m.target=7;require(!m.apply(&backend,err)&&began==0,"invalid target rejected");
    reset();m.target=0;pollState=0;require(m.apply(&backend,err)&&!m.applied&&m.pendingTransaction==123,"queued application is not reported as completed");
    require(!m.apply(&backend,err),"overlapping application rejected");
    m.selectCombination(0,0);pollState=1;m.poll(&backend);require(!m.applied&&!m.pendingTransaction,"old completion cannot mark a new selection applied");
    pollState=0;require(m.apply(&backend,err)&&!m.applied,"new queued selection");
    pollState=1;m.poll(&backend);require(m.applied&&!m.pendingTransaction,"render completion acknowledgement");
    hex::Model drafts;drafts.selectCombination(1,2);drafts.draft.topBlend=.63f;const auto all=drafts.draft;
    drafts.selectTarget(0);drafts.selectCombination(2,1);drafts.draft.bottomBlend=.27f;const auto lyselia=drafts.draft;
    drafts.selectTarget(3);drafts.selectCombination(0,2);drafts.selectTarget(0);require(drafts.draft==lyselia,"switching witches preserves unsaved regional edits");
    drafts.selectTarget(-1);require(drafts.draft==all,"all-witch draft has its own edits");
    drafts.selectTarget(0);drafts.restoreSaved();require(drafts.draft==drafts.saved[0]&&drafts.draftIsSaved(),"restore only the selected saved profile");
    std::cout<<"PASS: combinations, persistence, corrupt input, dependency capabilities, batch commit/rollback, per-witch draft retention and saved-profile restore.\nTEST PROVIDER ONLY: no NPC or outfit deformation exercised.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
