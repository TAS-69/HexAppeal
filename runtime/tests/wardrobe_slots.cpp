#include "wardrobe_slots.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace {
void check(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
}
}

int wmain(int argc, wchar_t** argv) {
    using namespace hex;
    check(argc == 2, "fixture path required");
    const std::filesystem::path path(argv[1]);
    std::string error;
    WardrobeSlots slots;
    check(slots.load(path,error), "start with an empty slot store");
    const OutfitSlots robe{{"Upper body", {"custom_ub_02"}}, {"Feet", {"cd_foot_001"}}};
    const OutfitSlots combat{{"Upper body", {"combat_ub_03"}}, {"Feet", {"cd_foot_001"}}};
    const OutfitSlots barber{{"Barber14", {"unknown_accessory_id"}}, {"Upper body", {"custom_ub_01"}}};
    check(slots.save(0,SlotOutfit::Saved,robe,error), "save registered parts without a provider");
    check(!slots.save(0,SlotOutfit::Combat,combat,error), "leave the Combat profile read-only");
    check(!slots.save(0,SlotOutfit::Barber,barber,error), "leave the Barber profile read-only");
    check(slots.observe(0,SlotOutfit::Combat,combat), "read current Combat outfit");
    check(slots.observe(0,SlotOutfit::Barber,barber), "read current Barber outfit");
    check(!slots.select(0,SlotOutfit::Combat,error), "keep an observed Combat outfit under Spouse Overhaul's control");
    check(slots.select(0,SlotOutfit::Barber,error), "select Barber independently of combat");
    check(slots.setHidden(0,"Feet",true,error), "persist visibility independently");
    WardrobeSlots restarted;
    check(restarted.load(path,error), "read saved names on a fresh start");
    const auto& profile=restarted.profile(0);
    check(profile.selected==SlotOutfit::Barber&&profile.saved==robe&&profile.combat.empty()&&profile.barber.empty(),
          "preserve only Hex Appeal's outfit and the selected profile");
    auto legacyPath=path;
    legacyPath+=L".legacy-combat";
    {
        std::ofstream legacy(legacyPath,std::ios::binary|std::ios::trunc);
        legacy<<"HexAppealSlots 2\n0\tS\t4\n0\tC\tFeet\tcd_foot_001\n0\tF\tUpper body\tcombat_ub_03\n";
    }
    WardrobeSlots legacy;
    check(legacy.load(legacyPath,error)&&legacy.profile(0).selected==SlotOutfit::Game&&
          legacy.profile(0).saved.at("Feet").count("cd_foot_001")&&legacy.profile(0).combat.empty(),
          "an old Combat selection follows the game and preserves the saved Wardrobe slots");
    check(profile.hidden==std::set<std::string>{"Feet"}, "hidden boots are still part of the saved outfit");
    const auto changes=outfitSlotChanges(robe,combat);
    check(changes.remove==OutfitParts{"custom_ub_02"}&&changes.add==OutfitParts{"combat_ub_03"},
          "only switch changed parts and leave shared boots in place");
    auto layered=robe;
    layered["Hands"]={"custom_hand_01"};
    layered["Headwear"]={"custom_hel_01"};
    const auto preview=outfitSlotChanges(robe,layered,true);
    check(preview.remove.empty()&&preview.add==outfitSlotParts(layered),
          "adding a slot resubmits the already equipped pieces in the same appearance request");
    check(!outfitPartsPresent(outfitSlotParts(robe),outfitSlotParts(layered))&&
          outfitPartsPresent(outfitSlotParts(layered),outfitSlotParts(robe))&&
          outfitPartsPresent(outfitSlotParts(layered),{}),
          "hide other clothing only once all saved parts are present, or when the saved outfit is deliberately empty");
    const OutfitParts nativeRobe{"cd_phw_00_ub_00_0159"};
    const OutfitParts nativeBoots{"cd_phw_00_foot_00_0159"};
    const OutfitParts moddedCloak{"mod_cloak_inherited"};
    check(canMaskInheritedOutfitParts({"cd_phw_00_ub_00_0159","cd_phw_00_foot_00_0159","mod_cloak_inherited"},
                                     nativeRobe,nativeBoots,moddedCloak),
          "mask clothing reattached from the default or Follow game outfit");
    check(!canMaskInheritedOutfitParts({"cd_phw_00_ub_00_0159","cd_phw_00_foot_0042"},
                                      nativeRobe,nativeBoots,moddedCloak),
          "unequip Wardrobe boots through native slots so Cancel can assemble them again");
    check(!canMaskInheritedOutfitParts(nativeBoots,nativeRobe,nativeBoots,moddedCloak),
          "wait for a missing target before masking inherited parts");
    WardrobeSlotRetry retry;
    check(!retry.tryAgain(100,false,false,0)&&!retry.tryAgain(200,false,true,300),
          "wait for a stable outfit and an expired assembly deadline");
    check(retry.tryAgain(300,false,true,300)&&!retry.tryAgain(350,true,true,0)&&
          retry.tryAgain(600,false,true,0)&&!retry.tryAgain(1000,false,true,0),
          "brief successful assembly cannot reset the retry budget during a conflict");
    check(!retry.tryAgain(2000,true,true,0)&&!retry.tryAgain(6999,true,true,0)&&
          retry.attempts==2&&!retry.tryAgain(7000,true,true,0)&&retry.tryAgain(8000,false,true,0),
          "a confirmed stable outfit starts a new retry budget for later game changes");
    const auto repair=outfitSlotChanges(layered,robe,true);
    check(repair.remove==OutfitParts{"custom_hand_01","custom_hel_01"}&&repair.add==outfitSlotParts(robe),
          "restoring an outfit resubmits its complete target and removes only unwanted pieces");
    const auto unchanged=outfitSlotChanges(layered,layered,true);
    check(unchanged.remove.empty()&&unchanged.add.empty(),"a confirmed Wardrobe is not resubmitted");
    check(outfitSlotForPart("new_mod_hel_03")=="Headwear"&&outfitSlotForPart("phw_00_uw_00_0001").empty(),
          "recognise registered naming by slot and keep underwear separate");
    check(outfitSlotForPart("NEW_MOD_HEL_03")=="Headwear"&&
          !restarted.save(0,SlotOutfit::Saved,{{"Upper body",{"PHW_00_NUDE_00_0001"}}},error),
          "mixed-case armour is recognised and body parts cannot slip into the clothing slots");
    check(!restarted.save(0,SlotOutfit::Saved,{{"Upper body",{"../forged"}}},error),"reject paths and preserve the current outfit");
    check(restarted.profile(0).saved==robe,"a rejected edit leaves the stored parts intact");
    check(!restarted.select(1,SlotOutfit::Saved,error),"missing wardrobe cannot be selected");
    check(restarted.save(1,SlotOutfit::Saved,{},error,true),"save an intentionally empty wardrobe");
    WardrobeSlots cleared;
    check(cleared.load(path,error)&&cleared.profile(1).hasSaved&&cleared.profile(1).saved.empty()&&
          cleared.profile(1).selected==SlotOutfit::Saved,"cleared wardrobe stays selectable and empty after reload");
    auto pending=cleared.profile(0).itemOptions;
    pending[{"example/boots","colour"}]="jade";
    check(cleared.profile(0).itemOptions.empty(),"discarding a draft leaves saved item options alone");
    check(cleared.save(0,SlotOutfit::Saved,layered,error,true,&pending),"save equipped parts and their options together");
    WardrobeSlots withItems;
    check(withItems.load(path,error)&&withItems.profile(0).saved==layered&&withItems.profile(0).itemOptions==pending&&
          withItems.profile(1).itemOptions.empty(),"reload existing witch profiles and their stable item option IDs");
    pending[{"example/boots","bad\tkey"}]="jade";
    check(!withItems.save(0,SlotOutfit::Saved,robe,error,true,&pending)&&
          withItems.profile(0).saved==layered,"invalid item options leave the equipped outfit intact");
    {std::ofstream file(path, std::ios::app | std::ios::binary);file<<"0\tC\tUpper body\tcustom_ub_02\n";}
    WardrobeSlots duplicate;
    check(duplicate.load(path,error)&&!error.empty()&&duplicate.profile(0).saved==layered&&
          duplicate.profile(0).selected==SlotOutfit::Saved,
          "skip duplicate persisted slot entries while keeping usable choices");
    auto addonPath=path;
    addonPath+=L".addon";
    {
        std::ofstream legacy(addonPath,std::ios::binary);
        legacy<<"HexAppealSlots 1\n0\tS\t0\n0\tH\tUpper body\n0\tH\tFeet\n";
    }
    WardrobeSlots addon;
    check(addon.load(addonPath,error,true)&&addon.profile(0).hideOutfit&&addon.profile(0).hidden.empty(),
          "migrate the old add-on whole-outfit setting to independent visibility");
    check(addon.setHidden(0,"Feet",true,error)&&addon.save(0,SlotOutfit::Saved,layered,error,true,&withItems.profile(0).itemOptions)&&
          addon.setOutfitHidden(0,false,error),"save the shared outfit format with independent add-on visibility");
    WardrobeSlots addonReloaded;
    check(addonReloaded.load(addonPath,error,true)&&addonReloaded.profile(0).saved==layered&&
          addonReloaded.profile(0).itemOptions==withItems.profile(0).itemOptions&&
          !addonReloaded.profile(0).hideOutfit&&addonReloaded.profile(0).hidden==std::set<std::string>{"Feet"},
          "reload the outfit and SDK settings, with the one hidden slot preserved independently");
    std::puts("PASS: Hex Appeal outfit persistence, read-only Barber and Combat profiles, slot visibility and native part deltas.");
}
