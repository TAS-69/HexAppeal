#pragma once
#include <cstdint>

namespace hex::creation {
class FactorySettle {
    uint64_t started=0,changed=0;
    uintptr_t previous=0;
public:
    explicit FactorySettle(uint64_t now):started(now),changed(now){}
    bool observe(uint64_t now,uintptr_t entry){
        if(entry!=previous){previous=entry;changed=now;}
        return now-started>=5000||(entry&&now-started>=1800&&now-changed>=650);
    }
};
}
