#include "character_addons.h"
#include "game_layout.h"
#include "record_flags.h"
#include "release_version.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

static void check(bool value,const char* message){
    if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}

static void checkCapacity(const std::filesystem::path& root){
    namespace fs=std::filesystem;
    using namespace hex;
    const auto folder=root/L"Characters";
    fs::create_directories(folder);
    const auto count=maximumAddonCharacters-2;
    for(size_t i=0;i<count;++i){
        const auto id="ha_capacity_"+std::to_string(i);
        std::ofstream file(folder/(id+".tsv"));
        file<<"HexAppealCharacters\t1\ncharacter\t"<<id<<"\tCapacity "<<i<<'\t'<<5000000+i
            <<"\tHA_Capacity_"<<i<<"\tcharacter/appearance/npc/capacity_"<<i<<".app_xml\n"
            <<"default\t"<<id<<"\tRobe\texample_ub_robe\n";
    }
    {
        std::ofstream group(folder/L"zz-group.tsv");
        group<<"HexAppealCharacters\t1\ncharacter\tha_capacity_main\tCapacity Main\t5001000\tHA_Capacity_Main\t"
              "character/appearance/npc/capacity_group.app_xml\n"
              "#variant\tha_capacity_variant\tCapacity Variant\t5001001\tHA_Capacity_Variant\tha_capacity_main\n"
              "default\tha_capacity_main\tRobe\texample_ub_robe\n";
        std::ofstream overflow(folder/L"zz-overflow.tsv");
        overflow<<"HexAppealCharacters\t1\ncharacter\tha_capacity_overflow\tCapacity Overflow\t5001002\tHA_Capacity_Overflow\t"
                 "character/appearance/npc/capacity_overflow.app_xml\n"
                 "default\tha_capacity_overflow\tRobe\texample_ub_robe\n";
    }
    const auto started=std::chrono::steady_clock::now();
    const auto scan=scanCharacterAddons(folder);
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    check(scan.characters.size()==maximumAddonCharacters&&scan.packs.size()==maximumAddonCharacters&&
          scan.messages.size()==1&&scan.messages.front().find("character limit was reached")!=std::string::npos&&
          scan.packs.back().accepted==0,"a full roster loads and only the extra record is rejected");
    initializeCharacterAddons(root);
    const auto main=addonTargetForRecord("HA_Capacity_Main");
    const auto variant=addonTargetForRecord("HA_Capacity_Variant");
    const auto low=addonTargetForRecord("HA_Capacity_0");
    check(main==int(characterCapacity)-2&&variant==int(characterCapacity)-1&&low==firstAddonCharacter&&
          enabledAddonTargets().size()==maximumAddonCharacters,"first and last enabled records resolve at full capacity");
    for(const auto target:enabledAddonTargets()){
        const auto& addon=*characterAddon(target);
        check(addonTargetForRecord(addon.record)==target&&characterAddonPackEnabled(addon.pack),
              "every loaded record and pack resolves through the full-width indexes");
        if(!addon.requiresRecord)
            check(enabledAddonTargetForAppearance(addon.appearance.c_str())==target,"appearance resolves at full capacity");
    }
    check(enabledAddonTargetForAppearance("CHARACTER/APPEARANCE/NPC/CAPACITY_0.APP_XML")==low&&
          enabledAddonTargetForAppearance("character/appearance/npc/capacity_group.app_xml")==-1,
          "appearance lookup is case insensitive and cannot select an arbitrary group member");
    const auto highPack=characterAddon(variant)->pack;
    std::string error;
    recordAddonVenusBody(main,false);
    recordAddonVenusBody(variant,true);
    check(addonVenusBodyActive(main)&&!addonVenusUnderwearPresent(main)&&addonVenusUnderwearPresent(variant)&&
          !addonVenusBodyActive(low),"last VENUS slots keep distinct state");
    check(setCharacterAddonPackEnabled(highPack,false,error)&&!characterAddonEnabled(variant)&&characterAddonEnabled(low)&&
          enabledAddonTargets().size()==maximumAddonCharacters-2,"last pack can be disabled without affecting earlier entries");
    initializeCharacterAddons(root);
    check(!characterAddonEnabled(variant)&&characterAddonEnabled(low)&&setCharacterAddonPackEnabled(highPack,true,error),
          "last pack's enabled preference survives restart");
    Profile profile;
    profile.height=.2f;
    check(saveAddonBodyProfile(variant,profile,error)&&addonBodyProfile(main).profile==profile&&
          saveAddonUnderwearHidden(variant,true,error)&&addonUnderwearHidden(main)&&!addonUnderwearHidden(low),
          "last group shares Shape and visibility without affecting earlier records");
    check(setAddonGroupShared(main,false,error)&&addonBodyProfile(variant).profile==profile&&addonUnderwearHidden(variant)&&
          saveAddonUnderwearHidden(variant,false,error)&&addonUnderwearHidden(main),"last variant can keep independent state");
    initializeCharacterAddons(root);
    check(!addonGroupShared(main)&&addonUnderwearHidden(main)&&!addonUnderwearHidden(variant)&&
          addonBodyProfile(variant).profile==profile&&setAddonGroupShared(main,true,error)&&addonUnderwearHidden(variant),
          "last group's separate preferences survive restart and can resume sharing");

    RecordFlags<characterCapacity> slots;
    for(size_t i=0;i<characterCapacity;++i){
        check(!slots.exchange(i,true)&&slots.test(i)&&slots.exchange(i,true),"one flag is independent at every bit position");
    }
    check(!slots.exchange(characterCapacity,true)&&!slots.test(characterCapacity),"out-of-range slots are rejected");
    for(size_t i=0;i<characterCapacity;++i){
        check(slots.exchange(i,false)&&!slots.test(i),"clearing a flag preserves later flags");
    }
    check(!slots.any(),"the mask is empty after all valid flags are cleared");
    std::printf("PASS: %zu slots, %zu add-on records, %zu manifests, scan %.2f ms.\n",
                characterCapacity,maximumAddonCharacters,scan.packs.size(),elapsed);
}

int wmain(int argc,wchar_t** argv){
    hex::testGameLayout()=&hex::layouts[0];  // the fixtures use this build's addresses
    check(argc==4||argc==5,"test needs a temporary folder and both starter pack formats");
    namespace fs=std::filesystem;
    const fs::path root=argv[1],pack=argv[2];
    const fs::path mounted=argv[3];
    const auto folder=root/L"Characters";
    const auto missing=hex::scanCharacterAddons(folder);
    check(missing.characters.empty()&&missing.messages.empty(),"an absent pack leaves the roster empty");
    fs::create_directories(folder);
    const auto manifest=root/L"manifest.tsv";
    {
        std::ofstream file(manifest);
        file<<"dmm-runtime\t1\nfile\tcharacter/hexappeal/characters/maegu-woosa.xml\t"<<mounted.u8string()<<'\n';
    }
    auto mountOnly=hex::scanCharacterAddons(folder,manifest);
    check(mountOnly.messages.empty()&&mountOnly.characters.size()==2,"the mounted ZIP payload activates both characters");
    const auto compatible=hex::scanCharacterAddons(folder,manifest,"1.0.5");
    check(compatible.packs.size()==1&&compatible.packs[0].accepted==2&&compatible.packs[0].version=="1.0.1"&&
          compatible.packs[0].detail=="Character records passed validation."&&compatible.packs[0].multiCharacter,
          "the add-on version is independent of the compatible core version");
    const auto current=hex::scanCharacterAddons(folder,manifest,HEX_APPEAL_BUILD_VERSION);
    check(current.characters.size()==2&&current.packs.size()==1&&current.packs[0].version=="1.0.1"&&
          current.packs[0].detail=="Character records passed validation."&&current.packs[0].multiCharacter,
          "the Woosa & Maegu 1.0.1 add-on works with the current core build");
    const auto blocked=hex::scanCharacterAddons(folder,manifest,"1.0.4");
    check(blocked.characters.empty()&&blocked.packs.size()==1&&blocked.packs[0].detail=="Requires Hex Appeal 1.0.5 or later.",
          "a declared newer minimum blocks only the incompatible patch");
    const auto normalized=root/L"dmm-normalized.xml";
    {
        std::ifstream source(mounted);
        std::ofstream destination(normalized,std::ios::binary);
        const std::string bom="\xef\xbb\xbf";
        destination<<"\xef\xbb\xbf";
        std::string line;
        if(std::getline(source,line)){
            if(line.compare(0,bom.size(),bom)==0)line.erase(0,bom.size());
            destination<<line<<"\r\n";
        }
        while(std::getline(source,line))destination<<line<<"\r\n";
    }
    {
        std::ofstream file(manifest);
        file<<"dmm-runtime\t1\nfile\tcharacter/hexappeal/characters/maegu-woosa.xml\t"<<normalized.u8string()<<'\n';
    }
    auto normalizedMount=hex::scanCharacterAddons(folder,manifest);
    check(normalizedMount.messages.empty()&&normalizedMount.characters.size()==2,
          "DMM's UTF-8 BOM and CRLF normalization preserves the mounted roster");
    fs::copy_file(pack,folder/L"maegu-woosa.tsv");
    auto migrated=hex::scanCharacterAddons(folder,manifest);
    check(migrated.messages.empty()&&migrated.characters.size()==2,"the mounted pack takes priority over a same-named manual preview");
    fs::remove(folder/L"maegu-woosa.tsv");
    {
        std::ofstream file(manifest);
        file<<"dmm-runtime\t1\n";
    }
    check(hex::scanCharacterAddons(folder,manifest).characters.empty(),"disabling the DMM route removes the roster even when the payload remains installed");
    fs::copy_file(pack,folder/L"starter.tsv");
    auto scan=hex::scanCharacterAddons(folder);
    check(scan.messages.empty()&&scan.characters.size()==2,"Maegu and Woosa starter manifest loads");
    const auto& maegu=scan.characters[0];
    const auto& woosa=scan.characters[1];
    check(maegu.recordKey==3452&&woosa.recordKey==3453&&maegu.defaults.size()==3&&
          woosa.defaults.size()==2&&maegu.defaults.at("Headwear").count("cd_phw_00_hel_0103"),
          "verified stock pieces map to the right character and slots");
    hex::initializeCharacterAddons(root);
    check(hex::addonTargetForRecord("NHW_Unique_Maegu_3452")==6&&hex::addonTargetForRecord("NHW_Unique_Woosa_3453")==7&&
          hex::addonTargetForRecord("NHW_Unique_Maegu_34520")==-1&&!hex::characterAddon(0),
          "exact names resolve outside the original roster");
    check(hex::enabledAddonTargets()==std::vector<int>{6,7},"detected characters are enabled by default");
    check(hex::enabledAddonTargetForAppearance(maegu.appearance.c_str())==6&&
          hex::enabledAddonTargetForAppearance(woosa.appearance.c_str())==7,
          "checked appearance fallback finds both enabled add-on characters");
    std::string error;
    hex::Profile profile;
    profile.height=.2f;
    profile.top=hex::Preset::Flat;
    check(hex::saveAddonBodyProfile(6,profile,error)&&hex::saveAddonUnderwearHidden(6,true,error),
          "character controls save independent preferences");
    check(hex::setCharacterAddonPackEnabled(0,false,error)&&hex::enabledAddonTargets().empty()&&
          hex::enabledAddonTargetForAppearance(maegu.appearance.c_str())==-1,
          "disabling a patch removes both characters from the active roster");
    hex::initializeCharacterAddons(root);
    check(hex::enabledAddonTargets().empty()&&hex::addonBodyProfile(6).profile==profile&&hex::addonUnderwearHidden(6)&&
          !hex::addonUnderwearHidden(7),"disabled states and individual preferences survive restart");
    check(hex::setCharacterAddonPackEnabled(0,true,error)&&hex::enabledAddonTargets()==std::vector<int>{6,7},
          "re-enabling restores both selector entries and their preferences");
    {
        const auto settings=root/L"default-enabled";
        fs::create_directories(settings/L"Characters");
        const auto writePack=[&](const char* directive){
            std::ifstream source(mounted);
            std::ofstream payload(settings/L"configured.xml");
            std::string line;
            std::getline(source,line);
            payload<<line<<'\n';
            std::getline(source,line);
            payload<<line<<'\n'<<directive<<'\n'<<source.rdbuf();
        };
        const auto routes=settings/L"manifest.tsv";
        {
            std::ofstream file(routes);
            file<<"dmm-runtime\t1\nfile\tcharacter/hexappeal/characters/configured.xml\t"
                <<(settings/L"configured.xml").u8string()<<'\n';
        }
        writePack("#defaultEnabled\t0");
        hex::initializeCharacterAddons({},routes);
        check(hex::characterAddons().size()==2&&hex::enabledAddonTargets().empty(),
              "the mounted pack default also applies without a settings path");
        hex::initializeCharacterAddons(settings,routes);
        check(hex::characterAddonPacks().size()==1&&hex::characterAddonPacks()[0].accepted==2&&
              !hex::characterAddonPackEnabled(0)&&hex::enabledAddonTargets().empty()&&
              hex::enabledAddonTargetForAppearance(maegu.appearance.c_str())==-1,
              "a default-off mounted pack stays available to enable and its characters start inactive");
        check(hex::setCharacterAddonPackEnabled(0,true,error),"a default-off pack can be enabled");
        hex::initializeCharacterAddons(settings,routes);
        check(hex::enabledAddonTargets()==std::vector<int>{6,7},"saved on takes precedence over default off");
        writePack("#defaultEnabled\t1");
        hex::initializeCharacterAddons({},routes);
        check(hex::enabledAddonTargets()==std::vector<int>{6,7},"explicit default on enables both records");
        hex::initializeCharacterAddons(settings,routes);
        check(hex::setCharacterAddonPackEnabled(0,false,error),"a default-on pack can be disabled");
        hex::initializeCharacterAddons(settings,routes);
        check(hex::enabledAddonTargets().empty(),"saved off takes precedence over default on");
        writePack("#defaultEnabled\t2");
        const auto badDefault=hex::scanCharacterAddons(settings/L"Characters",routes);
        check(badDefault.characters.size()==2&&badDefault.packs[0].defaultEnabled&&badDefault.messages.size()==1&&
              badDefault.messages[0].find("ignored #defaultEnabled")!=std::string::npos,
              "an invalid default is reported without dropping a valid pack");
        std::puts("PASS: pack default off/on, saved choices in both directions, and invalid default fallback.");
    }
    {
        std::ofstream bad(folder/L"bad.tsv");
        bad<<"HexAppealCharacters\t1\ncharacter\tbad\tBad\t1002939\tNHW_Example_03\t"
              "character/appearance/npc/other.app_xml\ndefault\tbad\tRobe\texample_ub_robe\n";
        std::ofstream protectedBody(folder/L"body.tsv");
        protectedBody<<"HexAppealCharacters\t1\ncharacter\tbody\tBody\t10001\tNHW_Example_09\t"
                      "character/appearance/npc/body.app_xml\ndefault\tbody\tBody\tphw_00_nude_00_0001\n";
        std::ofstream traversal(folder/L"traversal.tsv");
        traversal<<"HexAppealCharacters\t1\ncharacter\tbad\tBad\t10001\tNHW_Example_09\t"
                  "character/appearance/../body.app_xml\n";
    }
    scan=hex::scanCharacterAddons(folder);
    check(scan.characters.size()==2&&scan.messages.size()==3,"invalid packs leave valid characters active");
    {
        std::ofstream duplicate(folder/L"duplicate.tsv");
        duplicate<<"HexAppealCharacters\t1\ncharacter\tclashing\tAnother Maegu\t3452\tNHW_Example_01\t"
                   "character/appearance/npc/clash.app_xml\ndefault\tclashing\tRobe\texample_ub_robe\n";
    }
    scan=hex::scanCharacterAddons(folder);
    check(scan.characters.size()==1&&scan.characters.front().id=="ha_woosa"&&scan.messages.size()==5,
          "duplicate ownership is disabled while unrelated characters remain available");
    const auto invalidMode=root/L"invalid-mode";
    fs::create_directories(invalidMode);
    {
        std::ofstream invalid(invalidMode/L"invalid.tsv");
        invalid<<"HexAppealCharacters\t1\n#multiCharacter\t2\n";
    }
    const auto rejectedMode=hex::scanCharacterAddons(invalidMode);
    check(rejectedMode.characters.empty()&&rejectedMode.messages.size()==1&&
          rejectedMode.messages.front().find("one tab and 0 or 1")!=std::string::npos,
          "an invalid explicit mode reports the correct pack option");
    if(argc==5){
        // Every published character pack loads alongside the others without identity conflicts.
        const auto everyPack=root/L"every-pack"/L"Characters";
        fs::create_directories(everyPack);
        size_t files=0;
        for(const auto& entry:fs::recursive_directory_iterator(fs::path(argv[4])))
            if(entry.path().extension()==L".tsv"&&entry.path().parent_path().filename()==L"Characters"){
                fs::copy_file(entry.path(),everyPack/entry.path().filename());++files;
            }
        const auto all=hex::scanCharacterAddons(everyPack,{},HEX_APPEAL_BUILD_VERSION);
        for(const auto& message:all.messages)std::fprintf(stderr,"%s\n",message.c_str());
        check(files>=6&&all.messages.empty()&&all.packs.size()==files&&all.characters.size()==files+4,
              "every character pack in addons/ loads together without conflicts");
        const auto naira=std::find_if(all.characters.begin(),all.characters.end(),[](const auto& c){return c.id=="ha_naira";});
        check(naira!=all.characters.end()&&naira->recordKey==1004236&&naira->record=="Nairah"&&
              naira->defaults.size()==4&&naira->defaults.at("Cloak").count("cd_phw_00_cloak_00_0137_t"),
              "Naira's pack names her record, player-class appearance and four stock slots");
        hex::initializeCharacterAddons(everyPack.parent_path());
        const int mainNaira=hex::addonTargetForRecord("Nairah");
        const int mainRucchia=hex::addonTargetForRecord("NHW_Unique_Lucia_429");
        check(mainNaira>=0&&mainRucchia>=0,"both original roster entries still resolve");
        const int naira412=hex::addonTargetForRecord("NHW_Unique_Nairah_DualSword_412");
        const int naira444=hex::addonTargetForRecord("NHW_Unique_Nairah_DualSword_444");
        const int stranger=hex::addonTargetForRecord("NHW_Unique_Lucia_Catchfire_Forest");
        const auto contains=[](const std::vector<int>& list,int target){return std::find(list.begin(),list.end(),target)!=list.end();};
        // By default a character's records share one set of settings and one character-list entry.
        const auto roster=hex::rosterAddonTargets();
        check(contains(roster,mainNaira)&&contains(roster,mainRucchia)&&!contains(roster,naira412)&&!contains(roster,stranger),
              "the character list shows each character once");
        check(hex::addonGroupShared(mainNaira)&&hex::addonGroupShared(mainRucchia)&&
              hex::addonGroupTargets(mainNaira)==std::vector<int>{mainNaira,naira412,naira444}&&
              hex::addonGroupTargets(mainRucchia)==std::vector<int>{mainRucchia,stranger}&&
              hex::addonSettingsTarget(naira444)==mainNaira&&hex::addonSettingsTarget(stranger)==mainRucchia,
              "Naira's and Rucchia's records use their main entry's settings by default");
        auto shared=profile;
        shared.height=.1f;
        check(hex::saveAddonBodyProfile(naira412,shared,error)&&hex::addonBodyProfile(mainNaira).profile==shared&&
              hex::addonBodyProfile(naira444).profile==shared,"a shape saved from any record applies to all of the character's records");
        check(hex::saveAddonUnderwearHidden(naira444,true,error)&&hex::addonUnderwearHidden(mainNaira)&&hex::addonUnderwearHidden(naira412),
              "underwear settings are shared too");
        const auto owners=hex::enabledAddonSettingsTargets();
        check(contains(owners,mainNaira)&&!contains(owners,naira412)&&!contains(owners,stranger),
              "ALL CHARACTERS edits each shared character once");
        // With sharing off, each record starts from the shared look and then keeps its own settings.
        check(hex::setAddonGroupShared(mainNaira,false,error)&&hex::setAddonGroupShared(mainRucchia,false,error)&&
              !hex::addonGroupShared(mainNaira)&&hex::addonBodyProfile(naira412).profile==shared&&
              hex::addonUnderwearHidden(naira444)&&contains(hex::enabledAddonSettingsTargets(),naira412),
              "turning sharing off copies the shared settings to each record");
        struct Variant {const char* record;uint32_t key;int parent;};
        const Variant variants[]{
            {"NHW_Unique_Nairah_DualSword_412",412,mainNaira},
            {"NHW_Unique_Nairah_DualSword_444",444,mainNaira},
            {"NHW_Unique_Lucia_Catchfire_Forest",1000212,mainRucchia},
        };
        for(const auto& variant:variants){
            const int target=hex::addonTargetForRecord(variant.record);
            const auto* entry=hex::characterAddon(target);
            const auto* parent=hex::characterAddon(variant.parent);
            check(entry&&target!=variant.parent&&entry->recordKey==variant.key&&entry->pack==parent->pack&&
                  entry->appearance==parent->appearance&&entry->defaults==parent->defaults,
                  "an alternate record gets its own target with the same stock equipment");
            check(hex::enabledAddonTargetForAppearance(entry->appearance.c_str())==-1,
                  "a shared appearance never picks an arbitrary record");
            const auto before=hex::addonBodyProfile(variant.parent);
            check(hex::saveAddonBodyProfile(target,profile,error)&&hex::addonBodyProfile(target).profile==profile&&
                  hex::addonBodyProfile(variant.parent).profile==before.profile,
                  "with sharing off, a record's shape leaves the main record unchanged");
        }
        hex::initializeCharacterAddons(everyPack.parent_path());
        check(!hex::addonGroupShared(mainNaira)&&!hex::addonGroupShared(mainRucchia)&&hex::addonBodyProfile(naira444).profile==profile,
              "the sharing choice and each record's settings survive a restart");
        const auto revision=hex::addonBodyProfile(naira444).revision;
        check(hex::setAddonGroupShared(mainNaira,true,error)&&hex::addonBodyProfile(naira444).profile==hex::addonBodyProfile(mainNaira).profile&&
              hex::addonBodyProfile(naira444).revision!=revision,
              "turning sharing back on returns every record to the main settings as a new shape revision");
    }
    checkCapacity(root/L"capacity");
    std::puts("PASS: optional character roster, stock equipment, exact identity and invalid pack isolation.");
}
