#include "wardrobe_transport.h"
#include "wardrobe_nude.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <Windows.h>
static constexpr uintptr_t base=0x140000000,oldWrapper=0x10000,newWrapper=0x20000,node=0x30000;
static std::map<uintptr_t,int> refs;
static int calls=0,retainCalls=0,failRetainAt=0;
static bool failCall=false;
static uintptr_t expectedRemoval=oldWrapper,expectedAddition=newWrapper;
static void check(bool condition,const char* label){if(!condition){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
static uintptr_t word(const hex::WardrobeRecord& record,size_t offset){uintptr_t value=0;std::memcpy(&value,record.bytes.data()+offset,8);return value;}
static void put(hex::WardrobeRecord& record,size_t offset,uintptr_t value){std::memcpy(record.bytes.data()+offset,&value,8);}
static bool retain(uintptr_t p){if(++retainCalls==failRetainAt)return false;++refs[p];return true;}
static bool release(uintptr_t* p){--refs[*p];*p=base+0x6f29f48;return true;}
static bool destroy(hex::WardrobeRecord* p){auto wrapper=word(*p,0);return release(&wrapper);}
static bool pipeline(uintptr_t actor,hex::WardrobeVector* remove,hex::WardrobeVector* add,uint8_t flag){
    ++calls;check(actor==node&&flag==0,"exact node and flag forwarded");
    check(remove->count==1&&remove->capacity==1&&add->count==1&&add->capacity==1,"native vector layout");
    const auto* removed=static_cast<hex::WardrobeRemoval*>(remove->data);const auto* record=static_cast<hex::WardrobeRecord*>(add->data);
    check(removed->wrapper==expectedRemoval&&removed->metadata==0&&word(*record,0)==expectedAddition,"selected removal and target addition");
    check(record->bytes[8]==1&&record->bytes[9]==255&&word(*record,0x60)==base+0x6f14108,"default metadata and sentinel preserved");
    check(refs[oldWrapper]==3&&refs[newWrapper]==3,"both temporaries retained before the native call");return !failCall;
}
static void reset(){refs={{oldWrapper,2},{newWrapper,2}};calls=retainCalls=failRetainAt=0;failCall=false;expectedRemoval=oldWrapper;expectedAddition=newWrapper;}
static bool attachBatch=true;
static bool batch(uintptr_t actor,hex::WardrobeVector* remove,hex::WardrobeVector* add,uint8_t flag){
    ++calls;
    check(actor==node&&flag==0,"batch forwards exact actor and flag");
    const auto* used=attachBatch?add:remove;
    const auto* unused=attachBatch?remove:add;
    check(used->data&&used->count==2&&used->capacity==2&&!unused->data&&!unused->count&&!unused->capacity,"batch touches only its selected direction");
    if(attachBatch){
        const auto* rows=static_cast<hex::WardrobeRecord*>(used->data);
        check(word(rows[0],0)==oldWrapper&&word(rows[1],0)==newWrapper&&rows[1].bytes[9]==255,"attached originals keep exact record metadata");
    }else{
        const auto* rows=static_cast<hex::WardrobeRemoval*>(used->data);
        check(rows[0].wrapper==oldWrapper&&rows[1].wrapper==newWrapper&&!rows[0].metadata&&!rows[1].metadata,"detach includes only the original wrappers");
    }
    check(refs[oldWrapper]==3&&refs[newWrapper]==3,"all batch records are retained through the call");
    return !failCall;
}
static bool copyRecipe(hex::WardrobeRecord* destination,const hex::WardrobeRecord* source){
    if(!retain(word(*source,0)))return false;
    *destination=*source;
    return true;
}
static bool recipe(uintptr_t actor,hex::WardrobeVector* remove,hex::WardrobeVector* add,uint8_t flag){
    ++calls;
    check(actor==node&&!flag&&remove->count==1&&add->count==1,"recipe removes the live original and adds the saved piece together");
    const auto* removals=static_cast<const hex::WardrobeRemoval*>(remove->data);
    const auto* records=static_cast<const hex::WardrobeRecord*>(add->data);
    check(removals[0].wrapper==oldWrapper&&word(records[0],0)==newWrapper&&word(records[0],0x18)==0x245000,"recipe carries native appearance metadata without substituting the wrapper");
    check(refs[oldWrapper]==3&&refs[newWrapper]==3,"recipe owns the native copies through dispatch");
    return !failCall;
}
static size_t wantedAdditions=1;
static bool edits(uintptr_t actor,const hex::WardrobeVector* baseline,hex::WardrobeVector* remove,hex::WardrobeVector* add,uint8_t flag){
    ++calls;check(actor==node&&!flag,"Hide outfit forwards the checked node and flags");
    check(baseline&&baseline->count==2,"the exact ready-list baseline accompanies a rebuild");
    check(remove->count<=2&&add->count<=2&&remove->capacity==remove->count&&add->capacity==add->count,"remove-only and add-only native vectors stay bounded");
    check(add->count==wantedAdditions,"rebuild submits the complete desired body and outfit, excluding removed clothes");
    check((remove->count?remove->data!=nullptr:remove->data==nullptr)&&(add->count?add->data!=nullptr:add->data==nullptr),"empty native vectors have no dangling data");
    for(uint32_t i=0;i<remove->count;++i){const auto& item=static_cast<hex::WardrobeRemoval*>(remove->data)[i];check(!item.metadata&&refs[item.wrapper]==3,"each removed garment has a retained temporary");}
    for(uint32_t i=0;i<add->count;++i){const auto& item=static_cast<hex::WardrobeRecord*>(add->data)[i];check(refs[word(item,0)]==3&&hex::isSimpleWardrobeRecord(item,base),"restoration uses validated retained original records");}
    return !failCall;
}
static int guardMode=0;
static bool guardedPipeline(uintptr_t actor,hex::WardrobeVector*,hex::WardrobeVector*,uint8_t){
    ++calls;check(*reinterpret_cast<const uint32_t*>(actor+0x48)==0,"the retained source list cannot silently reintroduce removed clothing");
    if(guardMode==2)RaiseException(0xe0123456,0,0,nullptr);return guardMode==0;
}
int main(){
    using namespace hex;WardrobeRecord record;put(record,0,oldWrapper);put(record,0x60,base+0x6f14108);put(record,0x68,0xffffffff);record.bytes[8]=1;record.bytes[9]=255;
    const auto original=record.bytes;const WardrobeTransport ops{retain,release,destroy,pipeline};reset();
    check(isSimpleWardrobeRecord(record,base),"observed plain baseline accepted");
    check(invokeWardrobeSwap(node,record,newWrapper,base,ops)==WardrobeCallResult::Completed,"native call completion reported");
    check(calls==1&&refs[oldWrapper]==2&&refs[newWrapper]==2&&record.bytes==original,"temporary refs balanced and source untouched");
    reset();expectedRemoval=newWrapper;expectedAddition=oldWrapper;auto restoration=record;put(restoration,0,newWrapper);
    check(invokeWardrobeSwap(node,restoration,oldWrapper,base,ops)==WardrobeCallResult::Completed,"restore original using retained baseline metadata and live replacement removal");
    check(calls==1&&refs[oldWrapper]==2&&refs[newWrapper]==2&&record.bytes==original,"restoration balances references without modifying baseline");
    reset();failCall=true;check(invokeWardrobeSwap(node,record,newWrapper,base,ops)==WardrobeCallResult::CallFailed,"call failure differs from completion");
    check(refs[oldWrapper]==2&&refs[newWrapper]==2,"failure still releases both temporaries");
    reset();failRetainAt=2;check(invokeWardrobeSwap(node,record,newWrapper,base,ops)==WardrobeCallResult::RetainFailed,"failed target retain rejects call");
    check(calls==0&&refs[oldWrapper]==2&&refs[newWrapper]==2,"partial retain rolled back");
    for(size_t offset:{0x10u,0x18u,0x20u,0x28u,0x30u,0x38u,0x48u,0x50u,0x58u}){
        auto complex=record;put(complex,offset,1);reset();
        check(invokeWardrobeSwap(node,complex,newWrapper,base,ops)==WardrobeCallResult::InvalidRecord&&calls==0&&retainCalls==0,"owned metadata or arrays never shallow-copied");
    }
    reset();auto missing=ops;missing.destroy=nullptr;
    check(invokeWardrobeSwap(node,record,newWrapper,base,missing)==WardrobeCallResult::MissingFunctions&&retainCalls==0,"missing cleanup refuses call before ownership changes");
    auto body=record;put(body,0,newWrapper);const std::vector<WardrobeRecord> baseline{record,body};const WardrobeTransport editOps{retain,release,destroy,nullptr,edits};
    const WardrobeTransport batchOps{retain,release,destroy,batch};
    for(const bool attach:{true,false}){
        attachBatch=attach;reset();
        check(invokeWardrobePartBatch(node,baseline,attach,base,batchOps)==WardrobeCallResult::Completed&&calls==1&&refs[oldWrapper]==2&&refs[newWrapper]==2,"original batch balances both references");
        reset();failRetainAt=2;
        check(invokeWardrobePartBatch(node,baseline,attach,base,batchOps)==WardrobeCallResult::RetainFailed&&!calls&&refs[oldWrapper]==2,"partial batch retain rolls back before dispatch");
        reset();failCall=true;
        check(invokeWardrobePartBatch(node,baseline,attach,base,batchOps)==WardrobeCallResult::CallFailed&&refs[oldWrapper]==2&&refs[newWrapper]==2,"failed original batch releases all retained records");
    }
    reset();check(invokeWardrobePartBatch(node,{record,record},true,base,batchOps)==WardrobeCallResult::InvalidRecord&&!retainCalls,"duplicate original root rejected before retaining");
    auto saved=body;put(saved,0x18,0x245000);const auto savedBytes=saved.bytes;
    const WardrobeTransport recipeOps{retain,release,destroy,recipe,nullptr,copyRecipe};
    reset();check(invokeWardrobeRecipe(node,{oldWrapper},{&saved},base,recipeOps)==WardrobeCallResult::Completed&&calls==1&&refs[oldWrapper]==2&&refs[newWrapper]==2&&saved.bytes==savedBytes,"recipe uses its native clone and balances references");
    reset();failRetainAt=2;check(invokeWardrobeRecipe(node,{oldWrapper},{&saved},base,recipeOps)==WardrobeCallResult::RetainFailed&&!calls&&refs[oldWrapper]==2,"failed recipe clone rolls back before dispatch");
    reset();failCall=true;check(invokeWardrobeRecipe(node,{oldWrapper},{&saved},base,recipeOps)==WardrobeCallResult::CallFailed&&refs[oldWrapper]==2&&refs[newWrapper]==2,"failed recipe call releases both temporaries");
    reset();check(invokeWardrobeRecipe(node,{oldWrapper,oldWrapper},{&saved},base,recipeOps)==WardrobeCallResult::InvalidRecord&&!retainCalls,"recipe rejects duplicate removals before retaining");
    reset();wantedAdditions=1;check(invokeWardrobeRebuild(node,baseline,{record},{},base,editOps)==WardrobeCallResult::Completed&&calls==1&&refs[oldWrapper]==2&&refs[newWrapper]==2,"removal keeps the body in the rebuilt assembly and balances every reference");
    reset();wantedAdditions=2;check(invokeWardrobeRebuild(node,baseline,{}, {record},base,editOps)==WardrobeCallResult::Completed&&calls==1&&refs[oldWrapper]==2&&refs[newWrapper]==2&&record.bytes==original,"restoration deduplicates the retained source record and balances references");
    reset();failRetainAt=2;check(invokeWardrobeRebuild(node,baseline,{record},{},base,editOps)==WardrobeCallResult::RetainFailed&&calls==0&&refs[oldWrapper]==2,"partial Hide outfit retention rolls back without an engine call");
    reset();failCall=true;check(invokeWardrobeRebuild(node,baseline,{}, {record},base,editOps)==WardrobeCallResult::CallFailed&&refs[oldWrapper]==2&&refs[newWrapper]==2,"failed restoration still releases temporary references");
    reset();check(invokeWardrobeRebuild(node,baseline,{},{},base,editOps)==WardrobeCallResult::InvalidRecord&&invokeWardrobeRebuild(node,baseline,std::vector<WardrobeRecord>(9,record),{},base,editOps)==WardrobeCallResult::InvalidRecord&&calls==0,"empty and oversized wardrobe edits are refused");
    reset();auto conflict=record;conflict.bytes[8]^=1;check(invokeWardrobeRebuild(node,baseline,{}, {conflict},base,editOps)==WardrobeCallResult::InvalidRecord&&calls==0&&retainCalls==0,"conflicting restoration metadata cannot replace a newer live record");
    auto complexBody=body;put(complexBody,0x18,1);check(invokeWardrobeRebuild(node,{record,complexBody},{record},{},base,editOps)==WardrobeCallResult::InvalidRecord&&retainCalls==0,"unknown owned source fields block rebuilding instead of shallow copying");
    std::array<uint64_t,16> nativeNode{};const auto address=reinterpret_cast<uintptr_t>(nativeNode.data());nativeNode[0x40/8]=reinterpret_cast<uintptr_t>(baseline.data());nativeNode[0x48/8]=uint64_t(2)|(uint64_t(2)<<32);const auto originalNode=nativeNode;
    WardrobeVector nativeReady{const_cast<WardrobeRecord*>(baseline.data()),2,2},empty{};
    for(guardMode=0;guardMode<3;++guardMode){calls=0;check(rebuildWardrobePipeline(address,&nativeReady,&empty,&empty,0,guardedPipeline)==(guardMode==0)&&calls==1&&nativeNode==originalNode,"source pointer, size, capacity and records are restored after success, failure and an engine exception");}
    auto changed=baseline;changed[0].bytes[9]^=1;nativeReady.data=changed.data();calls=0;check(!rebuildWardrobePipeline(address,&nativeReady,&empty,&empty,0,guardedPipeline)&&calls==0&&nativeNode==originalNode,"a changed source baseline rejects the rebuild before any write");
    WardrobeNudeCycle cycle;
    cycle.removing(100);
    auto observation=cycle.observe(true,true,true,101);
    check(!observation.acknowledged&&!observation.matchesRequest&&cycle.pending(),"Hide outfit waits until all requested parts are absent");
    observation=cycle.observe(true,true,false,102);
    check(!observation.acknowledged&&observation.matchesRequest&&cycle.pending(),
        "turning Hide outfit off confirms the intact outfit and continues watching the submitted removal");
    observation=cycle.observe(true,false,false,5100);
    check(!observation.acknowledged&&!observation.matchesRequest&&observation.firstDelay&&cycle.pending(),
        "partial removal reports the delay and cannot confirm the changed request");
    check(!cycle.observe(true,false,false,6000).firstDelay,"the same delayed removal reports once");
    observation=cycle.observe(false,false,false,7000);
    check(observation.acknowledged&&!observation.matchesRequest&&cycle.phase==WardrobeNudeCycle::Nude,
        "a late removal settles before the latest show request can be dispatched");
    observation=cycle.observe(true,false,true,7050);
    check(!observation.matchesRequest&&!cycle.pending()&&cycle.retainsRemovedOutfit(),
        "pieces reattached externally can be masked while the original removed outfit stays retained");
    cycle.restoring(7100);
    observation=cycle.observe(false,false,true,7101);
    check(!observation.acknowledged&&observation.matchesRequest&&cycle.pending(),
        "turning Hide outfit on again acknowledges the currently hidden outfit and continues watching the restoration");
    check(cycle.observe(true,false,true,12100).firstDelay,"a new pending restoration reports its own delay");
    observation=cycle.observe(true,true,true,12500);
    check(observation.acknowledged&&!observation.matchesRequest&&cycle.phase==WardrobeNudeCycle::Dressed,
        "late full restoration settles before the latest hide request is applied");
    std::puts("PASS: wardrobe swap transactions, bounded references, failed-call cleanup and latest Hide outfit intent through delayed native acknowledgements.");
}
