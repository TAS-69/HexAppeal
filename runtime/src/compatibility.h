#pragma once
#include <array>
#include <string>

namespace hex {
enum class Feature : size_t {Shaping,Hex,Weight,Wardrobe,Underwear,Count};
enum class CompatibilityState {Waiting,Ready,Blocked};
struct FeatureCompatibility {
    CompatibilityState state=CompatibilityState::Waiting;
    std::string detail="Waiting for the compatibility check.";
    bool allows(bool filesReady) const{return filesReady&&state!=CompatibilityState::Blocked;}
};
using CompatibilitySnapshot=std::array<FeatureCompatibility,size_t(Feature::Count)>;
inline FeatureCompatibility effectiveCompatibility(const CompatibilitySnapshot& snapshot,Feature feature){
    const auto index=size_t(feature);if(index>=snapshot.size())return {CompatibilityState::Blocked,"Unknown feature."};
    const auto& value=snapshot[index];const auto& shaping=snapshot[size_t(Feature::Shaping)];
    if((feature==Feature::Wardrobe||feature==Feature::Underwear||feature==Feature::Weight)&&value.state==CompatibilityState::Waiting&&shaping.state==CompatibilityState::Blocked)return {CompatibilityState::Blocked,"Shaping adapter unavailable: "+shaping.detail};
    return value;
}
inline const char* featureName(Feature feature){
    switch(feature){case Feature::Shaping:return "Shaping";case Feature::Hex:return "HEX preset";case Feature::Weight:return "Weight";case Feature::Wardrobe:return "Wardrobe";case Feature::Underwear:return "Base underwear";default:return "Unknown feature";}
}
}
