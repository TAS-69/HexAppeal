#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hex {
struct alignas(8) WardrobeRecord {std::array<unsigned char,0x70> bytes{};};
struct WardrobeVector {void* data=nullptr;uint32_t count=0,capacity=0;};
struct WardrobeRemoval {uintptr_t wrapper=0,metadata=0;};
static_assert(sizeof(WardrobeRecord)==0x70&&sizeof(WardrobeVector)==16&&sizeof(WardrobeRemoval)==16);
struct WardrobeTransport {
    bool(*retain)(uintptr_t)=nullptr;
    bool(*release)(uintptr_t*)=nullptr;
    bool(*destroy)(WardrobeRecord*)=nullptr;
    bool(*pipeline)(uintptr_t,WardrobeVector*,WardrobeVector*,uint8_t)=nullptr;
    bool(*rebuild)(uintptr_t,const WardrobeVector*,WardrobeVector*,WardrobeVector*,uint8_t)=nullptr;
    bool(*copy)(WardrobeRecord*,const WardrobeRecord*)=nullptr;
};
enum class WardrobeCallResult {Completed,InvalidRecord,MissingFunctions,RetainFailed,CallFailed,CleanupFailed};
bool isSimpleWardrobeRecord(const WardrobeRecord& record,uintptr_t gameBase);
bool rebuildWardrobePipeline(uintptr_t node,const WardrobeVector* ready,WardrobeVector* removals,WardrobeVector* additions,uint8_t flag,decltype(WardrobeTransport::pipeline) pipeline);
WardrobeCallResult invokeWardrobeRebuild(uintptr_t node,const std::vector<WardrobeRecord>& ready,const std::vector<WardrobeRecord>& removals,const std::vector<WardrobeRecord>& additions,uintptr_t gameBase,const WardrobeTransport& transport);
WardrobeCallResult invokeWardrobeSwap(uintptr_t node,const WardrobeRecord& source,uintptr_t target,uintptr_t gameBase,const WardrobeTransport& transport);
WardrobeCallResult invokeWardrobePartBatch(uintptr_t node,const std::vector<WardrobeRecord>& parts,bool attach,uintptr_t gameBase,const WardrobeTransport& transport);
WardrobeCallResult invokeWardrobeRecipe(uintptr_t node,const std::vector<uintptr_t>& remove,const std::vector<const WardrobeRecord*>& add,uintptr_t gameBase,const WardrobeTransport& transport);
}
