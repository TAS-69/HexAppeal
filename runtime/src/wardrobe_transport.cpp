#include "wardrobe_transport.h"
#include <cstring>

namespace hex {
namespace {
uintptr_t pointer(const WardrobeRecord& record,size_t offset){uintptr_t value=0;std::memcpy(&value,record.bytes.data()+offset,sizeof value);return value;}
}
bool isSimpleWardrobeRecord(const WardrobeRecord& record,uintptr_t gameBase){
    if(!gameBase||!pointer(record,0)||pointer(record,0)==gameBase+0x6f29f48)return false;


    for(const auto offset:{0x10u,0x18u,0x20u,0x28u,0x30u,0x38u,0x48u,0x50u,0x58u})if(pointer(record,offset))return false;
    uint32_t id=0;std::memcpy(&id,record.bytes.data()+0x68,4);
    return pointer(record,0x60)==gameBase+0x6f14108&&id==0xffffffff;
}
WardrobeCallResult invokeWardrobeSwap(uintptr_t node,const WardrobeRecord& source,uintptr_t target,uintptr_t gameBase,const WardrobeTransport& ops){
    if(!node||!target||target==gameBase+0x6f29f48||!isSimpleWardrobeRecord(source,gameBase))return WardrobeCallResult::InvalidRecord;
    if(!ops.retain||!ops.release||!ops.destroy||!ops.pipeline)return WardrobeCallResult::MissingFunctions;
    WardrobeRemoval removal{pointer(source,0),0};
    auto addition=source;std::memcpy(addition.bytes.data(),&target,sizeof target);
    if(!ops.retain(removal.wrapper))return WardrobeCallResult::RetainFailed;
    if(!ops.retain(target))return ops.release(&removal.wrapper)?WardrobeCallResult::RetainFailed:WardrobeCallResult::CleanupFailed;
    WardrobeVector removeList{&removal,1,1},addList{&addition,1,1};
    const bool called=ops.pipeline(node,&removeList,&addList,0);
    const bool released=ops.release(&removal.wrapper),destroyed=ops.destroy(&addition);
    if(!released||!destroyed)return WardrobeCallResult::CleanupFailed;
    return called?WardrobeCallResult::Completed:WardrobeCallResult::CallFailed;
}
}
