#include "wardrobe_slots.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iterator>

namespace hex {
namespace {
constexpr std::pair<std::string_view, std::string_view> slotMarkers[] = {
    {"_ub_", "Upper body"}, {"_lb_", "Lower body"}, {"_hel_", "Headwear"},
    {"_hand_", "Hands"}, {"_foot_", "Feet"}, {"_cloak_", "Cloak"},
    {"_necklace_", "Accessories"}, {"_earring_", "Accessories"}, {"_ring_", "Accessories"},
    {"_shoulder_", "Shoulders"}, {"_belt_", "Waist"}, {"_armor_", "Armour"}
};
constexpr size_t maxParts = 24;
constexpr uintmax_t maxFile = 131072;
constexpr size_t maxItemOptions = 96;

bool validOptionIdentifier(std::string_view text,size_t limit,bool outfit=false){
    return !text.empty()&&text.size()<=limit&&(!outfit||text.find('/')!=text.npos)&&
        std::all_of(text.begin(),text.end(),[=](char c){
            return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||(outfit&&c=='/');
        });
}

bool validItemOption(std::string_view outfit,std::string_view control,std::string_view value){
    return validOptionIdentifier(outfit,95,true)&&validOptionIdentifier(control,31)&&validOptionIdentifier(value,31);
}

bool containsPartMarker(std::string_view name,std::string_view marker){
    return std::search(name.begin(),name.end(),marker.begin(),marker.end(),[](char value,char lower){
        return (value>='A'&&value<='Z'?value+('a'-'A'):value)==lower;
    })!=name.end();
}

bool validLook(const OutfitSlots& slots) {
    size_t count = 0;
    for (const auto& [slot, parts] : slots) {
        if (!validOutfitSlot(slot) || parts.empty() || (count += parts.size()) > maxParts) return false;
        for (const auto& part : parts) if (!validOutfitPart(part) || protectedOutfitPart(part)) return false;
    }
    return true;
}
}

bool validOutfitPart(std::string_view name) {
    return !name.empty() && name != "." && name != ".." && name.size() < 192 && std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '_' || c == '-' || c == '.';
    });
}

bool protectedOutfitPart(std::string_view name) {
    constexpr std::string_view bodyParts[] = {
        "_nude_", "_uw_", "_head_", "_hair_", "_eyebrow_", "_eyelash_", "_eye_",
        "_teeth_", "_tongue_", "_body_", "_beard_"
    };
    for (const auto marker : bodyParts) if (containsPartMarker(name,marker)) return true;
    return false;
}

std::string outfitSlotForPart(std::string_view name) {
    for (const auto& [marker, slot] : slotMarkers) if (containsPartMarker(name,marker)) return std::string(slot);
    return {};
}

bool validOutfitSlot(std::string_view name) {
    for (const auto& [marker, slot] : slotMarkers) if (name == slot) return true;
    if (name.size() == 8 && name.substr(0, 6) == "Barber" &&
        name[6] >= '0' && name[6] <= '1' && name[7] >= '0' && name[7] <= '9')
        return (name[6] - '0') * 10 + name[7] - '0' < 16;
    return false;
}

OutfitParts outfitSlotParts(const OutfitSlots& slots) {
    OutfitParts all;
    for (const auto& [slot, parts] : slots) all.insert(parts.begin(), parts.end());
    return all;
}

bool outfitPartsPresent(const OutfitParts& worn, const OutfitParts& target) {
    return std::includes(worn.begin(), worn.end(), target.begin(), target.end());
}

bool canMaskInheritedOutfitParts(const OutfitParts& worn, const OutfitParts& target,
                               const OutfitParts& defaults, const OutfitParts& follow) {
    if (!outfitPartsPresent(worn, target)) return false;
    return std::all_of(worn.begin(), worn.end(), [&](const auto& part) {
        return target.count(part) || defaults.count(part) || follow.count(part);
    });
}

OutfitSlotChanges outfitSlotChanges(const OutfitSlots& current, const OutfitSlots& target, bool includeRetainedParts) {
    const auto before = outfitSlotParts(current), after = outfitSlotParts(target);
    if (before == after) return {};
    OutfitSlotChanges changes;
    std::set_difference(before.begin(), before.end(), after.begin(), after.end(),
        std::inserter(changes.remove, changes.remove.end()));
    if (includeRetainedParts) changes.add = after;
    else std::set_difference(after.begin(), after.end(), before.begin(), before.end(),
        std::inserter(changes.add, changes.add.end()));
    return changes;
}

bool WardrobeSlotRetry::tryAgain(uint64_t now,bool matches,bool settled,uint64_t deadline) {
    if(matches){
        if(!matchingSince)matchingSince=now;
        if(now-matchingSince>=5000)attempts=0;
        return false;
    }
    matchingSince=0;
    if(attempts>=2||!settled||(deadline&&now<deadline))return false;
    ++attempts;
    return true;
}

bool WardrobeSlots::load(const std::filesystem::path& file, std::string& error,bool separateVisibility) {
    path = file;
    separateOutfitVisibility=separateVisibility;
    std::error_code ec;
    if (!std::filesystem::exists(file, ec) && !ec) {
        recoveryBackupNeeded=false;
        error.clear();
        return true;
    }
    error = "Saved outfit slots could not be read.";
    if (std::filesystem::file_size(file, ec) > maxFile || ec) return false;
    std::ifstream in(file, std::ios::binary);
    std::string row;
    if (!std::getline(in, row)) return false;
    if (!row.empty()&&row.back()=='\r')row.pop_back();
    if (row != "HexAppealSlots 1"&&row != "HexAppealSlots 2"&&row != "HexAppealSlots 3") return false;
    const bool hasGlobalVisibility=row=="HexAppealSlots 3";
    const bool hasItemFeatures=row!="HexAppealSlots 1";
    std::array<SlotProfile, 6> loaded{};
    std::array<bool, 6> hasSelection{};
    const auto readRow=[&]()->bool {
        const auto first = row.find('\t'), second = row.find('\t', first + 1);
        if (row.size() < 5 || first != 1 || second != 3 || row[0] < '0' || row[0] > '5') return false;
        const auto witch = size_t(row[0] - '0');
        auto& profile = loaded[witch];
        if (row[2] == 'S') {
            if (hasSelection[witch] || row.size() != 5 || row[4] < '0' || row[4] > '4') return false;
            hasSelection[witch] = true;
            profile.selected = SlotOutfit(row[4] - '0');
            return true;
        }
        if (row[2] == 'H') {
            const auto slot = row.substr(4);
            return validOutfitSlot(slot)&&profile.hidden.insert(slot).second;
        }
        if(row[2]=='V'&&hasGlobalVisibility){
            if(row.size()!=5||row[4]!='1'||profile.hideOutfit)return false;
            profile.hideOutfit=true;
            return true;
        }
        if (row[2] == 'W') {
            if (row.size()!=5 || row[4]!='1' || profile.hasSaved) return false;
            profile.hasSaved = true;
            return true;
        }
        if (row[2] == 'O' && hasItemFeatures) {
            const auto idEnd=row.find('\t',4);
            if(idEnd==row.npos) return false;
            const auto controlEnd=row.find('\t',idEnd+1);
            if(controlEnd==row.npos) return false;
            auto id=row.substr(4,idEnd-4), control=row.substr(idEnd+1,controlEnd-idEnd-1);
            auto value=row.substr(controlEnd+1);
            if(profile.itemOptions.size()>=maxItemOptions||!validItemOption(id,control,value)||
               !profile.itemOptions.try_emplace({std::move(id),std::move(control)},std::move(value)).second) return false;
            return true;
        }
        if (row[2] != 'B' && row[2] != 'C' && row[2] != 'F') return false;
        const auto last = row.find('\t', 4);
        if (last == row.npos) return false;
        const auto slot = row.substr(4, last - 4), part = row.substr(last + 1);
        if (!validOutfitSlot(slot) || !validOutfitPart(part)) return false;
        auto& slots = row[2] == 'B' ? profile.barber : row[2] == 'F' ? profile.combat : profile.saved;
        if (protectedOutfitPart(part)) return false;
        auto& parts=slots[slot];
        if(!parts.insert(part).second)return false;
        if(validLook(slots))return true;
        parts.erase(part);
        if(parts.empty())slots.erase(slot);
        return false;
    };
    size_t rowNumber=1,skipped=0,firstInvalid=0;
    while(std::getline(in,row)){
        ++rowNumber;
        if(!row.empty()&&row.back()=='\r')row.pop_back();
        if(row.empty()||readRow())continue;
        if(skipped++==0)firstInvalid=rowNumber;
    }
    if (in.bad()) return false;
    for (auto& profile : loaded) {
        profile.hasSaved |= !profile.saved.empty();
        if (profile.selected == SlotOutfit::Saved && !profile.hasSaved){
            profile.selected=SlotOutfit::Game;
            ++skipped;
        }
        if (profile.selected == SlotOutfit::Combat) profile.selected = SlotOutfit::Game;
        profile.barber.clear();
        profile.combat.clear();
        if(separateVisibility&&!hasGlobalVisibility&&!profile.hidden.empty()){
            profile.hideOutfit=true;
            profile.hidden.clear();
        }
    }
    profiles = std::move(loaded);
    recoveryBackupNeeded=skipped>0;
    error.clear();
    if(recoveryBackupNeeded){
        error="Recovered saved outfits with "+std::to_string(skipped)+" invalid record(s)";
        if(firstInvalid)error+=" beginning at line "+std::to_string(firstInvalid);
        error+=". The original is backed up before the next write.";
    }
    return true;
}

bool WardrobeSlots::commit(const std::array<SlotProfile, 6>& updated, std::string& error) {
    error = "Saved outfit slots could not be written.";
    if (path.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    if(recoveryBackupNeeded){
        auto backup=path;
        backup+=L".recovery-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".bak";
        if(!CopyFileW(path.c_str(),backup.c_str(),TRUE)){
            error="Could not back up the original wardrobe file before saving recovered slots.";
            return false;
        }
        recoveryBackupNeeded=false;
    }
    auto temporary = path;
    temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    const auto hasItemFeatures=std::any_of(updated.begin(),updated.end(),[](const auto& profile){return !profile.itemOptions.empty();});
    out << (separateOutfitVisibility?"HexAppealSlots 3\n":hasItemFeatures?"HexAppealSlots 2\n":"HexAppealSlots 1\n");
    for (size_t witch = 0; witch < updated.size(); ++witch) {
        const auto& profile = updated[witch];
        out << witch << "\tS\t" << int(profile.selected) << '\n';
        if (profile.hasSaved&&profile.saved.empty()) out << witch << "\tW\t1\n";
        if(separateOutfitVisibility&&profile.hideOutfit)out<<witch<<"\tV\t1\n";
        for (const auto& slot : profile.hidden) out << witch << "\tH\t" << slot << '\n';
        const auto write = [&](char kind, const OutfitSlots& slots) {
            for (const auto& [slot, parts] : slots)
                for (const auto& part : parts) out << witch << '\t' << kind << '\t' << slot << '\t' << part << '\n';
        };
        write('C', profile.saved);
        for(const auto& [ids,value]:profile.itemOptions)
            out << witch << "\tO\t" << ids.first << '\t' << ids.second << '\t' << value << '\n';
    }
    out.close();
    if (!out || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    profiles = updated;
    error.clear();
    return true;
}

bool WardrobeSlots::select(int witch, SlotOutfit outfit, std::string& error) {
    if (outfit == SlotOutfit::Combat) {
        error = "Spouse Overhaul controls Combat Gear.";
        return false;
    }
    if (witch == -1 && (outfit == SlotOutfit::Game || outfit == SlotOutfit::Default)) {
        auto updated = profiles;
        for (auto& profile : updated) profile.selected = outfit;
        if (std::all_of(profiles.begin(), profiles.end(), [=](const auto& profile) {return profile.selected == outfit;})) return true;
        return commit(updated, error);
    }
    if (witch < 0 || witch >= int(profiles.size())) { error = "Choose one witch."; return false; }
    const auto& before = profiles[witch];
    if ((outfit == SlotOutfit::Barber && before.barber.empty()) || (outfit == SlotOutfit::Saved && !before.hasSaved)) {
        error = "This outfit has no saved slots.";
        return false;
    }
    if (before.selected == outfit) return true;
    auto updated = profiles;
    updated[witch].selected = outfit;
    return commit(updated, error);
}

bool WardrobeSlots::save(int witch, SlotOutfit outfit, const OutfitSlots& slots, std::string& error,bool activate,
                         const WardrobeItemOptions* itemOptions) {
    if (witch < 0 || witch >= int(profiles.size()) ||
        outfit != SlotOutfit::Saved ||
        !validLook(slots)) { error = "No valid outfit slots to save."; return false; }
    if(itemOptions&&(itemOptions->size()>maxItemOptions||std::any_of(itemOptions->begin(),itemOptions->end(),[](const auto& option){
        return !validItemOption(option.first.first,option.first.second,option.second);
    }))){error="Too many saved item options or an invalid item option.";return false;}
    const auto& before = profiles[witch];
    if (before.hasSaved && before.saved == slots && (!itemOptions||before.itemOptions==*itemOptions) &&
        (!activate || before.selected == outfit)) return true;
    auto updated = profiles;
    auto& next = updated[witch];
    next.saved = slots;
    if(itemOptions)next.itemOptions=*itemOptions;
    next.hasSaved = true;
    if(activate)next.selected=outfit;
    return commit(updated, error);
}
bool WardrobeSlots::observe(int witch, SlotOutfit outfit, OutfitSlots slots) {
    if (witch < 0 || witch >= int(profiles.size()) ||
        (outfit != SlotOutfit::Barber && outfit != SlotOutfit::Combat) || !validLook(slots)) return false;
    auto& profile = profiles[witch];
    (outfit == SlotOutfit::Barber ? profile.barber : profile.combat) = std::move(slots);
    return true;
}

bool WardrobeSlots::setHidden(int witch, const std::string& slot, bool hidden, std::string& error) {
    if (witch < 0 || witch >= int(profiles.size()) || !validOutfitSlot(slot)) {
        error = "Choose one witch and an outfit slot.";
        return false;
    }
    if ((profiles[witch].hidden.count(slot) != 0) == hidden) return true;
    auto updated = profiles;
    if (hidden) updated[witch].hidden.insert(slot);
    else updated[witch].hidden.erase(slot);
    return commit(updated, error);
}
bool WardrobeSlots::setOutfitHidden(int witch,bool hidden,std::string& error){
    if(!separateOutfitVisibility||witch<0||witch>=int(profiles.size())){
        error="Outfit visibility is unavailable for this character.";return false;
    }
    if(profiles[witch].hideOutfit==hidden)return true;
    auto updated=profiles;
    updated[witch].hideOutfit=hidden;
    return commit(updated,error);
}
}
