#include "wardrobe_transport.h"
#include "game_layout.h"
#include <algorithm>
#include <cstring>
#include <Windows.h>

namespace hex {
namespace {
uintptr_t pointer(const WardrobeRecord& record,size_t offset){uintptr_t value=0;std::memcpy(&value,record.bytes.data()+offset,sizeof value);return value;}
}
bool isSimpleWardrobeRecord(const WardrobeRecord& record,uintptr_t gameBase){
    if(!gameBase||!pointer(record,0)||pointer(record,0)==gameBase+gameLayout().emptyWrapper)return false;


    for(const auto offset:{0x10u,0x18u,0x20u,0x28u,0x30u,0x38u,0x48u,0x50u,0x58u})if(pointer(record,offset))return false;
    uint32_t id=0;std::memcpy(&id,record.bytes.data()+0x68,4);
    return pointer(record,0x60)==gameBase+gameLayout().emptyRecord&&id==0xffffffff;
}
bool rebuildWardrobePipeline(uintptr_t node,const WardrobeVector* ready,WardrobeVector* removals,WardrobeVector* additions,uint8_t flag,decltype(WardrobeTransport::pipeline) pipeline){
    if(!node||!ready||!ready->data||!ready->count||ready->count>64||!pipeline)return false;
    bool called=false,restored=false;
    __try{
        const auto data=*reinterpret_cast<const uintptr_t*>(node+0x40);auto* count=reinterpret_cast<uint32_t*>(node+0x48);
        if(!data||*count!=ready->count||*count>*reinterpret_cast<const uint32_t*>(node+0x4c)||std::memcmp(reinterpret_cast<const void*>(data),ready->data,ready->count*sizeof(WardrobeRecord)))return false;
        *count=0;
        __try{called=pipeline(node,removals,additions,flag);}
        __finally{if(*reinterpret_cast<const uintptr_t*>(node+0x40)==data&&!*count){*count=ready->count;restored=true;}}
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return called&&restored;
}
WardrobeCallResult invokeWardrobeSwap(uintptr_t node,const WardrobeRecord& source,uintptr_t target,uintptr_t gameBase,const WardrobeTransport& ops){
    if(!node||!target||target==gameBase+gameLayout().emptyWrapper||!isSimpleWardrobeRecord(source,gameBase))return WardrobeCallResult::InvalidRecord;
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
WardrobeCallResult invokeWardrobePartBatch(uintptr_t node,const std::vector<WardrobeRecord>& parts,bool attach,uintptr_t gameBase,const WardrobeTransport& ops){
    if(!node||parts.empty()||parts.size()>8)return WardrobeCallResult::InvalidRecord;
    if(!ops.retain||!ops.release||!ops.destroy||!ops.pipeline)return WardrobeCallResult::MissingFunctions;
    std::array<WardrobeRemoval,8> removals{};
    std::array<WardrobeRecord,8> additions{};
    for(size_t i=0;i<parts.size();++i){
        const auto wrapper=pointer(parts[i],0);
        if(!isSimpleWardrobeRecord(parts[i],gameBase)||std::any_of(removals.begin(),removals.begin()+i,[&](const auto& row){return row.wrapper==wrapper;}))return WardrobeCallResult::InvalidRecord;
        removals[i].wrapper=wrapper;additions[i]=parts[i];
    }
    uint32_t count=0;
    for(;count<parts.size();++count)if(!ops.retain(removals[count].wrapper))break;
    const bool retained=count==parts.size();
    WardrobeVector removeList{attach?nullptr:removals.data(),attach?0:count,attach?0:count};
    WardrobeVector addList{attach?additions.data():nullptr,attach?count:0,attach?count:0};
    const bool called=retained&&ops.pipeline(node,&removeList,&addList,0);
    bool cleaned=true;
    for(uint32_t i=0;i<count;++i){const bool ok=attach?ops.destroy(&additions[i]):ops.release(&removals[i].wrapper);cleaned&=ok;}
    if(!cleaned)return WardrobeCallResult::CleanupFailed;
    if(!retained)return WardrobeCallResult::RetainFailed;
    return called?WardrobeCallResult::Completed:WardrobeCallResult::CallFailed;
}
WardrobeCallResult invokeWardrobeRecipe(uintptr_t node,const std::vector<uintptr_t>& remove,const std::vector<const WardrobeRecord*>& add,uintptr_t gameBase,const WardrobeTransport& ops){
    if(!node||!gameBase||(add.empty()&&remove.empty())||add.size()>24||remove.size()>24)return WardrobeCallResult::InvalidRecord;
    if(!ops.retain||!ops.release||!ops.destroy||!ops.pipeline||!ops.copy)return WardrobeCallResult::MissingFunctions;
    const auto valid=[&](uintptr_t wrapper){return wrapper&&wrapper!=gameBase+gameLayout().emptyWrapper;};
    for(auto i=remove.begin();i!=remove.end();++i)if(!valid(*i)||std::find(remove.begin(),i,*i)!=i)return WardrobeCallResult::InvalidRecord;
    for(auto i=add.begin();i!=add.end();++i)if(!*i||!valid(pointer(**i,0))||std::any_of(add.begin(),i,[&](auto earlier){return pointer(*earlier,0)==pointer(**i,0);}))return WardrobeCallResult::InvalidRecord;
    std::array<WardrobeRemoval,24> removals{};
    std::array<WardrobeRecord,24> additions{};
    uint32_t nr=0,na=0;
    const auto cleanup=[&](){
        bool ok=true;
        for(uint32_t i=0;i<nr;++i)ok&=ops.release(&removals[i].wrapper);
        for(uint32_t i=0;i<na;++i)ok&=ops.destroy(&additions[i]);
        return ok;
    };
    for(const auto wrapper:remove){if(!ops.retain(wrapper))return cleanup()?WardrobeCallResult::RetainFailed:WardrobeCallResult::CleanupFailed;removals[nr++].wrapper=wrapper;}
    for(const auto* part:add){if(!ops.copy(&additions[na],part))return cleanup()?WardrobeCallResult::RetainFailed:WardrobeCallResult::CleanupFailed;++na;}
    WardrobeVector removeList{nr?removals.data():nullptr,nr,nr},addList{na?additions.data():nullptr,na,na};
    const bool called=ops.pipeline(node,&removeList,&addList,0);
    if(!cleanup())return WardrobeCallResult::CleanupFailed;
    return called?WardrobeCallResult::Completed:WardrobeCallResult::CallFailed;
}
WardrobeCallResult invokeWardrobeRebuild(uintptr_t node,const std::vector<WardrobeRecord>& ready,const std::vector<WardrobeRecord>& removals,const std::vector<WardrobeRecord>& additions,uintptr_t gameBase,const WardrobeTransport& ops){
    if(!node||ready.empty()||ready.size()>64||removals.size()>8||additions.size()>8||(removals.empty()&&additions.empty()))return WardrobeCallResult::InvalidRecord;
    for(const auto* rows:{&ready,&removals,&additions})for(const auto& row:*rows)if(!isSimpleWardrobeRecord(row,gameBase))return WardrobeCallResult::InvalidRecord;
    if(!ops.retain||!ops.release||!ops.destroy||!ops.rebuild)return WardrobeCallResult::MissingFunctions;
    std::vector<WardrobeRecord> desired;
    const auto has=[](const auto& rows,uintptr_t wrapper){return std::any_of(rows.begin(),rows.end(),[&](const auto& row){return pointer(row,0)==wrapper;});};
    for(const auto& row:ready)if(!has(removals,pointer(row,0))){if(has(desired,pointer(row,0)))return WardrobeCallResult::InvalidRecord;desired.push_back(row);}
    for(const auto& row:additions){const auto found=std::find_if(desired.begin(),desired.end(),[&](const auto& item){return pointer(item,0)==pointer(row,0);});if(found!=desired.end()){if(found->bytes!=row.bytes)return WardrobeCallResult::InvalidRecord;}else desired.push_back(row);}
    if(desired.empty()||desired.size()>64)return WardrobeCallResult::InvalidRecord;
    std::array<WardrobeRemoval,8> removed{};std::array<WardrobeRecord,64> added{};uint32_t nr=0,na=0;
    const auto clean=[&](){bool ok=true;for(uint32_t i=0;i<nr;++i)if(!ops.release(&removed[i].wrapper))ok=false;for(uint32_t i=0;i<na;++i)if(!ops.destroy(&added[i]))ok=false;return ok;};
    for(const auto& row:removals){const auto wrapper=pointer(row,0);if(!ops.retain(wrapper))return clean()?WardrobeCallResult::RetainFailed:WardrobeCallResult::CleanupFailed;removed[nr++]={wrapper,0};}
    for(const auto& row:desired){const auto wrapper=pointer(row,0);if(!ops.retain(wrapper))return clean()?WardrobeCallResult::RetainFailed:WardrobeCallResult::CleanupFailed;added[na++]=row;}
    WardrobeVector removeList{nr?removed.data():nullptr,nr,nr},addList{na?added.data():nullptr,na,na};
    const WardrobeVector baseline{const_cast<WardrobeRecord*>(ready.data()),uint32_t(ready.size()),uint32_t(ready.size())};
    const bool called=ops.rebuild(node,&baseline,&removeList,&addList,0);if(!clean())return WardrobeCallResult::CleanupFailed;
    return called?WardrobeCallResult::Completed:WardrobeCallResult::CallFailed;
}
}
