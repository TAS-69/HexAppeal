#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace hex {
struct OutfitPartPreference {std::string part;bool visible=true;};
struct OutfitVisibilityPolicy {bool owned=false,enabled=false,replaceBaseUnderwear=false;uintptr_t owner=0;std::string prefab;std::vector<OutfitPartPreference> parts;};
struct OutfitVisibilityCalls {
    bool (*intern)(const char*,uint32_t*)=nullptr;
    bool (*baseline)(void*,uint32_t,float*)=nullptr;
    bool (*apply)(void*,uint32_t,float)=nullptr;
};
class OutfitVisibility {
    struct Part {std::string name;uint32_t key=0;bool hidden=false,forced=false,known=false;float normal=0;uint64_t appliedAt=0;};
    struct Entry {int witch=-1;uintptr_t renderer=0,owner=0;std::string prefab;bool active=false;uint64_t seen=0;std::vector<Part> parts;};
    std::mutex writer,cache;
    std::map<uintptr_t,Entry> entries;
    std::set<uint32_t> keys;
    bool updateLocked(uintptr_t actor,uintptr_t renderer,int witch,const OutfitVisibilityPolicy& policy,uint64_t now,const OutfitVisibilityCalls& calls);
public:
    bool contains(uint32_t key);
    float query(uintptr_t actor,uintptr_t renderer,int witch,uint32_t key,float normal,uint64_t now);
    bool update(uintptr_t actor,uintptr_t renderer,int witch,const OutfitVisibilityPolicy& policy,uint64_t now,const OutfitVisibilityCalls& calls);
    bool restore(uintptr_t actor,uintptr_t renderer,uint64_t now,const OutfitVisibilityCalls& calls);
};
}
