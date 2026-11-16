#include "outfit_visibility.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
namespace {
hex::OutfitVisibility* visibility=nullptr;
unsigned writes=0,baselineReads=0;float normal=.7f,last=-1;bool failWrite=false,reentrant=false;
bool intern(const char* name,uint32_t* key){*key=std::string(name)=="MyBra"?1:2;return true;}
bool baseline(void*,uint32_t,float* value){++baselineReads;*value=normal;return true;}
bool apply(void*,uint32_t key,float value){++writes;last=value;if(reentrant)visibility->query(10,20,3,key,normal,1000);return !failWrite;}
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
}
int main(){try{
    hex::OutfitVisibility subject;visibility=&subject;hex::OutfitVisibilityCalls calls{intern,baseline,apply};
    hex::OutfitVisibilityPolicy policy;policy.owned=true;policy.enabled=true;policy.owner=30;policy.prefab="my_dress";policy.parts={{"MyBra",true},{"MyBriefs",true}};
    check(subject.update(10,20,3,policy,1000,calls)&&writes==0,"visible defaults never force naturally hidden parts on");
    check(subject.contains(1)&&!subject.contains(99),"only declared keys participate");
    policy.parts[0].visible=false;reentrant=true;
    check(subject.update(10,20,3,policy,1000,calls)&&writes==1&&near(last,0),"hide uses bounded part write with reentrant query");
    check(near(subject.query(10,20,3,1,.7f,1001),0)&&near(subject.query(10,20,3,2,.4f,1001),.4f),"only the selected part is hidden");
    check(near(subject.query(11,20,3,1,.7f,1001),.7f)&&near(subject.query(10,21,3,1,.7f,1001),.7f)&&near(subject.query(10,20,2,1,.7f,1001),.7f),"actor renderer and witch identity are isolated");
    check(near(subject.query(10,20,3,1,.7f,3000),.7f),"stale policies never override a live query");
    check(subject.update(10,20,3,policy,1100,calls)&&writes==1,"stable hidden part does not flood setters");
    policy.parts[0].visible=true;check(subject.update(10,20,3,policy,1101,calls)&&writes==2&&near(last,.7f),"show restores captured natural value, including fractional visibility");
    normal=0;policy.parts[1].visible=false;
    check(subject.update(10,20,3,policy,1200,calls)&&near(last,0),"naturally hidden part can be masked");
    reentrant=false;policy.parts[1].visible=true;check(subject.update(10,20,3,policy,1201,calls)&&near(last,.4f),"a prior rule query is retained as the baseline");
    hex::OutfitVisibility zero;visibility=&zero;normal=0;policy.parts={{"MyBra",false}};
    check(zero.update(10,20,3,policy,1300,calls),"zero baseline captured");policy.parts[0].visible=true;
    check(zero.update(10,20,3,policy,1301,calls)&&near(last,0),"show never forces a naturally zero baseline to one");
    visibility=&subject;normal=.6f;policy.parts={{"MyBra",false}};
    check(subject.update(10,20,3,policy,1400,calls),"hide before explicit swap restore");
    check(subject.restore(10,20,1401,calls)&&near(last,.7f)&&near(subject.query(10,20,3,1,.8f,1402),.8f),"pre-swap restore clears query override and restores original value");
    check(subject.update(10,20,3,policy,1500,calls),"hide before provider becomes unavailable");
    auto disabled=policy;disabled.enabled=false;
    check(subject.update(10,20,3,disabled,1501,calls)&&near(last,.7f),"unavailable provider restores owned components");
    check(subject.update(10,20,3,policy,1600,calls),"hide before ownership lost");
    auto foreign=policy;foreign.owned=false;const auto before=writes;
    check(subject.update(10,20,3,foreign,1601,calls)&&writes==before&&near(subject.query(10,20,3,1,.9f,1602),.9f),"ownership loss drops policy without touching a custom outfit");
    failWrite=true;check(!subject.update(10,20,3,policy,1700,calls),"failed native setters report failure");
    failWrite=false;check(subject.update(10,20,3,policy,1701,calls),"failed hidden write retries");
    const auto beforeRendererChange=writes;check(subject.restore(10,21,1702,calls)&&writes==beforeRendererChange,"renderer replacement never restores through an old pointer");
    check(!subject.update(0,20,3,policy,1800,calls)&&!subject.update(10,0,3,policy,1800,calls),"invalid live identities fail closed");
    std::atomic<bool> failed{false};reentrant=false;normal=.7f;
    std::thread update([&]{for(unsigned i=0;i<500;++i)if(!subject.update(10,20,3,policy,2000+i,calls))failed=true;});
    std::thread query([&]{for(unsigned i=0;i<500;++i){const float value=subject.query(10,20,3,1,.7f,2000+i);if(value!=0&&!near(value,.7f))failed=true;}});
    update.join();query.join();check(!failed,"concurrent rule queries and updates remain bounded and valid");
    std::cout<<"PASS: outfit part ownership, natural visibility restoration, reentrant queries, stale isolation, provider removal, failed writes and concurrent reads.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
