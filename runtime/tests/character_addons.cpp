#include "character_addons.h"
#include "release_version.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

static void check(bool value,const char* message){
    if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int wmain(int argc,wchar_t** argv){
    check(argc==4,"test needs a temporary folder and both starter pack formats");
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
    check(compatible.packs.size()==1&&compatible.packs[0].accepted==2&&compatible.packs[0].version=="1.0.0"&&
          compatible.packs[0].detail=="Character records passed validation.",
          "the add-on version is independent of the compatible core version");
    const auto current=hex::scanCharacterAddons(folder,manifest,HEX_APPEAL_BUILD_VERSION);
    check(current.characters.size()==2&&current.packs.size()==1&&current.packs[0].version=="1.0.0"&&
          current.packs[0].detail=="Character records passed validation.",
          "the published 1.0.0 add-on works with the current core build");
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
    std::puts("PASS: optional character roster, stock equipment, exact identity and invalid pack isolation.");
}
