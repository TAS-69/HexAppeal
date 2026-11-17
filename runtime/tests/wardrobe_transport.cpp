#include "wardrobe_transport.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
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
    std::puts("PASS: 112-byte wardrobe ABI, temporary reference balance, failure cleanup, original record preservation and complex-record rejection. This fixture does not call the game.");
}
