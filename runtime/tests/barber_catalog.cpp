#include "barber_catalog.h"
#include "barber_layers.h"
#include "spouse_wardrobe.h"
#include "wardrobe_items.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>

static void check(bool condition,const char* message){if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int wmain(int argc,wchar_t** argv){
    check(argc==2||argc==3,"fixture path and optional Black Widow test mount required");
    const std::filesystem::path file=argv[1];
    {
        std::ofstream out(file);
        out<<R"(<MeshParam>
  <ParamDesc Category="Hair" Index="2"><MeshSet Index="1"><MeshList MeshFileName="fixture_hair"/></MeshSet></ParamDesc>
  <ParamDesc Category="Armor" Index="7">
    <MeshSet Index="20"><MeshList MeshFileName="first_dress"/><MeshList MeshFileName="first_shawl"/></MeshSet>
    <MeshSet Index="21"><MeshList MeshFileName="second_dress"/></MeshSet>
  </ParamDesc>
  <ParamDesc Index="11" Category="Armor"><MeshSet Index="15"><MeshList MeshFileName="fixture_boots"/></MeshSet></ParamDesc>
</MeshParam>)";
    }
    hex::BarberCatalog catalog;
    check(catalog.load(file)&&catalog.has("first_dress")&&!catalog.has("fixture_hair"),"only mounted armor definitions enter the outfit catalog");
    std::array<uint8_t,16> choices;
    choices.fill(255);choices[2]=1;choices[7]=20;choices[11]=15;
    std::set<std::string> parts;
    check(catalog.resolve(choices,parts)&&parts==std::set<std::string>{"first_dress","first_shawl","fixture_boots"},"resolve all parts at the indices declared by the mounted XML");
    hex::OutfitSlots slots;
    check(catalog.resolveSlots(choices,slots)&&slots.at("Barber07").size()==2&&slots.at("Barber11").count("fixture_boots"),
          "retain native slot identity and all the parts belonging to each slot");
    choices[7]=21;
    check(catalog.resolve(choices,parts)&&parts==std::set<std::string>{"second_dress","fixture_boots"},"updated slot drops the previous dress and shawl");
    auto combatChoices=choices;
    combatChoices[11]=0;
    check(catalog.combatFallbacks(combatChoices)==hex::OutfitParts{"second_dress"},
          "exclude only the live Barber selections and leave a native combat-owned slot untouched");
    auto appearance=file;appearance+=L".app_xml";
    {
        std::ofstream out(appearance);
        out<<R"(<Appearance><Hair><Prefab Name="cd_phw_00_foot_0092"/></Hair><Armor>
          <Prefab Name="cd_nhw_so_ub_20166"/><Prefab Name="cd_phw_00_hel_0185"/>
          <Prefab Name="cd_phw_00_foot_0092"/>
          <Prefab Name="cd_phw_00_foot_00_0185"/><Prefab Name="phw_00_uw_00_0001"/>
          <Prefab Name="other_outfit"/></Armor></Appearance>)";
    }
    const auto defaults=hex::readDefaultWardrobeSlots(appearance,0);
    check(defaults.at("Upper body").count("cd_nhw_so_ub_20166")&&defaults.at("Headwear").count("cd_phw_00_hel_0185")&&
          defaults.at("Feet").count("cd_phw_00_foot_00_0185")&&hex::outfitSlotParts(defaults).size()==3,
          "restore Lyselia's observed native boots when the appearance lists both footwear prefabs");
    check(hex::readDefaultWardrobeSlots(appearance,3).empty(),"a default outfit for a different witch is refused");
    {
        std::ofstream out(appearance);
        out<<R"(<Appearance><Hair><Prefab Name="cd_phw_00_foot_00_0185"/></Hair><Armor>
          <Prefab Name="cd_nhw_so_ub_20166"/><Prefab Name="cd_phw_00_foot_0092"/></Armor></Appearance>)";
    }
    const auto alternateDefault=hex::readDefaultWardrobeSlots(appearance,0);
    check(alternateDefault.at("Feet")==hex::OutfitParts{"cd_phw_00_foot_0092"},
          "keep the alternate boots when they are the only footwear in the default outfit");
    auto itemRoot=file;itemRoot+=L".items";
    std::filesystem::create_directories(itemRoot/L"icons");
    {
        std::ofstream out(itemRoot/L"items.tsv",std::ios::binary);
        out<<"HexAppealItems 1\n42\tSilver Buckled Boots\tFeet\tboots shoes\tcd_phw_00_foot_0042\n"
              "21\tWoven Mantle\tCloak\tcloak cape\tcd_phw_00_cloak_0021\n";
        std::ofstream icon(itemRoot/L"icons/42.rgba",std::ios::binary);
        const std::string pixels(32*32*4,'\xff');icon.write(pixels.data(),pixels.size());
    }
    hex::WardrobeItemCatalog items;
    check(items.load(itemRoot),"load validated local names and actual icon pixels");
    const auto boots=items.find("42 boots",{});
    check(boots.size()==1&&boots.front()->name=="Silver Buckled Boots"&&boots.front()->slot=="Feet"&&boots.front()->icon.size()==32*32,
          "look up equipment by item ID and tag with the item's slot and thumbnail");
    check(items.find("BUCKLED","Feet")==boots&&items.find("42","Cloak").empty()&&items.find("2",{}).empty(),
          "name search ignores case, slot filters apply, and IDs match exactly");
    check(items.forPart("cd_phw_00_foot_0042")==boots.front(),"label existing equipped pieces by their registered part");
    auto cachedRoot=file;cachedRoot+=L".cached-items";
    std::filesystem::create_directories(cachedRoot);
    {
        std::ofstream out(cachedRoot/L"items.tsv",std::ios::binary);
        out<<"HexAppealItems 1\n42\tMy Boots\tFeet\tboots\tcd_phw_00_foot_0042\n"
              "43\tOther Boots\tFeet\tboots\tcd_phw_00_foot_0042\n";
    }
    hex::WardrobeItemCatalog cachedItems;
    check(cachedItems.load(cachedRoot)&&cachedItems.hasMissingIcons(),"accept a local index with missing icons");
    cachedItems.fillMissingIcons(items);
    const auto cachedBoots=cachedItems.find("42","Feet");
    check(cachedBoots.size()==1&&cachedBoots.front()->name=="My Boots"&&cachedBoots.front()->icon==boots.front()->icon&&
          cachedItems.find("43","Feet",48,true).empty(),"fill a missing icon only for the matching stock item");
    auto activeMount=file;activeMount+=L".manager";
    const auto package=activeMount/L"mods/Unlabelled Outfit";
    std::filesystem::create_directories(package/L"files");
    const auto source=package/L"files/worn.pac";
    const auto mount=activeMount/L"alternate.tsv";
    {
        std::ofstream out(mount);
        out<<"dmm-runtime\t1\n";
        out<<"file\tcharacter/model/1_pc/2_phw/armor/12_foot/cd_phw_00_foot_0399.pac\tmods/Unlabelled Outfit/files/worn.pac\n";
        std::ofstream(source)<<"fixture";
        std::ofstream(activeMount/L"mod_items.json")<<R"({"81":"Unlabelled Outfit","82":"Another Outfit","83":"Unlabelled Outfit"})";
        std::ofstream(package/L"mod.json")<<R"({"modinfo":false})";
        std::ofstream(package/L"00-malformed.field.json")<<R"({"modinfo":false})";
        std::ofstream(package/L"strings.field.json")<<R"({"targets":[
          {"file":"gamedata/bin/StringInfo.pabgb","intents":[0,{"new_key":"900","template":{"buffer":"cd_phw_00_foot_0399"}}]},
          {"file":"localizationstring_eng.paloc","intents":[{"entry":"4257858778562672","new":"Ember Boots"}]}
        ]})";
        std::ofstream(package/L"outfit.field.json")<<R"({
          "modinfo":{"title":"Unlabelled Outfit"},
          "targets":[false,
            {"file":"ItemInfo.pabgb","intents":[null,
              {"op":"clone_record","new_key":81,"patches":[
                {"path":"item_name.default","new":"4257858778562672"},
                {"path":"prefab_data_list[0].prefab_names","new":["900"]},
                {"path":"item_icon_list","new":true}]},
              {"op":"clone_record","new_key":82,"patches":[
                {"path":"item_name","new":"Wrong owner"},
                {"path":"prefab_data_list","new":[{"prefab_names":[900]}]}]},
              {"op":"clone_record","new_key":83,"patches":[
                {"path":"item_name","new":"Not mounted"},
                {"path":"prefab_data_list","new":[{"prefab_names":[901]}]}]}]}
          ]})";
    }
    hex::WardrobeItemCatalog unknown;
    check(unknown.addMounted({}, "vanilla",mount)==1,"identify mounted equipment despite malformed optional mod metadata and unrelated rows");
    const auto ember=unknown.find("ember boots","Feet");
    check(ember.size()==1&&ember.front()->id==81&&ember.front()->fitUnverified&&ember.front()->icon.empty(),
          "show the declared name for a body-unlabelled mod even when its thumbnail is unavailable");
    check(unknown.find("82","").empty()&&unknown.find("83","").empty(),"ignore other owners and unmounted armour");
    check(unknown.find("cd_phw_00_foot_0399","Feet")==ember&&unknown.find("Unlabelled Outfit","Feet")==ember,
          "read split item and string rules and find the result by exact prefab or mod name");
    std::ofstream(package/L"mod.json")<<R"json({"modinfo":{"title":"Ember Boots (VENUS)"}})json";
    hex::WardrobeItemCatalog otherBody,matchingBody;
    check(otherBody.addMounted({}, "vanilla",mount)==1&&matchingBody.addMounted({}, "venus",mount)==1,
          "keep mounted equipment discoverable for both body modes");
    const auto otherBodyBoots=otherBody.find("81","Feet");
    const auto matchingBodyBoots=matchingBody.find("81","Feet");
    check(otherBodyBoots.size()==1&&otherBodyBoots.front()->fitUnverified&&
          matchingBodyBoots.size()==1&&!matchingBodyBoots.front()->fitUnverified,
          "show a fit advisory only where the declared body differs");
    hex::WardrobeItemCatalog originalCharacter;
    check(originalCharacter.addMounted({}, {},mount)==1,"list mounted VENUS equipment for a character with its own rig");
    const auto originalBoots=originalCharacter.find("ember boots","Feet");
    check(originalBoots.size()==1&&originalBoots.front()->fitUnverified&&originalBoots.front()->id==81,
          "flag the fit while keeping the mod's item name and ID");
    {
        std::ofstream out(mount,std::ios::app);
        out<<"file\tcharacter/model/1_pc/2_phw/armor/12_foot/cd_phw_00_foot_0399.pac\tother.pac\n";
    }
    hex::WardrobeItemCatalog conflicted;
    check(!conflicted.addMounted({},"venus",mount),"an ambiguous mounted accessory is excluded from selection");
    check(!conflicted.addMounted({},{},mount),"a character's own rig still requires unambiguous mounted equipment");
    {
        const auto pack=activeMount/L"mods/Example Wardrobe";
        const auto selected=pack/L"VENUS Fit/files";
        std::filesystem::create_directories(selected);
        std::ofstream(pack/L"mod.json")<<R"({"modinfo":{"title":"Example Wardrobe"}})";
        std::ofstream(selected/L"coat.pac")<<"fixture";
        std::ofstream(selected/L"unregistered.pac")<<"fixture";
        const auto fixture=activeMount/L"dmm";
        std::filesystem::create_directories(fixture);
        const std::string bootRoute="character/model/1_pc/2_phw/armor/12_foot/cd_phw_00_foot_0042.pac";
        const auto staged=fixture/L"files"/std::filesystem::u8path(bootRoute);
        std::filesystem::create_directories(staged.parent_path());
        std::ofstream(staged)<<"fixture";
        const auto registry=fixture/L"partprefabtable.pappt";
        {
            std::ofstream out(registry,std::ios::binary);
            const uint32_t header[3]={1,0,1},secondary=0;
            out.write(reinterpret_cast<const char*>(header),sizeof header);
            const auto str=[&](std::string_view value){out.put(char(value.size()+1));out<<value;out.put(0);};
            str("cd_phw_00_ub_test_coat");str("1_pc/02_phw/armor/09_upperbody");str("");
            out.put(1);str("");out.put(0);out.put(0);
            out.write(reinterpret_cast<const char*>(&secondary),sizeof secondary);
        }
        const auto routes=fixture/L"manifest.tsv";
        {
            std::ofstream out(routes,std::ios::binary);
            out<<"dmm-runtime\t1\n";
            out<<"file\tcharacter/bin__/partprefabtable.pappt\tpartprefabtable.pappt\n";
            for(const auto& [part,asset]:std::map<std::string,std::string>{
                {"cd_phw_00_ub_test_coat","coat"},{"cd_phw_00_ub_test_unregistered","unregistered"}})
                out<<"file\tcharacter/model/1_pc/2_phw/armor/09_upperbody/"<<part<<".pac\t"<<
                    (selected/std::filesystem::u8path(asset+".pac")).u8string()<<'\n';
            out<<"file\t"<<bootRoute<<'\t'<<staged.u8string()<<'\n';
            std::ofstream(activeMount/L"mount_log.txt")<<"[INJECT] "<<bootRoute<<
                " (runtime, file=cd_phw_00_foot_0042.pac, from=Example Wardrobe/VENUS Fit)\n";
        }
        auto withVenus=items,withVanilla=items;
        check(withVenus.addMounted({},"venus",routes)==2,"detect refits without an item map and read a newly registered wardrobe part");
        const auto refit=withVenus.find("Example Wardrobe",{},10);
        check(refit.size()==2&&withVenus.find("42","Feet").front()->name=="Silver Buckled Boots"&&
            withVenus.find("CD_PHW_00_UB_TEST_COAT","Upper body").size()==1&&
            withVenus.find("CD_PHW_00_UB_TEST_UNREGISTERED","Upper body").empty(),
            "retain native metadata, identify the staged winner and expose only the authoritative registered part");
        check(withVanilla.addMounted({},"vanilla",routes)==2,"keep active nested refits discoverable across body modes");
        const auto vanillaRefits=withVanilla.find("Example Wardrobe",{},10);
        check(vanillaRefits.size()==2&&withVanilla.find("42","Feet").size()==1&&
            vanillaRefits[0]->fitUnverified&&vanillaRefits[1]->fitUnverified&&
            withVanilla.find("CD_PHW_00_UB_TEST_UNREGISTERED","Upper body").empty(),
            "mark active nested VENUS refits with a fit advisory in Vanilla and still require a registered part");
    }
    auto spouseFile=file;spouseFile+=L".spouse.json";
    auto spouseLog=file;spouseLog+=L".spouse.log";
    {
        std::ofstream out(spouseFile);
        out<<"\xef\xbb\xbf"<<R"({"version":4,"slots":{
          "ignored":false,
          "0":{"sections":{"spouse_looks":{"v":1,"rows":[null,{"character":0,"npc":184,"mesh":"ffff01ffffffffffffffffffffffffff","outfit":{"7":"first_dress","11":"fixture_boots"},"version":1}]}}},
          "1":{"sections":{"spouse_looks":{"v":1,"rows":[{"character":0,"npc":184,"mesh":"ffff01ffffffffffffffffffffffffff","outfit":{"7":"second_dress"},"version":1}]}}}
        }})";
    }
    {
        std::ofstream out(spouseLog);
        out<<"[00:00:00.000] core: ini loaded log_level=INFO\n"
           <<"[00:00:00.001] data: slot loaded slot=0 sections=spouse_looks\n"
           <<"[00:00:00.002] gear: look set npc=184 reason=settings-changed state=always before=4:0 after=4:991101 put=1 clear=0 err=0\n"
           <<"[00:00:00.003] gear: look set npc=185 reason=settings-changed state=always before=4:0 after=4:991101 put=1 clear=0 err=0\n"
           <<"[00:00:00.004] gear: look set npc=185 reason=settings-changed state=no-fight before=4:991101 after=4:0 put=0 clear=1 err=0\n";
    }
    hex::SpouseBarberSource spouse;
    check(spouse.refresh(spouseFile,catalog),"read Spouse Overhaul's slot format");
    const auto active=hex::activeSpouseWardrobe(spouseLog);
    check(active.save=="0"&&active.combat==std::set<unsigned>{184},"locate the current save and active combat equipment");
    check(active.gearRevision.at(184)==1&&active.gearRevision.at(185)==2,
          "track combat entry and exit independently per spouse");
    hex::SpouseGearHandoff handoff;
    check(!handoff.settling(100),"no gear dispatch allows immediate Wardrobe work");
    handoff.observe(100,1);
    handoff.observe(300,1);
    check(handoff.settling(849)&&!handoff.settling(850),"re-reading an event does not extend assembly time");
    handoff.observe(850,2);
    check(handoff.settling(850)&&!handoff.settling(1600),"a confirmed exit yields one bounded handoff");
    hex::OutfitSlots saved;
    check(spouse.find(active.save,choices,catalog,saved)==184&&saved.at("Barber07").count("first_dress"),
          "restore active-save IDs even when the world actor currently wears another selected slot");
    check(spouse.find("1",choices,catalog,saved)&&saved.at("Barber07").count("second_dress"),
          "keep saved outfit slots apart across game saves");
    auto unrelated=choices;unrelated[2]=2;
    check(!spouse.find(active.save,unrelated,catalog,saved)&&!spouse.find({},choices,catalog,saved),
          "refuse to import another spouse's outfit or guess the active save");
    {
        std::ofstream out(spouseLog,std::ios::app);
        out<<"[00:00:00.005] barber: opened for spouse\n"
           <<"[00:00:00.006] barber: look saved npc=184 outfit=FF FF 68 23 35 04 27 2F / FF FF\n";
    }
    const auto editing=hex::activeSpouseWardrobe(spouseLog);
    const auto& firstEdit=editing.barberEdits.at(184);
    const std::array<uint8_t,16> firstSaved{255,255,255,255,255,255,255,255,255,255,0x68,0x23,0x35,4,0x27,0x2f};
    check(editing.editorOpen()&&firstEdit.revision==1&&firstEdit.choices==firstSaved,
          "track the committed outfit and its native slot indices while the editor is open");
    hex::SpouseEditorHandoff editorHandoff;
    auto resumed=editing;
    check(editorHandoff.reconcile(resumed,true)&&!resumed.editorOpen()&&resumed.usesCombatLook(184),
          "a native Wardrobe interaction ends a stale editor pause and preserves active Combat Gear");
    resumed=editing;
    check(editorHandoff.reconcile(resumed,false)&&!resumed.editorOpen(),
          "rereading the same editor event cannot freeze Wardrobe again");
    {
        std::ofstream out(spouseLog,std::ios::app);
        out<<"[00:00:00.007] gear: Set Combat Gear pressed npc=184\n"
           <<"[00:00:00.008] gear: screen opened; actor held by screen npc=184\n";
    }
    const auto combatEditor=hex::activeSpouseWardrobe(spouseLog);
    check(combatEditor.editorOpen()&&!combatEditor.barberOpen&&combatEditor.usesCombatLook(184)&&!combatEditor.usesCombatLook(185),
          "a real combat preview retires a stale barber event and preserves other outfits");
    resumed=combatEditor;
    check(!editorHandoff.reconcile(resumed,false)&&resumed.editorOpen(),"a newly opened spouse editor retains control");
    check(combatEditor.gearRevision.at(184)==2&&combatEditor.gearRevision.at(185)==2,
          "preview intent yields only the affected spouse before the native editor opens");
    {
        std::ofstream out(spouseLog,std::ios::app);
        out<<"[00:00:00.009] gear: screen closed; look after server weapons read npc=184\n"
           <<"[00:00:00.010] barber: look saved npc=184 outfit=FF FF 68 23 34 04 27 2F / FF FF\n";
    }
    const auto done=hex::activeSpouseWardrobe(spouseLog);
    auto latestSaved=firstSaved;
    latestSaved[12]=0x34;
    check(!done.editorOpen()&&done.usesCombatLook(184)&&!done.usesCombatLook(185)&&
          done.gearRevision.at(184)==3&&done.barberEdits.at(184).revision==2&&done.barberEdits.at(184).choices==latestSaved,
          "replace the previous outfit with the latest committed slots");
    {
        std::ofstream out(spouseLog,std::ios::app);
        out<<"[00:00:00.011] barber: look saved npc=184 outfit=FF FF 68 23 34 04 27 2F / 01 FF\n";
    }
    const auto changedLayout=hex::activeSpouseWardrobe(spouseLog);
    check(changedLayout.barberEdits.at(184).revision==3&&!changedLayout.barberEdits.at(184).choices,
          "an unfamiliar slot encoding signals a save without reusing previous slot choices");
    {
        std::ofstream out(spouseLog,std::ios::app);
        out<<"[00:00:00.011] gear: screen opened for non-spouse npc=186\n";
    }
    const auto foreignEditor=hex::activeSpouseWardrobe(spouseLog);
    check(!foreignEditor.editorOpen()&&!foreignEditor.usesCombatLook(186)&&foreignEditor.usesCombatLook(184),
          "a non-spouse gear notification cannot pause Wardrobe or claim another character");
    {
        std::ofstream out(spouseLog,std::ios::app);
        out<<"[00:00:00.011] barber: opened for spouse\n"
           <<"[00:00:00.011] data: slot loaded, no mod records slot=2\n";
    }
    const auto emptySave=hex::activeSpouseWardrobe(spouseLog);
    check(emptySave.save.empty()&&!emptySave.editorOpen()&&emptySave.combat.empty()&&emptySave.barberEdits.empty(),
          "a save without Spouse Overhaul records clears live editor and gear state");
    {
        std::ofstream out(spouseLog,std::ios::app);
        out<<"[00:00:00.011] data: slot loaded slot=0 sections=spouse_looks\n";
    }
    const auto reloaded=hex::activeSpouseWardrobe(spouseLog);
    check(reloaded.save==done.save&&reloaded.saveEpoch==emptySave.saveEpoch+1&&reloaded.barberEdits.empty()&&reloaded.combat.empty()&&reloaded.gearRevision.empty(),
          "loading another copy of the same slot clears the previous live changes");
    {
        std::ofstream out(spouseLog,std::ios::app);
        out<<"[00:00:01.000] data: records not applied (records belong to another save file) slot=1 sections=spouse_looks\n";
    }
    const auto rejected=hex::activeSpouseWardrobe(spouseLog);
    check(rejected.save.empty()&&rejected.combat.empty()&&rejected.barberEdits.empty()&&
          !spouse.find(rejected.save,choices,catalog,saved),
          "discard the active save and combat state when the game rejects the stored records");
    {std::ofstream out(spouseFile,std::ios::binary);out<<"{\"version\":4,\"slots\":";}
    check(!spouse.refresh(spouseFile,catalog)&&spouse.find(active.save,choices,catalog,saved)&&
          saved.at("Barber07").count("first_dress"),
          "a torn save read leaves the last valid outfit available for that save");
    choices[7]=22;
    check(!catalog.resolve(choices,parts)&&parts==std::set<std::string>{"second_dress","fixture_boots"},"unmapped choices never publish a partial outfit");
    const std::set<std::string> originals{"witch_robe","witch_boots","witch_hat"};
    const hex::BarberLayers partial{{"new_cloak"},{"witch_robe","witch_boots"}};
    check(partial.sharesOriginals(originals)&&partial.all()==std::set<std::string>{"new_cloak","witch_robe","witch_boots"},"partial Barber outfit keeps its inherited originals in the visibility and replay targets");
    const hex::BarberLayers explicitOriginal{{"new_cloak","witch_hat"},{}};
    check(explicitOriginal.sharesOriginals(originals)&&explicitOriginal.all().size()==2,"explicitly selected original prefab still needs the layered switch");
    check(!hex::BarberLayers{{"new_dress","new_boots"},{}}.sharesOriginals(originals),"a complete replacement keeps its existing armory route");
    hex::BarberReplayWatch replay;
    using Replay=hex::BarberReplayResult;
    replay.submitted(100);
    check(replay.observe(5099,false)==Replay::Waiting,"a partial return is still pending before the deadline");
    check(replay.observe(5100,false)==Replay::TimedOut,"a delayed return reports the timeout once");
    check(replay.observe(5400,false)==Replay::Overdue,"an incomplete assembly remains observable after the deadline");
    check(replay.observe(7500,true)==Replay::Confirmed,"a verified late assembly can complete the original return");
    replay.submitted(8000);
    check(replay.observe(8500,true)==Replay::Confirmed,"a normal return is confirmed without waiting");
    replay.submitted(9000);
    check(replay.observe(14000,false)==Replay::TimedOut,"a new submission has its own timeout report");
    choices.fill(255);
    check(catalog.resolve(choices,parts)&&parts.empty(),"an untouched appearance has no selected armor slots");
    {std::ofstream out(file);out<<R"(<!DOCTYPE MeshParam [<!ENTITY foreign SYSTEM "file:///fixture.txt">]><MeshParam>&foreign;</MeshParam>)";}
    check(!catalog.load(file)&&catalog.has("first_dress"),"unsupported XML load preserves the previous catalog");
    if(argc==3){
        hex::WardrobeItemCatalog venus,vanilla;
        const auto added=venus.addMounted(argv[2],"venus");
        const auto boots=venus.find("991103 boots","Feet",1,true);
        check(added>=4&&boots.size()==1&&boots.front()->name=="Black Widow's Stiletto Boots",
              "resolve the mod's exact item name and decode its mounted thumbnail");
        vanilla.addMounted(argv[2],"vanilla");
        const auto vanillaBoots=vanilla.find("991103","",1,true);
        check(vanillaBoots.size()==1&&vanillaBoots.front()->name==boots.front()->name&&vanillaBoots.front()->fitUnverified,
              "display the mounted outfit and its icon with a fit advisory for the other body");
        hex::WardrobeItemCatalog addon;
        addon.addMounted(argv[2],{});
        const auto addonBoots=addon.find("991103 boots","Feet",1,true);
        check(addonBoots.size()==1&&addonBoots.front()->name==boots.front()->name&&addonBoots.front()->fitUnverified,
              "original-character equipment keeps the mounted item's name and thumbnail with an unverified fit");
    }
    std::puts("PASS: mounted barber definitions, delayed replay, and tolerant item indexing with active ownership and body checks.");
}
