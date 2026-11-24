#pragma once
#include "barber_catalog.h"
#include <map>
#include <optional>
#include <vector>

namespace hex {
class SpouseBarberSource {
    struct Look {
        std::array<uint8_t,16> identity{};
        std::string save, owner;
        unsigned npc=0;
        OutfitSlots slots;
    };
    std::vector<Look> looks;
    std::filesystem::path lastPath;
    std::filesystem::file_time_type lastWrite{};
    uintmax_t lastSize=0;
public:
    bool refresh(const std::filesystem::path& path,const BarberCatalog& catalog);
    unsigned find(const std::string& save,const std::array<uint8_t,16>& identity,const BarberCatalog& catalog,OutfitSlots& outfit) const;
};
struct SpouseBarberEdit {
    unsigned revision=0;
    std::optional<std::array<uint8_t,16>> choices;
    bool operator==(const SpouseBarberEdit& other) const {
        return revision==other.revision&&choices==other.choices;
    }
};
struct SpouseWardrobeState {
    std::string save;
    unsigned saveEpoch=0;
    uint64_t editorRevision=0;
    std::set<unsigned> combat;
    std::map<unsigned,unsigned> gearRevision;
    std::map<unsigned,SpouseBarberEdit> barberEdits;
    unsigned combatEditorNpc=0;
    bool barberOpen=false,combatEditorOpen=false;
    bool editorOpen() const {return barberOpen||combatEditorOpen;}
    bool usesCombatLook(unsigned npc) const {
        return npc&&((combatEditorOpen&&combatEditorNpc==npc)||combat.count(npc));
    }
};
class SpouseEditorHandoff {
    uint64_t superseded=0;
public:
    bool reconcile(SpouseWardrobeState& state,bool nativeWardrobeOpened);
};
class SpouseGearHandoff {
    unsigned revision=0;
    uint64_t began=0;
    bool active=false;
public:
    void observe(uint64_t now,unsigned nextRevision);
    bool settling(uint64_t now) const {return active&&now>=began&&now-began<750;}
};
SpouseWardrobeState activeSpouseWardrobe(const std::filesystem::path& log);
}
