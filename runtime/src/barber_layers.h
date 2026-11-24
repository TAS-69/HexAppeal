#pragma once
#include <algorithm>
#include <cstdint>
#include <set>
#include <string>

namespace hex {
struct BarberLayers {
    std::set<std::string> explicitPieces;
    std::set<std::string> inheritedPieces;

    std::set<std::string> all() const {
        auto pieces=explicitPieces;
        pieces.insert(inheritedPieces.begin(),inheritedPieces.end());
        return pieces;
    }
    bool sharesOriginals(const std::set<std::string>& originals) const {
        return !inheritedPieces.empty()||std::any_of(explicitPieces.begin(),explicitPieces.end(),[&](const auto& name){return originals.count(name)!=0;});
    }
};

enum class BarberReplayResult { Waiting, TimedOut, Overdue, Confirmed };

class BarberReplayWatch {
    uint64_t deadline=0;
    bool timeoutReported=false;
public:
    void submitted(uint64_t now){
        deadline=now+5000;
        timeoutReported=false;
    }
    BarberReplayResult observe(uint64_t now,bool confirmed){
        if(confirmed)return BarberReplayResult::Confirmed;
        if(now<deadline)return BarberReplayResult::Waiting;
        if(timeoutReported)return BarberReplayResult::Overdue;
        timeoutReported=true;
        return BarberReplayResult::TimedOut;
    }
};
}
