#include "outfit_visibility.h"
#include <algorithm>
#include <cmath>

namespace hex {
bool OutfitVisibility::contains(uint32_t key){std::lock_guard<std::mutex> guard(cache);return keys.count(key)!=0;}
float OutfitVisibility::query(uintptr_t actor,uintptr_t renderer,int witch,uint32_t key,float normal,uint64_t now){
    std::lock_guard<std::mutex> guard(cache);const auto found=entries.find(actor);
    if(found==entries.end()||!found->second.active||found->second.renderer!=renderer||found->second.witch!=witch||now-found->second.seen>1500)return normal;
    for(auto& part:found->second.parts)if(part.key==key){if(std::isfinite(normal)&&normal>=0){part.normal=normal;part.known=true;}return part.hidden?0.f:normal;}
    return normal;
}
bool OutfitVisibility::update(uintptr_t actor,uintptr_t renderer,int witch,const OutfitVisibilityPolicy& policy,uint64_t now,const OutfitVisibilityCalls& calls){
    std::lock_guard<std::mutex> writes(writer);return updateLocked(actor,renderer,witch,policy,now,calls);
}
bool OutfitVisibility::updateLocked(uintptr_t actor,uintptr_t renderer,int witch,const OutfitVisibilityPolicy& policy,uint64_t now,const OutfitVisibilityCalls& calls){
    if(!actor||!renderer||witch<0||witch>5||!calls.intern||!calls.baseline||!calls.apply)return false;
    Entry entry;
    {
        std::lock_guard<std::mutex> guard(cache);
        for(auto i=entries.begin();i!=entries.end();)if(now-i->second.seen>30000)i=entries.erase(i);else++i;
        const auto previous=entries.find(actor);if(previous!=entries.end())entry=previous->second;
        if(!policy.owned){entries.erase(actor);return true;}
        if(entry.renderer!=renderer||entry.owner!=policy.owner||entry.prefab!=policy.prefab||entry.witch!=witch)entry=Entry{};
        if(!entries.count(actor)&&entries.size()>=64)return false;
    }
    entry.witch=witch;entry.renderer=renderer;entry.owner=policy.owner;entry.prefab=policy.prefab;entry.seen=now;entry.active=policy.enabled;
    for(auto& part:entry.parts)part.hidden=false;
    if(policy.enabled)for(const auto& wanted:policy.parts){
        auto found=std::find_if(entry.parts.begin(),entry.parts.end(),[&](const auto& part){return part.name==wanted.part;});
        if(found==entry.parts.end()){
            if(entry.parts.size()>=16)return false;Part part;part.name=wanted.part;
            if(!calls.intern(part.name.c_str(),&part.key)||!part.key)return false;
            entry.parts.push_back(std::move(part));found=entry.parts.end()-1;
        }
        found->hidden=!wanted.visible;
    }
    {std::lock_guard<std::mutex> guard(cache);for(const auto& part:entry.parts)keys.insert(part.key);entries[actor]=entry;}
    bool success=true;
    for(auto& part:entry.parts){
        if(!part.hidden&&!part.forced)continue;
        if(part.hidden&&part.forced&&now-part.appliedAt<500)continue;
        if(!part.known)part.known=calls.baseline(reinterpret_cast<void*>(renderer),part.key,&part.normal)&&std::isfinite(part.normal)&&part.normal>=0;
        if(!part.known){success=false;continue;}
        if(!calls.apply(reinterpret_cast<void*>(renderer),part.key,part.hidden?0.f:part.normal)){success=false;continue;}
        part.forced=part.hidden;part.appliedAt=now;
    }
    {
        std::lock_guard<std::mutex> guard(cache);const auto latest=entries.find(actor);
        if(latest!=entries.end())for(auto& part:entry.parts)for(const auto& observed:latest->second.parts)if(part.key==observed.key&&observed.known){part.normal=observed.normal;part.known=true;}
        entries[actor]=std::move(entry);
    }
    return success;
}
bool OutfitVisibility::restore(uintptr_t actor,uintptr_t renderer,uint64_t now,const OutfitVisibilityCalls& calls){
    std::lock_guard<std::mutex> writes(writer);OutfitVisibilityPolicy policy;int witch=-1;
    {
        std::lock_guard<std::mutex> guard(cache);const auto found=entries.find(actor);if(found==entries.end())return true;
        if(found->second.renderer!=renderer){entries.erase(found);return true;}
        policy.owned=true;policy.owner=found->second.owner;policy.prefab=found->second.prefab;
        witch=found->second.witch;
    }
    return updateLocked(actor,renderer,witch,policy,now,calls);
}
}
