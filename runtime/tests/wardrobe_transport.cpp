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
    WardrobeNudeCycle cycle;cycle.removing(100);check(!cycle.observe(true,true,101)&&cycle.pending(),"Hide outfit waits until all requested parts are absent");
    check(cycle.observe(false,false,102)&&cycle.phase==WardrobeNudeCycle::Nude,"removal is acknowledged from observed absence");cycle.restoring(103);
    check(!cycle.observe(true,false,104)&&cycle.pending(),"partial outfit restoration is not completion");check(cycle.observe(true,true,105)&&cycle.phase==WardrobeNudeCycle::Dressed,"all original outfit parts must return before releasing the baseline");
    cycle.removing(1000);cycle.observe(true,true,6000);check(cycle.phase==WardrobeNudeCycle::Failed,"unacknowledged removal times out instead of repeatedly writing");
    std::puts("PASS: wardrobe swaps, remove/add transactions, original metadata, bounded references, failed-call cleanup and observed Hide outfit restoration.");
}
