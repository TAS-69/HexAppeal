#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace hex {
inline constexpr int defaultOutfitRange=45;
inline constexpr int maximumOutfitRange=120;
inline constexpr int clampOutfitRange(int meters){return std::clamp(meters,0,maximumOutfitRange);}

using WardrobePosition=std::array<float,3>;

inline bool validWardrobePosition(const WardrobePosition& position){
    return std::all_of(position.begin(),position.end(),[](float axis){return std::isfinite(axis)&&std::abs(axis)<1.0e7f;});
}
inline bool inOutfitRange(const WardrobePosition& player,const WardrobePosition& actor,int meters){
    if(meters<=0||!validWardrobePosition(player)||!validWardrobePosition(actor))return false;
    const auto range=double(clampOutfitRange(meters));
    double squared=0;
    for(std::size_t axis=0;axis<player.size();++axis){
        const auto delta=double(actor[axis])-player[axis];
        squared+=delta*delta;
    }
    return squared<=range*range;
}
inline bool needsObservedWardrobeUpdate(bool hasPosition,bool withinRange,bool listed){
    return !hasPosition||(withinRange&&!listed);
}
}
