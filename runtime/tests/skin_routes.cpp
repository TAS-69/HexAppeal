#include "game_archive.h"
#include "skin_dds.h"
#include "skin_tone.h"
#include "mounted_dependencies.h"
#include "dependency_fixture.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>

int wmain(int argc,wchar_t** argv){
    if(argc!=3&&argc!=4)return 1;
    using namespace hex;
    namespace fs=std::filesystem;
    const fs::path game=argv[1],fixture=argv[2];
    try{
        fs::create_directories(fixture);
        auto routes=readMountedRoutes(test::venusManifestFixture(game/L"bin64/dmm/manifest.tsv",fixture/L"venus"));
        const std::string bodyRoute="character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac";
        const std::string damianeRoute="character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml";
        const std::string torso="character/texture/phw_00_nude_00_0001.dds";
        const std::string neck="character/texture/phw_00_head_00_0001_01.dds";
        const std::string hands="character/texture/phw_00_nude_00_0001_hand.dds";
        const std::string bodyMaterial="character/modelproperty/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac_xml";
        for(const auto& path:{torso,neck,hands,bodyMaterial})routes.erase(path);
        if(!mountedDamianeVenus(routes)&&argc==4)routes[bodyRoute]=argv[3];
        const GameArchive archive(game/L"0009");
        const std::vector<std::string> addonLooks={
            "character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20113.app_xml",
            "character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20114.app_xml",
            damianeRoute
        };
        auto cleanAddons=routes;
        for(const auto& route:addonLooks)if(route!=damianeRoute)cleanAddons.erase(route);
        const auto fitted=prepareAddonVenus(game,cleanAddons,addonLooks);
        if(fitted.appearances.size()!=2||fitted.appearances.count(damianeRoute)||!fitted.hasBaseUnderwear)
            throw std::runtime_error("Expected both PHW add-ons and their base underwear on the VENUS mount.");
        for(const auto& [route,body]:fitted.appearances){
            const auto before=archive.read(route);
            const std::regex original("Name=\"phw_00_nude_00_0001\"",std::regex::icase);
            if(std::regex_replace(before,original,"Name=\"phw_00_nude_00_0001_damian\"")!=body)
                throw std::runtime_error("An add-on body change altered non-body appearance data.");
        }
        auto incompatibleAddons=cleanAddons;
        const auto customLook=fixture/L"custom-addon.app_xml";
        std::ofstream(customLook)<<"<Appearance><Nude><Prefab Name=\"other_body\"/></Nude></Appearance>";
        incompatibleAddons[addonLooks[0]]=customLook;
        if(prepareAddonVenus(game,incompatibleAddons,addonLooks).appearances.size()!=1)
            throw std::runtime_error("The VENUS adapter must retain a separately configured add-on body.");
        auto source=activeDamianeSkin(routes);
        if(!source.custom()){
            const std::string stock="character/texture/phw_00_head_00_0111.dds";
            const auto sample=fixture/L"external-face.dds";
            const auto data=archive.read(stock);
            if(data.empty())throw std::runtime_error("Could not locate the stock face fixture.");
            std::ofstream(sample,std::ios::binary).write(data.data(),data.size());
            routes[stock]=sample;
            const auto head=routes.find("character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml");
            if(head!=routes.end())routes.erase(head);
        }
        auto plan=prepareSkinTone(game,routes);
        for(const auto& line:plan.skipped)std::cerr<<line<<'\n';
        if(!plan.source.custom()||plan.appearances.size()!=7||plan.files.size()!=38)
            throw std::runtime_error("Expected six private heads and seven isolated witch appearances.");
        if(argc==4){
            auto humanRoutes=routes;
            humanRoutes[bodyRoute]=argv[3];
            if(prepareSkinTone(game,humanRoutes).files!=plan.files)
                throw std::runtime_error("VENUS Human Body did not follow the same active face texture.");
        }
        auto missingBody=routes;
        missingBody.erase(bodyRoute);
        if(activeVenusDamianeSkin(missingBody).custom()||!prepareSkinTone(game,missingBody).files.empty())
            throw std::runtime_error("A custom face activated the overlay without VENUS Body for Damiane.");
        auto otherBody=routes;
        const auto alternateAppearance=fixture/L"other-damiane.app_xml";
        std::ofstream(alternateAppearance)<<"<Appearance><Nude><Prefab Name=\"other_body\"/></Nude>"
            "<Head><Prefab Name=\"phw_00_head_00_0111\"/></Head></Appearance>";
        otherBody[damianeRoute]=alternateAppearance;
        if(!prepareSkinTone(game,otherBody).files.empty())
            throw std::runtime_error("A mounted but unused VENUS body activated the override.");
        static const std::regex alias(R"(hxk_[a-z0-9_]+)");
        const auto get=[&](const std::string& route){
            const auto mounted=routes.find(route);
            if(mounted==routes.end())return archive.read(route);
            std::ifstream input(mounted->second,std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(input),{});
        };
        if(plan.files.count("character/appearance/1_pc/2_phw/cd_phw_sylvia/cd_phw_sylvia_00000.app_xml")||
           plan.files.count("character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml"))
            throw std::runtime_error("A player appearance was included in the skin overlay.");
        for(const auto& [route,data]:plan.files){
            if(route.find("character/texture/")==0)throw std::runtime_error("An original texture route was overridden.");
            if(route.find("character/model/")==0){
                auto original=route;
                original.replace(original.find("/ha_p/"),6,"/1_pc/");
                original.replace(original.find("hxk_"),3,"phw");
                if(data!=get(original))throw std::runtime_error("The head geometry was altered.");
            }
        }
        auto vanillaRoutes=routes;
        for(const auto& route:plan.appearances){
            const auto original=get(route);
            const auto& patched=plan.files.at(route);
            std::smatch match;
            if(!std::regex_search(patched,match,alias)||patched.size()!=original.size())
                throw std::runtime_error("An appearance changed outside its equal-length head reference.");
            auto restored=patched;
            restored.replace(static_cast<size_t>(match.position()),match.length(),
                             original.substr(static_cast<size_t>(match.position()),match.length()));
            if(restored!=original)throw std::runtime_error("The witch body or outfit changed with the face reference.");
            auto variant=original;
            const auto bodyAt=variant.find("<Nude>");
            if(bodyAt==variant.npos)throw std::runtime_error("Expected a default body reference.");
            const auto name=variant.find("Name=",bodyAt);
            if(name==variant.npos)throw std::runtime_error("Expected a named body reference.");
            const auto valueAt=name+6;
            const auto end=variant.find(variant[valueAt-1],valueAt);
            if(end==variant.npos)throw std::runtime_error("Expected a complete body reference.");
            variant.replace(valueAt,end-valueAt,test::vanillaBodyFor(route));
            const auto file=fixture/fs::path(route).filename();
            std::ofstream(file,std::ios::binary).write(variant.data(),variant.size());
            vanillaRoutes[route]=file;
        }
        if(mountedBodyMode(vanillaRoutes,nullptr,nullptr)!=BodyMode::Vanilla||
           activeVenusDamianeSkin(vanillaRoutes).custom()||!prepareSkinTone(game,vanillaRoutes).files.empty()||
           !prepareAddonVenus(game,vanillaRoutes,addonLooks).appearances.empty())
            throw std::runtime_error("Vanilla witches accepted the override with VENUS mounted for Damiane.");
        const auto toneSample=fixture/L"external-damiane.paccd_xml";
        auto toneRoutes=routes;
        toneRoutes.erase(plan.source.route);
        toneRoutes[damianeCustomizationRoute]=toneSample;
        const auto setTone=[&](unsigned brightness){
            std::ofstream(toneSample)<<"<CharacterCustomization Version=\"0\">\n<DecorationParam>\n"
                "<Data UIKey=\"skinColor\" Value=\"9\"/>\n<Data UIKey=\"skinBrightness\" Value=\""
                <<brightness<<"\"/>\n</>\n<MeshParam>\n</>\n</>";
        };
        for(const unsigned brightness:{12u,98u}){
            setTone(brightness);
            const auto tonePlan=prepareSkinTone(game,toneRoutes);
            for(const auto& line:tonePlan.skipped)std::cerr<<line<<'\n';
            if(tonePlan.source.brightness!=brightness||tonePlan.source.colour!=9||
               !tonePlan.source.texture.empty()||!tonePlan.appearances.empty()||
               tonePlan.tones.size()!=7||tonePlan.files.size()!=7)
                throw std::runtime_error("A customization-only source did not cover the seven witch records.");
            for(const auto& [path,patched]:tonePlan.files){
                const auto original=get(path);
                if(original.empty()||path==damianeCustomizationRoute||
                   path.find("character/descriptors/customization/")!=0)
                    throw std::runtime_error("A tone-only overlay targeted a non-witch route.");
                const auto expected=std::regex_replace(
                    std::regex_replace(original,std::regex("UIKey=\"skinBrightness\" Value=\"[0-9]+\""),
                                       "UIKey=\"skinBrightness\" Value=\""+std::to_string(brightness)+"\""),
                    std::regex("UIKey=\"skinColor\" Value=\"[0-9]+\""),"UIKey=\"skinColor\" Value=\"9\"");
                if(patched!=expected||patched==original)
                    throw std::runtime_error("An unrelated witch customization setting changed.");
            }
        }
        auto combined=routes;
        combined[damianeCustomizationRoute]=toneSample;
        const auto combinedPlan=prepareSkinTone(game,combined);
        if(combinedPlan.appearances.size()!=7||combinedPlan.tones.size()!=7||combinedPlan.files.size()!=45)
            throw std::runtime_error("The two skin sources could not apply together.");
        const auto expanded=prepareSkinTone(game,combined,fitted.appearances);
        for(size_t index=0;index<2;++index){
            const auto& look=addonLooks[index];
            const auto stem=std::filesystem::path(look).stem().string();
            const auto alias="cd_npc_south/ha_"+stem+"_customization";
            const auto& combinedAppearance=expanded.files.at(look);
            if(combinedAppearance.find(alias)==combinedAppearance.npos||
               combinedAppearance.find("hxk_00_head_")==combinedAppearance.npos||
               combinedAppearance.find("phw_00_nude_00_0001_damian")==combinedAppearance.npos||
               expanded.files.at("character/descriptors/customization/"+alias+".paccd_xml").find("UIKey=\"skinBrightness\" Value=\"98\"")==std::string::npos)
                throw std::runtime_error("An add-on did not inherit the common VENUS face and private customization tone.");
        }
        if(expanded.files.count("character/descriptors/customization/cd_npc_south/cd_nhw_south_20067_customization.paccd_xml"))
            throw std::runtime_error("Matching Maegu's tone changed a shared customization.");
        vanillaRoutes[damianeCustomizationRoute]=toneSample;
        if(!prepareSkinTone(game,vanillaRoutes).files.empty())
            throw std::runtime_error("Vanilla witches accepted a custom tone.");
        const std::string privateBody="character/model/ha_p/2_phw/nude/hxt_00_nude_00_0001_damian.pac";
        const std::string privateMaterial="character/modelproperty/ha_p/2_phw/nude/hxt_00_nude_00_0001_damian.pac_xml";
        const auto privateColour=[](const std::string& route){
            return "character/texture/hexappeal_skin_"+std::filesystem::path(route).filename().u8string();
        };
        const auto writeTint=[&](const std::string& route,const SkinGain& gain){
            const auto path=fixture/fs::path(route).filename();
            const auto data=adjustSkinColour(archive.read(route),gain);
            if(data.empty())throw std::runtime_error("Could not prepare the test colour for "+route+".");
            std::ofstream(path,std::ios::binary).write(data.data(),data.size());
            return path;
        };
        for(const float scale:{0.76f,1.24f}){
            auto partial=routes;
            partial.erase(plan.source.route);
            partial[torso]=writeTint(torso,{scale,scale,scale});
            const auto full=prepareSkinTone(game,partial);
            if(full.appearances.size()!=7||!full.privateBodies||full.files.at(privateBody)!=get(bodyRoute)||
               full.files.count(torso)||full.files.count(neck)||full.files.count(hands)||
               full.files.count(damianeRoute))
                throw std::runtime_error("A torso-only pack did not isolate the witch body geometry and appearances.");
            const auto& material=full.files.at(privateMaterial);
            for(const auto& route:{neck,hands}){
                const auto alias=privateColour(route);
                const auto found=full.files.find(alias);
                if(found==full.files.end()||material.find(alias)==material.npos||
                   !compareSkinColour(archive.read(route),found->second))
                    throw std::runtime_error("The missing skin region was not coloured: "+route);
            }
            const auto privateFace=privateColour("character/texture/phw_00_head_00_0111.dds");
            if(!full.files.count(privateFace)||material.find(torso)==material.npos||
               std::find(full.sources.begin(),full.sources.end(),torso)==full.sources.end())
                throw std::runtime_error("The full skin plan lost its mounted torso tone or generated face.");

            if(scale<1.0f){
                auto mixed=partial;
                const auto put=[&](const std::string& route,const std::string& bytes){
                    const auto file=fixture/("mixed-"+fs::path(route).filename().string());
                    std::ofstream(file,std::ios::binary).write(bytes.data(),bytes.size());
                    mixed[route]=file;
                };
                const std::string alias="hva_00_nude_00_0001_damian";
                const auto isolated=[](const std::string& data){
                    return std::regex_replace(data,std::regex("hxt_00_nude_00_0001_damian"),"hva_00_nude_00_0001_damian");
                };
                const std::string prefabDir="character/bin__/prefab/ha_p/02_phw/nude/";
                const std::string metadataDir="character/prefab/ha_p/02_phw/nude/";
                const std::string modelDir="character/model/ha_p/2_phw/nude/";
                const std::string physicsDir="character/bin__/meshphysics/ha_p/2_phw/nude/";
                const std::string materialDir="character/modelproperty/ha_p/2_phw/nude/";
                const std::string hxt="hxt_00_nude_00_0001_damian";
                put(skinRegistryRoute,std::regex_replace(isolated(full.files.at(skinRegistryRoute)),
                    std::regex("hxk_00_head_"),"hzz_00_head_"));
                put(prefabDir+alias+".prefab",isolated(full.files.at(prefabDir+hxt+".prefab")));
                put(metadataDir+alias+".prefabdata_xml",full.files.at(metadataDir+hxt+".prefabdata_xml"));
                put(modelDir+alias+".pac",full.files.at(privateBody));
                put(physicsDir+alias+".hkx",full.files.at(physicsDir+hxt+".hkx"));
                const std::string localTorso="character/texture/hva_00_nude_00_0001.dds";
                const std::string localHands="character/texture/hva_00_nude_00_0001_hand.dds";
                auto localMaterial=get(bodyMaterial);
                localMaterial.replace(localMaterial.find(torso),torso.size(),localTorso);
                localMaterial.replace(localMaterial.find(hands),hands.size(),localHands);
                put(materialDir+alias+".pac_xml",localMaterial);
                put(localTorso,archive.read(torso));
                put(localHands,archive.read("character/texture/phw_00_head_00_0111.dds"));
                const std::string witch="character/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml";
                put(witch,std::regex_replace(get(witch),std::regex("Name=\"phw_00_nude_00_0001_damian\""),"Name=\""+alias+"\""));

                const auto compatible=prepareSkinTone(game,mixed);
                const auto& actual=compatible.files.at(materialDir+"hxu_00_nude_00_0001_damian.pac_xml");
                if(compatible.files.at(witch).find("hxk_00_head_")==std::string::npos||
                   actual.find(localHands)==actual.npos||actual.find(torso)==actual.npos||
                   actual.find(privateColour(neck))==actual.npos||
                   std::none_of(compatible.skipped.begin(),compatible.skipped.end(),[](const auto& message){
                       return message.find("cd_phm_00_nude_0001_hand_damian")!=message.npos;
                   }))
                    throw std::runtime_error("An incompatible local hand route blocked compatible torso or collar changes.");
            }

            partial[hands]=writeTint(hands,{scale,scale,scale});
            const auto withHands=prepareSkinTone(game,partial);
            if(withHands.files.count(privateColour(hands))||withHands.files.at(privateMaterial).find(hands)==std::string::npos)
                throw std::runtime_error("An explicitly installed hand colour was replaced.");
            partial[neck]=writeTint(neck,{scale,scale,scale});
            const auto complete=prepareSkinTone(game,partial);
            if(complete.privateBodies||complete.files.count(privateMaterial)||
               complete.files.count(privateColour(hands))||complete.files.count(privateColour(neck)))
                throw std::runtime_error("Shared skin routes were cloned despite already using Damiane’s colours.");
        }
        routes.erase(plan.source.route);
        if(!prepareSkinTone(game,routes).files.empty())
            throw std::runtime_error("Stock textures activated the witch override.");
        std::cout<<"PASS: seven private witch appearances with active VENUS and a custom face texture. "
                 <<"Customization-only light and dark exports alter only the seven witch tones. "
                 <<"Torso-only light and dark packs cover the face, collar and hands; explicit textures take priority. "
                 <<"Vanilla witches, missing or unused VENUS, and stock skin leave the override empty.\n";
    }catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
