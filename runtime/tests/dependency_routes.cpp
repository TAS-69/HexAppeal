#include "mounted_dependencies.h"
#include "runtime.h"
#include "venus_bodies.h"
#include "dependency_fixture.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int wmain(int argc,wchar_t** argv) {
    if(argc!=3&&argc!=4)return 1;
    const std::filesystem::path game=argv[1],temporary=argv[2];std::string error;
    if(!hex::verifyMountedDependencies(game,error)){std::cerr<<error<<'\n';return 1;}
    {
        const auto folder=temporary/L"custom-skin";
        std::filesystem::create_directories(folder);
        const auto texture=folder/L"face.dds",material=folder/L"face.pac_xml",appearance=folder/L"player.app_xml";
        const auto save=[](const auto& path,const std::string& contents){std::ofstream file(path,std::ios::binary);file<<contents;};
        const std::string stock="character/texture/phw_00_head_00_0111.dds";
        const std::string custom="character/texture/custom_player_face.dds";
        const std::string materialRoute="character/modelproperty/1_pc/2_phw/head/head/phw_00_head_00_0099.pac_xml";
        const std::string appearanceRoute="character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml";
        std::map<std::string,std::filesystem::path> routes{{"character/texture/unrelated_outfit.dds",texture}};
        if(hex::activeDamianeSkin(routes).custom())return 1;
        save(texture,"DDS ");
        routes[stock]=texture;
        if(hex::activeDamianeSkin(routes).custom())return 1;
        std::array<uint32_t,36> dds{};
        dds[0]=0x20534444;dds[1]=124;dds[3]=dds[4]=4;dds[19]=32;
        save(texture,std::string(reinterpret_cast<const char*>(dds.data()),sizeof dds));
        if(!hex::activeDamianeSkin(routes).custom())return 1;
        save(appearance,"<Appearance><Head><Prefab Name=\"PHW_00_HEAD_00_0099\"/></Head></Appearance>");
        routes[appearanceRoute]=appearance;
        if(hex::activeDamianeSkin(routes).custom())return 1;
        save(material,"<MaterialInformation/><Material _materialName=\"SkinnedMeshSkinWrinkle\">"
            "<Parameters><MaterialParameterTexture _name=\"_baseColorTexture\"><ResourceReferencePath_ITexture _path=\""+
            custom+"\"/></MaterialParameterTexture></Parameters></Material>");
        routes[materialRoute]=material;
        if(hex::activeDamianeSkin(routes).custom())return 1;
        routes[custom]=texture;
        const auto current=hex::activeDamianeSkin(routes);
        if(!current.custom()||current.route!=custom||current.texture!=texture)return 1;
        const auto active=hex::activeDamianeSkin(hex::readMountedRoutes(game/L"bin64/dmm/manifest.tsv"));
        std::cout<<"PASS: custom-skin source detection only follows Damiane's active DDS. Current mount: "
                 <<active.detail<<" "<<active.route<<'\n';
    }
    const auto reference=hex::test::venusManifestFixture(game/L"bin64/dmm/manifest.tsv",temporary/L"venus-reference");
    std::ifstream input(reference);std::ostringstream original;original<<input.rdbuf();
    if(argc==4){
        const auto human=std::filesystem::absolute(argv[3]);const auto* version=hex::venusBodyVersion(hex::sha256(human));
        if(!version||std::string(version->variant)!="Human Body"){std::cerr<<"Supply the external checked VENUS 2.6 Human Body.\n";return 1;}
        std::istringstream rows(original.str());std::ostringstream changed;std::string line;unsigned aliases=0;
        while(std::getline(rows,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            const auto tab=line.find('\t',5);
            if(line.rfind("file\t",0)==0&&tab!=std::string::npos){
                const auto key=line.substr(5,tab-5);
                if(key=="character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac"||key=="character/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac"){line.replace(tab+1,std::string::npos,human.u8string());++aliases;}
            }
            changed<<line<<'\n';
        }
        if(aliases<1){std::cerr<<"The primary active body route is required.\n";return 1;}
        const auto humanManifest=temporary/L"human-body/bin64/dmm/manifest.tsv";std::filesystem::create_directories(humanManifest.parent_path());
        {std::ofstream out(humanManifest,std::ios::binary);out<<changed.str();}
        std::string warning;
        if(!hex::verifyMountedDependencies(game,humanManifest,error,warning)||!warning.empty()){std::cerr<<"Human Body routes: "<<error<<warning<<'\n';return 1;}
        hex::DependencyPaths paths;paths.manifest=humanManifest;
        const auto humanStatus=hex::inspectDependencies(game,paths);
        if(!humanStatus.bodyVerified||!humanStatus.venus.verified||humanStatus.venus.path!=human||humanStatus.venus.detail.find("Human Body")==std::string::npos){std::cerr<<"Human Body detection was not verified.\n";return 1;}
        paths.venusBody=human;const auto manual=hex::inspectDependencies(game,paths);
        if(!manual.bodyVerified||!manual.venus.verified){std::cerr<<"Explicit Human Body path was rejected.\n";return 1;}
        std::istringstream humanRows(changed.str());std::ostringstream primary;
        const std::string alias="file\tcharacter/model/1_pc/2_phw/nude/cd_phw_00_nude_00_0001_damian.pac\t";
        while(std::getline(humanRows,line))if(line.rfind(alias,0)!=0)primary<<line<<'\n';
        const auto primaryManifest=temporary/L"human-primary-only.tsv";
        {std::ofstream out(primaryManifest,std::ios::binary);out<<primary.str();}
        if(!hex::verifyMountedDependencies(game,primaryManifest,error,warning)||!warning.empty()){std::cerr<<"Valid primary-only Human Body installation was blocked.\n";return 1;}
        const auto aliasManifest=temporary/L"human-with-alias.tsv";
        {std::ofstream out(aliasManifest,std::ios::binary);out<<primary.str()<<alias<<human.u8string()<<'\n';}
        if(!hex::verifyMountedDependencies(game,aliasManifest,error,warning)||!warning.empty()){std::cerr<<"Valid Human Body with optional alias was blocked.\n";return 1;}
        std::cout<<"PASS: original external Human Body, primary-only and aliased installations, checked automatic/manual detection and unchanged witch appearance routes.\n";
    }
    const auto manifest=temporary/L"bin64/dmm/manifest.tsv";
    std::filesystem::create_directories(manifest.parent_path());
    {
        const auto selected=temporary/L"ambiguous-routes.tsv";
        std::string warning;
        const auto shared=original.str()+
            "file\tui/texture/test.dds\ta.dds\nfile\tui/texture/test.dds\tb.dds\n";
        {std::ofstream out(selected,std::ios::binary);out<<shared;}
        if(!hex::verifyMountedDependencies(game,selected,error,warning)){
            std::cerr<<"An unrelated route conflict blocked shaping: "<<error<<'\n';return 1;
        }
        {std::ofstream out(selected,std::ios::binary);out<<shared<<
            "file\tcharacter/appearance/3_npc/cd_npc_south/cd_nhw_south/cd_nhw_south_20166.app_xml\tconflict.app_xml\n";}
        uint32_t unavailable=0;
        if(!hex::verifyMountedDependencies(game,selected,error,warning,&unavailable)||unavailable!=1||
           warning.find("Lyselia")==std::string::npos){
            std::cerr<<"An ambiguous witch route failed to isolate the affected witch: "<<error<<warning<<'\n';return 1;
        }
        {std::ofstream out(selected,std::ios::binary);out<<shared<<
            "file\tcharacter/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac\tconflict.pac\n";}
        if(hex::verifyMountedDependencies(game,selected,error,warning,&unavailable)){
            std::cerr<<"An ambiguous shared body model was accepted.\n";return 1;
        }
    }
    {
        const auto folder=temporary/L"stock-body-clone";std::filesystem::create_directories(folder);
        const auto selected=folder/L"manifest.tsv",appearance=folder/L"look.app_xml";
        const auto prefab=folder/L"clone.prefab",metadata=folder/L"clone.prefabdata_xml";
        const auto save=[](const auto& path,const std::string& contents){std::ofstream file(path,std::ios::binary);file<<contents;};
        const std::string name="private_npc_base";
        save(appearance,"<Appearance><Nude><Prefab Name=\""+name+"\"/></Nude></Appearance>");
        const std::string mesh=std::string("\xff\xff\x04\0\0\0",6)+"SkinnedMeshComponent character/model/1_pc/2_phw/nude/phw_00_nude_00_0001.pac";
        const std::string underwear="character/model/1_pc/2_phw/armor/38_underwear/phw_00_uw_00_0001.pac";
        constexpr char part[]="\x09\0\0\0" "Underwear" "\x01\x01\0\x01\x03\0\0"
            "\xff\xff\xff\xff\xff\xff\xff\xff" "\x01\0\0\0\0\0\0\0" "\x43\0\0\0";
        const auto prefabStart=mesh+std::string(part,sizeof(part)-1);
        save(prefab,prefabStart+underwear);
        save(folder/L"modinfo.json","{\"title\":\"NPC body fixture\",\"generator\":\"Test NPC generator\"}");
        const std::string skeleton="<SkeletonName FileName=\"1_pc/2_phw/PHW_01.pab\"/>";
        const std::string variation="<SkeletonVariationName FileName=\"1_PC/2_PHW/Nude/PHW_00_Nude_00_0001.pabc\"/>";
        save(metadata,"<NudePrefabData>"+skeleton+variation+"</NudePrefabData>");
        std::istringstream rows(original.str());std::ostringstream changed;std::string line;
        while(std::getline(rows,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            const auto tab=line.find('\t',5);
            if(tab!=std::string::npos&&line.substr(5,tab-5).find("cd_nhw_south_20166.app_xml")!=std::string::npos)line.replace(tab+1,std::string::npos,appearance.u8string());
            changed<<line<<'\n';
        }
        const auto prefabRow="file\tcharacter/bin__/prefab/example/npc/nude/"+name+".prefab\t"+prefab.u8string()+'\n';
        const auto metadataRow="file\tcharacter/prefab/example/npc/nude/"+name+".prefabdata_xml\t"+metadata.u8string()+'\n';
        save(selected,changed.str()+prefabRow+metadataRow);
        std::string warning;uint32_t clones=0;
        if(hex::mountedBodyMode(selected,&clones)!=hex::BodyMode::Venus||clones!=1||
           !hex::verifyMountedDependencies(game,selected,error,warning)||warning.find("body replacement")==std::string::npos){
            std::cerr<<"A mounted stock body clone blocked VENUS or was attributed to the wrong witch.\n";return 1;
        }
        hex::DependencyPaths paths;paths.manifest=selected;
        const auto keep=hex::inspectDependencies(game,paths);
        save(prefab,prefabStart+std::string(underwear.size(),'\0'));
        const auto remove=hex::inspectDependencies(game,paths);
        if(!remove.bodyVerified||keep.missingUnderwearMeshMask&1u||remove.missingUnderwearMeshMask!=(keep.missingUnderwearMeshMask|1u)||
           remove.underwearSources[0]!="Test NPC generator"){
            std::cerr<<"The cleared underwear path did not establish ownership for the affected witch only.\n";return 1;
        }
        save(metadata,"<NudePrefabData>"+skeleton+"<SkeletonVariationName FileName=\"unrelated.pabc\"/></NudePrefabData>");
        if(hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"A different rig was accepted for the body clone.\n";return 1;}
        auto diagnosticPaths=paths;
        diagnosticPaths.modsDirectory=temporary;
        const auto partial=hex::inspectDependencies(game,diagnosticPaths);
        const auto& cause=partial.unavailableBodyDetails[0];
        if(!partial.bodyVerified||partial.unavailableBodyWitches!=1||partial.bodyWarning.find("Lyselia")==std::string::npos||
           cause.find(name)==std::string::npos||cause.find("cd_nhw_south_20166.app_xml")==std::string::npos||
           cause.find("NPC body fixture")==std::string::npos||!partial.unavailableBodyDetails[3].empty()){
            std::cerr<<"An unsupported Lyselia replacement blocked other witches or was reported incorrectly.\n";return 1;
        }
        std::istringstream mixedRows(changed.str());std::ostringstream mixed;
        const auto vanillaLook=folder/L"vanilla-bari.app_xml";
        save(vanillaLook,"<Appearance><Nude><Prefab Name=\"hvw_00_nude_00_0301\"/></Nude></Appearance>");
        while(std::getline(mixedRows,line)){
            const auto tab=line.find('\t',5);
            if(tab!=std::string::npos&&line.substr(5,tab-5).find("cd_nhw_north_20026.app_xml")!=std::string::npos)
                line.replace(tab+1,std::string::npos,vanillaLook.u8string());
            mixed<<line<<'\n';
        }
        save(selected,mixed.str()+prefabRow+metadataRow);
        if(hex::inspectDependencies(game,paths).bodyVerified){std::cerr<<"Conflicting Hex Appeal body options were accepted.\n";return 1;}
        save(metadata,"<NudePrefabData>"+skeleton+variation+"</NudePrefabData>");
        save(selected,changed.str()+metadataRow);
        if(hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"An unmounted body clone was accepted.\n";return 1;}
        const auto originalVenus="character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac";
        const auto privateRoute="file\tcharacter/bin__/prefab/ha_p/02_phw/nude/hva_00_nude_00_0001_damian.prefab\t";
        const auto privateDefinition="file\tcharacter/prefab/ha_p/02_phw/nude/hva_00_nude_00_0001_damian.prefabdata_xml\t";
        save(appearance,"<Appearance><Nude><Prefab Name=\"hva_00_nude_00_0001_damian\"/></Nude></Appearance>");
        save(prefab,std::string("\xff\xff\x04\0\0\0",6)+originalVenus+" "+underwear);
        save(selected,changed.str()+privateRoute+prefab.u8string()+"\n"+privateDefinition+metadata.u8string()+"\n");
        const auto isolated=hex::inspectDependencies(game,paths);
        if(!isolated.bodyVerified||(isolated.missingUnderwearMeshMask&1u)){
            std::cerr<<"The isolated VENUS prefab inherited the player remover.\n";return 1;
        }
        save(prefab,std::string("\xff\xff\x04\0\0\0",6)+originalVenus);
        if(hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"An isolated prefab without its underwear reference was accepted.\n";return 1;}
        const std::string privateModel="character/model/ha_p/2_phw/nude/hva_00_nude_00_0001_damian.pac";
        const std::string bodyColour="character/texture/hva_00_nude_00_0001.dds",handColour="character/texture/hva_00_nude_00_0001_hand.dds";
        const auto model=folder/L"local-model.pac",colours=folder/L"local-colour.dds",material=folder/L"local-material.pac_xml";
        save(prefab,std::string("\xff\xff\x04\0\0\0",6)+privateModel+" "+underwear);
        save(model,"PAR isolated fixture");save(colours,"DDS isolated fixture");save(material,bodyColour+" "+handColour);
        const auto skinRoutes=changed.str()+privateRoute+prefab.u8string()+"\n"+privateDefinition+metadata.u8string()+"\n"+
            "file\t"+privateModel+"\t"+model.u8string()+"\n"+
            "file\tcharacter/modelproperty/ha_p/2_phw/nude/hva_00_nude_00_0001_damian.pac_xml\t"+material.u8string()+"\n"+
            "file\tcharacter/bin__/meshphysics/ha_p/2_phw/nude/hva_00_nude_00_0001_damian.hkx\t"+metadata.u8string()+"\n"+
            "file\t"+bodyColour+"\t"+colours.u8string()+"\n";
        save(selected,skinRoutes+"file\t"+handColour+"\t"+colours.u8string()+"\n");
        if(!hex::verifyMountedDependencies(game,selected,error,warning)||warning.find("different VENUS model")==std::string::npos){
            std::cerr<<"A readable isolated skin must work with a warning if its source model differs.\n";return 1;
        }
        save(selected,skinRoutes);
        if(hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"An isolated skin with a missing colour route was accepted.\n";return 1;}
        uint32_t unavailable=0;
        if(!hex::verifyMountedDependencies(game,selected,error,warning,&unavailable)||unavailable!=1){std::cerr<<"A missing local alias must only block the actor using it.\n";return 1;}
        std::cout<<"PASS: a mounted stock PHW body clone produces a warning; only the affected witch delegates cleared underwear; wrong and missing clone resources are rejected.\n";
    }
    {
        const auto folder=temporary/L"vanilla";std::filesystem::create_directories(folder);
        const auto selected=folder/L"manifest.tsv";
        const auto save=[&](const std::filesystem::path& path,const std::string& text){std::ofstream file(path,std::ios::binary);file<<text;};
        std::istringstream rows(original.str());std::ostringstream changed;std::string line;unsigned looks=0;
        std::filesystem::path sylviaAppearance;
        while(std::getline(rows,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            const auto tab=line.find('\t',5);
            if(line.rfind("file\t",0)==0&&tab!=std::string::npos){
                const auto key=line.substr(5,tab-5);
                if(key.find("character/model/1_pc/2_phw/nude/")==0||
                   key.find("character/model/ha_p/2_phw/nude/")==0||
                   key.find("character/modelproperty/ha_p/2_phw/nude/")==0||
                   key.find("character/bin__/meshphysics/ha_p/2_phw/nude/")==0||
                   key=="character/descriptors/hexappeal/body-mode.txt")continue;
                if(key.rfind("character/appearance/",0)==0&&hex::test::witchAppearance(key)){
                    const auto app=folder/(std::to_wstring(looks++)+L".app_xml");
                    if(key.find("20169.app_xml")!=std::string::npos)sylviaAppearance=app;
                    const std::string body=key.find("20026.app_xml")!=std::string::npos?"hvw_00_nude_00_0301":key.find("20169.app_xml")!=std::string::npos?"hvw_00_nude_00_0001_damian":"hvw_00_nude_00_0001";
                    save(app,"<Appearance>\n<Nude>\n<Prefab CharacterScale='0.97' Name = '"+body+"'/>\n</Nude>\n</Appearance>");
                    line.replace(tab+1,std::string::npos,app.u8string());
                }
            }
            changed<<line<<'\n';
        }
        if(looks!=7){std::cerr<<"Expected seven mounted witch appearances in vanilla fixture.\n";return 1;}
        for(const std::string stem:{"hvw_00_nude_00_0001","hvw_00_nude_00_0001_damian","hvw_00_nude_00_0002"}){
            const auto model=folder/std::filesystem::u8path(stem+".pac");save(model,"PAR test fixture, never a model output");
            changed<<"file\tcharacter/model/ha_p/2_phw/nude/"<<stem<<".pac\t"<<model.u8string()<<'\n';
            const auto metadata=folder/std::filesystem::u8path(stem+".fixture");save(metadata,"test metadata fixture");
            changed<<"file\tcharacter/modelproperty/ha_p/2_phw/nude/"<<stem<<".pac_xml\t"<<metadata.u8string()<<'\n';
            changed<<"file\tcharacter/bin__/meshphysics/ha_p/2_phw/nude/"<<stem<<".hkx\t"<<metadata.u8string()<<'\n';
        }
        save(selected,changed.str());
        std::string warning;
        if(!hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"Explicit vanilla option needs no VENUS body: "<<error<<'\n';return 1;}
        hex::DependencyPaths paths;paths.manifest=selected;const auto status=hex::inspectDependencies(game,paths);
        if(!status.bodyVerified||!status.bodySourceReady()||status.venus.valid||status.bodyMode!=hex::BodyMode::Vanilla){std::cerr<<"Vanilla body discovery/gating failed.\n";return 1;}
        const auto remover=folder/L"No Underwear - NPCs";
        std::filesystem::create_directories(remover);
        const auto legacyUnderwear=remover/L"phw_00_uw_00_0001.pac";
        save(legacyUnderwear,"legacy shared underwear fixture");
        save(selected,changed.str()+"file\tcharacter/model/1_pc/2_phw/armor/38_underwear/phw_00_uw_00_0001.pac\t"+
            legacyUnderwear.u8string()+"\n");
        const auto withRemover=hex::inspectDependencies(game,paths);
        if(!withRemover.bodyVerified||std::any_of(withRemover.underwearSources.begin(),withRemover.underwearSources.end(),
           [](const auto& owner){return !owner.empty();})){
            std::cerr<<"An unreferenced shared VENUS underwear route claimed the isolated Vanilla witches.\n";return 1;
        }

        const auto npcPrefab=remover/L"npc.prefab",npcMetadata=remover/L"npc.prefabdata_xml";
        save(npcPrefab,std::string("\xff\xff\x04\0\0\0",6)+"Underwear character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac");
        save(npcMetadata,"<SkeletonName FileName=\"1_pc/2_phw/PHW_01.pab\"/>");
        auto mixedRoutes=hex::readMountedRoutes(selected);
        mixedRoutes["character/bin__/prefab/1_pc/02_phw/nude/phw_00_nude_00_0001_damian_lnw.prefab"]=npcPrefab;
        mixedRoutes["character/prefab/1_pc/02_phw/nude/phw_00_nude_00_0001_damian_lnw.prefabdata_xml"]=npcMetadata;
        save(sylviaAppearance,"<Appearance><Nude><Prefab Name=\"phw_00_nude_00_0001_damian_lnw\"/></Nude></Appearance>");
        const auto mixedManifest=folder/L"mixed-remover.tsv";
        std::ostringstream mixed;
        mixed<<"dmm-runtime\t1\n";
        for(const auto& [route,asset]:mixedRoutes)mixed<<"file\t"<<route<<'\t'<<asset.u8string()<<'\n';
        save(mixedManifest,mixed.str());
        paths.manifest=mixedManifest;
        const auto scoped=hex::inspectDependencies(game,paths);
        constexpr uint32_t sylvia=1u<<4;
        if(!scoped.bodyVerified||scoped.bodyMode!=hex::BodyMode::Vanilla||scoped.unavailableBodyWitches||
           scoped.missingUnderwearMeshMask!=sylvia||scoped.underwearSources[4].empty()||
           scoped.bodyNote!="Shape uses the mounted replacement body as the midpoint for Sylvia."||
           scoped.bodyWarning.find("No Underwear")==std::string::npos){
            std::cerr<<"The NPC underwear replacement must preserve shaping and keep ownership scoped to Sylvia in Vanilla mode. "
                <<"Mode "<<static_cast<int>(scoped.bodyMode)<<", shape mask "<<scoped.unavailableBodyWitches
                <<", missing mesh mask "<<scoped.missingUnderwearMeshMask<<", owner "<<scoped.underwearSources[4]
                <<", body detail "<<scoped.bodyDetail<<'\n';return 1;
        }
        if(!hex::verifyMountedDependencies(game,mixedManifest,error,warning)){
            std::cerr<<"The checked NPC body redirect was rejected in Vanilla mode: "<<error<<'\n';return 1;
        }
        const auto clonePrefab=remover/L"renamed.prefab",cloneMetadata=remover/L"renamed.prefabdata_xml";
        const auto cloneModel=remover/L"custom_body.pac";
        save(clonePrefab,std::string("\xff\xff\x04\0\0\0",6)+"SkinnedMeshComponent character/model/third_party/nude/custom_body.pac");
        save(cloneModel,"PAR mounted replacement fixture");
        save(cloneMetadata,"<SkeletonName FileName=\"1_pc/2_phw/PHW_01.pab\"/>"
            "<SkeletonVariationName FileName=\"1_PC/2_PHW/Nude/PHW_00_Nude_00_0001.pabc\"/>");
        save(sylviaAppearance,"<Appearance><Nude><Prefab Name=\"custom_underwear_body\"/></Nude></Appearance>");
        const auto cloneRoutes=mixed.str()+
            "file\tcharacter/bin__/prefab/third_party/nude/custom_underwear_body.prefab\t"+clonePrefab.u8string()+"\n"+
            "file\tcharacter/prefab/third_party/nude/custom_underwear_body.prefabdata_xml\t"+cloneMetadata.u8string()+"\n"+
            "file\tcharacter/model/third_party/nude/custom_body.pac\t"+cloneModel.u8string()+"\n";
        save(mixedManifest,cloneRoutes);
        const auto custom=hex::inspectDependencies(game,paths);
        if(!custom.bodyVerified||custom.bodyMode!=hex::BodyMode::Vanilla||custom.unavailableBodyWitches||
           custom.bodyNote!="Shape uses the mounted replacement body as the midpoint for Sylvia."||
           custom.bodyWarning.find("mounted witch body replacement")==std::string::npos){
            std::cerr<<"A compatible replacement with an unrelated name and folder blocked Vanilla shaping.\n";return 1;
        }
        save(mixedManifest,cloneRoutes+"file\tcharacter/bin__/prefab/another_mod/nude/custom_underwear_body.prefab\t"+clonePrefab.u8string()+"\n");
        const auto ambiguous=hex::inspectDependencies(game,paths);
        if(!ambiguous.bodyVerified||ambiguous.unavailableBodyWitches!=sylvia){
            std::cerr<<"An ambiguous replacement affected the wrong shaping targets.\n";return 1;
        }
        save(sylviaAppearance,"<Appearance><Nude><Prefab Name=\"hvw_00_nude_00_0001_damian\"/></Nude></Appearance>");
        save(selected,changed.str());
        save(folder/L"0.app_xml","<Appearance><Nude><Prefab Name=\"phw_00_nude_00_0001\"/></Nude></Appearance>");
        if(hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"Missing VENUS silently selected a stock body outside the explicit Vanilla option.\n";return 1;}
        save(folder/L"0.app_xml","<Appearance><Nude><Prefab Name=\"phw_00_nude_00_0001_damian\"/></Nude></Appearance>");
        uint32_t unavailable=0;
        if(hex::verifyMountedDependencies(game,selected,error,warning,&unavailable)){std::cerr<<"Both Hex Appeal body options were accepted at once.\n";return 1;}
        save(folder/L"0.app_xml","<Appearance><Nude><Prefab Name=\"hvw_00_nude_00_0001\"/></Nude></Appearance>");
        const auto overrideBody=folder/L"hvw_00_nude_00_0001.pac";save(overrideBody,"PAR unverified test fixture, never a model output");
        if(!hex::verifyMountedDependencies(game,selected,error,warning)||warning.empty()){std::cerr<<"Unverified readable vanilla override should remain usable with warning.\n";return 1;}
        save(overrideBody,"unreadable model fixture");
        if(hex::verifyMountedDependencies(game,selected,error,warning)){std::cerr<<"Invalid vanilla override accepted.\n";return 1;}
        std::cout<<"PASS: explicit original-witch resources, targeted NPC underwear redirect, mixed-option rejection and soft unverified override.\n";
    }
    std::ofstream badBody(temporary/L"overriding-body.pac",std::ios::binary);badBody<<"different installed body";badBody.close();
    auto altered=original.str();const std::string route="file\tcharacter/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac\t";
    const auto start=altered.find(route);if(start==std::string::npos)return 1;
    const auto path=start+route.size(),end=altered.find('\n',path);
    altered.replace(path,end-path,(temporary/L"overriding-body.pac").u8string());
    std::ofstream out(manifest,std::ios::binary);out<<altered;out.close();
    if(hex::verifyMountedDependencies(temporary,error)){std::cerr<<"Conflicting body was accepted.\n";return 1;}
    std::cout<<"PASS: actual mounted VENUS and seven witch routes verified; conflicting renamed body rejected despite the correct installed source.\n";
    return 0;
}
