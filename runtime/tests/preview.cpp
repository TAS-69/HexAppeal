#include "runtime.h"
#include "character_addons.h"
#include "wardrobe_slots.h"
#include "wardrobe_native.h"
#include "gui/menu.h"
#include "core/state.h"
#include "controls.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace ml::input {void MenuOpened(){}void MenuClosed(){}void InputModeChanged(){}SHORT AsyncKeyRaw(int){return 0;}void FeedInput(ImGuiIO&){}void SetOverlaySize(float,float){}}
namespace ml::hooks {void EnsureXInputHooks(){}}
static void frame(){
    hex::test::controls.clear();
    hex::test::hoveredHelp.clear();
    ml::gui::SetDisplayHeight(ImGui::GetIO().DisplaySize.y);
    ImGui::NewFrame();
    ml::gui::Render();
    ImGui::Render();
}
static void expectHover(const std::string& name,const std::string& reason){
    const auto rect=hex::test::controls.at(name);
    auto& io=ImGui::GetIO();
    io.AddMousePosEvent(io.DisplaySize.x-5,5);frame();
    io.AddMousePosEvent((rect.min.x+rect.max.x)*.5f,(rect.min.y+rect.max.y)*.5f);
    for(int i=0;i<24;++i)frame();
    if(hex::test::hoveredHelp.find(reason)==std::string::npos)
        throw std::runtime_error("Wrong hover detail for "+name+": "+hex::test::hoveredHelp);
}
static void click(const std::string& name,float fraction=.5f){
    auto& io=ImGui::GetIO();const auto rect=hex::test::controls.at(name);
    io.AddMousePosEvent(rect.min.x+(rect.max.x-rect.min.x)*fraction,(rect.min.y+rect.max.y)/2);frame();
    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
}
static void pad(float x,float y){
    auto& io=ImGui::GetIO();const auto rect=hex::test::controls.at("pad");
    io.AddMousePosEvent(rect.min.x+(rect.max.x-rect.min.x)*x,rect.min.y+(rect.max.y-rect.min.y)*y);frame();
    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
}
static void rightClick(const std::string& name){auto& io=ImGui::GetIO();const auto rect=hex::test::controls.at(name);io.AddMousePosEvent((rect.min.x+rect.max.x)/2,(rect.min.y+rect.max.y)/2);frame();io.AddMouseButtonEvent(1,true);frame();io.AddMouseButtonEvent(1,false);frame();}
static void output(const char* path){
    auto* file=fopen(path,"wb");if(!file)throw std::runtime_error("preview file");
    auto write=[&](const auto& x){fwrite(&x,sizeof(x),1,file);};
    auto& io=ImGui::GetIO();unsigned char* pixels=nullptr;int w=0,h=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    write(w);write(h);fwrite(pixels,4,size_t(w)*h,file);
    const auto* draw=ImGui::GetDrawData();write(draw->CmdListsCount);
    for(int l=0;l<draw->CmdListsCount;++l){const auto* d=draw->CmdLists[l];write(d->VtxBuffer.Size);fwrite(d->VtxBuffer.Data,sizeof(ImDrawVert),d->VtxBuffer.Size,file);write(d->IdxBuffer.Size);for(auto idx:d->IdxBuffer){uint32_t i=idx;write(i);}write(d->CmdBuffer.Size);for(const auto& c:d->CmdBuffer){write(c.ElemCount);write(c.IdxOffset);write(c.VtxOffset);write(c.ClipRect);}}
    fclose(file);
}
static void verifyScale(hex::Runtime& runtime,const std::string& previewPath){
    auto& io=ImGui::GetIO();
    auto& state=ml::State::Get();
    ml::State renderer;
    const uint64_t openedAt=10000;
    const auto grace=ml::nativeWardrobeDrawGraceMs;
    renderer.lastNativeWardrobeDrawAt=openedAt-1;
    if(renderer.HasRecentPresentation(openedAt)||renderer.NativeWardrobeDrawExpired(openedAt,openedAt+grace-1)||
       !renderer.NativeWardrobeDrawExpired(openedAt,openedAt+grace))
        throw std::runtime_error("native Wardrobe did not time out when no frame reached the editor");
    renderer.lastPresentAt=openedAt+grace;
    renderer.lastNativeWardrobeDrawAt=openedAt+grace-1;
    if(!renderer.HasRecentPresentation(openedAt+grace)||renderer.NativeWardrobeDrawExpired(openedAt,openedAt+grace)||
       !renderer.NativeWardrobeDrawExpired(openedAt,openedAt+2*grace))
        throw std::runtime_error("native Wardrobe did not distinguish game frames from editor draws");
    const auto* atlasPixels=io.Fonts->TexPixelsRGBA32;
    const auto texture=io.Fonts->TexID;
    const auto* font=io.FontDefault;
    const auto check=[&](float fontSize,float expectedWidth){
        const float width=state.panelRight.load()-state.panelLeft.load();
        if(io.FontDefault!=font||io.Fonts->TexPixelsRGBA32!=atlasPixels||io.Fonts->TexID!=texture)
            throw std::runtime_error("changing UI scale replaced the active font atlas");
        const float actualFontSize=font->FontSize*io.FontGlobalScale;
        if(std::fabs(actualFontSize-fontSize)>.5f||std::fabs(width-expectedWidth)>1.f)
            throw std::runtime_error("overlay size mismatch: text "+std::to_string(actualFontSize)+
                " px, panel "+std::to_string(width)+" px; expected "+std::to_string(fontSize)+
                " and "+std::to_string(expectedWidth)+" px");
    };
    const auto dragScale=[&](bool increase){
        const auto rect=hex::test::controls.at("scale");
        const float labelWidth=ImGui::CalcTextSize("Scale").x+ImGui::GetStyle().ItemInnerSpacing.x;
        const float y=(rect.min.y+rect.max.y)*.5f;
        const int oldPreference=runtime.uiScalePercent;
        io.AddMousePosEvent((rect.min.x+rect.max.x-labelWidth)*.5f,y);frame();
        io.AddMouseButtonEvent(0,true);frame();
        io.AddMousePosEvent(increase?rect.max.x-labelWidth-1.f:rect.min.x+1.f,y);frame();
        if(runtime.uiScalePercent!=oldPreference)throw std::runtime_error("UI scale applied while dragging the slider");
        io.AddMouseButtonEvent(0,false);frame();frame();
    };

    runtime.uiScalePercent=100;
    io.DisplaySize={3840,2160};
    frame();frame();click("tabInterface");
    check(32.f,1620.f);
    io.AddMousePosEvent(3800,10);frame();output((previewPath+".4k-default").c_str());
    for(int i=0;i<4;++i){
        dragScale(true);
        if(runtime.uiScalePercent!=130)throw std::runtime_error("4K scale slider did not reach 130%");
        check(41.6f,2106.f);
        for(int j=0;j<6;++j)frame();
        check(41.6f,2106.f);
        if(i==0){io.AddMousePosEvent(3800,10);frame();output((previewPath+".4k-max").c_str());}
        dragScale(false);
        if(runtime.uiScalePercent!=80)throw std::runtime_error("4K scale slider did not reach 80%");
        check(25.6f,1296.f);
    }
    dragScale(true);
    hex::Runtime restored;
    restored.loadInterface(runtime.profiles.parent_path()/"interface.ini");
    if(restored.uiScalePercent!=130)throw std::runtime_error("UI scale did not persist");
    io.DisplaySize={1920,1080};frame();check(20.8f,1053.f);
    io.DisplaySize={2560,1440};frame();check(27.73f,1404.f);
    io.DisplaySize={3840,2160};frame();check(41.6f,2106.f);

    click("close");
    if(!ml::gui::RequestSpouseWardrobe(0))throw std::runtime_error("4K native Wardrobe request failed");
    ml::gui::PollToggle();frame();frame();
    if(!hex::test::controls.count("equipmentLookup"))throw std::runtime_error("4K Wardrobe editor did not open with saved scale");
    check(41.6f,1456.f);
    click("closeSpouseWardrobe");
    if(ml::gui::WantsDraw())throw std::runtime_error("4K Wardrobe editor did not close");
}
int main(int argc,char** argv){try{
    const std::string_view mode=argc>=2?argv[1]:"";
    if(argc==3&&(mode=="--addon-follow-game"||mode=="--addon-follow-game-capacity")){
        std::string error;
        if(!hex::verifyAddonFollowGame(argv[2],error,mode=="--addon-follow-game-capacity"))throw std::runtime_error(error);
        std::puts("PASS: shared and separate Follow game outfit restoration.");
        return 0;
    }
    if(argc!=3&&argc!=4&&argc!=6)throw std::runtime_error("preview output, profile output, optional addon folder or width height page required");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize=argc==6?ImVec2(float(atoi(argv[3])),float(atoi(argv[4]))):ImVec2(1200,1000);io.DeltaTime=1.f/60.f;io.IniFilename=nullptr;
    auto& r=hex::Runtime::get();r.preview=true;r.shapingAvailable=true;r.hexAvailable=true;r.weightAvailable=true;r.profiles=argv[2];r.native.detail="Offline preview: no game or editor is loaded.";
    ml::State::Get().menuOpen=true;ml::gui::InitStyle(io.DisplaySize.y);unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);io.Fonts->SetTexID((ImTextureID)1);
    if(!io.FontDefault->FindGlyphNoFallback(0x2013)||!io.FontDefault->FindGlyphNoFallback(0x2019)){
        std::fputs("Missing punctuation in Wardrobe and item labels.\n",stderr);
        return 1;
    }
    for(int i=0;i<4;++i)frame();
    if(hex::test::controls.count("tabCharacters"))throw std::runtime_error("character patches must live under Settings");
    if(argc==6){const std::string page=argv[5];if(page=="wardrobe")click("tabWardrobe");else if(page=="interface")click("tabInterface");else if(page=="compact")click("compact");io.AddMousePosEvent(io.DisplaySize.x-10,10);frame();frame();output(argv[1]);ImGui::DestroyContext();return 0;}
    const float points[]={.5f,.001f,.999f};
    for(int t=0;t<3;++t)for(int b=0;b<3;++b){pad(points[b],points[t]);if(int(r.model.draft.top)!=t||int(r.model.draft.bottom)!=b)throw std::runtime_error("pad cannot reach combination "+std::to_string(t)+"/"+std::to_string(b)+"; got "+std::to_string(int(r.model.draft.top))+"/"+std::to_string(int(r.model.draft.bottom)));}
    pad(.72f,.29f);const auto priorTop=r.model.draft.topBlend;click("BOTTOM.preset1");
    if(std::fabs(r.model.draft.topBlend-priorTop)>.00001f||r.model.draft.bottom!=hex::Preset::Hex||r.model.draft.bottomBlend!=1)throw std::runtime_error("bottom selector changed the prior top blend");
    const auto beforeHeight=r.model.draft;click("height",.7f);if(r.model.draft.height<=0||r.model.draft.topBlend!=beforeHeight.topBlend||r.model.draft.bottom!=beforeHeight.bottom)throw std::runtime_error("height slider changed a regional selection");
    const auto beforeWeight=r.model.draft;click("weight",.9f);if(r.model.draft.weight<=0||r.model.draft.height!=beforeWeight.height||r.model.draft.topBlend!=beforeWeight.topBlend||r.model.draft.bottom!=beforeWeight.bottom)throw std::runtime_error("independent Weight unavailable without Dancer");
    if(r.model.applied)throw std::runtime_error("unavailable auto application falsely acknowledged");
    hex::Model read;std::string error;if(!read.load(r.profiles,error)||!(read.saved[0]==r.model.draft))throw std::runtime_error("automatic menu save failed");
    click("copy");const auto copied=r.model.draft;click("target0");click("paste");if(!(r.model.draft==copied))throw std::runtime_error("cross-witch paste failed");
    pad(.54f,.305f);const auto prior=r.model.draft;click("target3");click("target0");if(!(r.model.draft==prior))throw std::runtime_error("witch navigation discarded the latest pad edit");
    click("reset");if(int(r.model.draft.top)||int(r.model.draft.bottom)||r.model.draft.topBlend!=1||r.model.draft.bottomBlend!=1)throw std::runtime_error("baseline reset failed");
    hex::DependencyStatus unverified;unverified.scanned=true;unverified.bodyVerified=true;for(auto* file:{&unverified.game,&unverified.instant,&unverified.venus,&unverified.hex}){file->valid=true;file->detail=hex::unverifiedDependency("test dependency");}
    r.publishDependencies(unverified);frame();click("TOP.preset1");if(r.model.draft.top!=hex::Preset::Hex)throw std::runtime_error("unverified preset was hard-disabled");const auto softBefore=r.model.draft.height;click("height",.8f);if(r.model.draft.height==softBefore)throw std::runtime_error("unverified base blocked proportion editing");const auto softWeight=r.model.draft.weight;click("weight",.8f);if(r.model.draft.weight==softWeight)throw std::runtime_error("unverified optional weight was hard-disabled");io.AddMousePosEvent(io.DisplaySize.x-5,5);frame();frame();output((std::string(argv[1])+".unverified").c_str());
    r.reportCompatibility(hex::Feature::Shaping,hex::CompatibilityState::Blocked,"Native function interface changed.");frame();const auto incompatible=r.model.draft;click("height",.1f);click("TOP.preset2");pad(.9f,.9f);if(!(r.model.draft==incompatible))throw std::runtime_error("readable files bypassed an actual shaping block");
    expectHover("height","Native function interface changed.");
    r.reportCompatibility(hex::Feature::Shaping,hex::CompatibilityState::Ready,"Native checks passed.");r.reportCompatibility(hex::Feature::Hex,hex::CompatibilityState::Blocked,"Unknown preset field.");frame();click("TOP.preset2");const auto flat=r.model.draft;click("TOP.preset1");if(!(r.model.draft==flat))throw std::runtime_error("incompatible HEX preset remained selectable");const auto usableHeight=r.model.draft.height;click("height",.25f);if(r.model.draft.height==usableHeight)throw std::runtime_error("HEX-only block disabled independent height");
    expectHover("TOP.preset1","Unknown preset field.");
    r.reportCompatibility(hex::Feature::Hex,hex::CompatibilityState::Ready,"External preset parsed.");frame();click("TOP.preset1");if(r.model.draft.top!=hex::Preset::Hex)throw std::runtime_error("fixed preset did not recover its controls");
    r.reportCompatibility(hex::Feature::Weight,hex::CompatibilityState::Blocked,"Native Weight interface is unavailable.");frame();const auto blockedWeight=r.model.draft.weight;click("weight",.15f);if(r.model.draft.weight!=blockedWeight)throw std::runtime_error("weight capability block was ignored");r.reportCompatibility(hex::Feature::Weight,hex::CompatibilityState::Ready,"Native controls available.");
    r.publishDependencies(unverified);frame();const auto missingOutfitWeight=r.model.draft.weight;click("weight",.2f);if(r.model.draft.weight==missingOutfitWeight)throw std::runtime_error("missing Dancer source disabled Weight");
    auto vanilla=unverified;vanilla.bodyMode=hex::BodyMode::Vanilla;vanilla.venus={};r.publishDependencies(vanilla);frame();
    const auto vanillaHeight=r.model.draft.height;click("height",.6f);if(r.model.draft.height==vanillaHeight)throw std::runtime_error("explicit vanilla option was blocked by missing VENUS");
    click("TOP.preset0");click("BOTTOM.preset2");if(r.model.draft.top!=hex::Preset::Venus||r.model.draft.bottom!=hex::Preset::Flat)throw std::runtime_error("vanilla baseline/FLAT controls were not selectable");
    r.publishDependencies(unverified);frame();
    hex::Model noOutfitSaved;if(!noOutfitSaved.load(r.profiles,error)||noOutfitSaved.saved[0].weight!=r.model.draft.weight)throw std::runtime_error("Weight without Dancer did not save");
    r.publishDependencies({});click("reset");
    r.hexAvailable=false;frame();const auto noHex=r.model.draft;click("TOP.preset1");pad(.1f,.8f);
    if(!(r.model.draft==noHex))throw std::runtime_error("missing Hex allowed a HEX button or pad change");
    pad(.85f,.7f);if(r.model.draft.top!=hex::Preset::Flat||r.model.draft.bottom!=hex::Preset::Flat)throw std::runtime_error("missing Hex blocked independent VENUS/FLAT controls");
    r.shapingAvailable=false;frame();const auto noBase=r.model.draft;click("height",.95f);click("weight",.95f);click("TOP.preset0");pad(.95f,.95f);
    if(!(r.model.draft==noBase))throw std::runtime_error("missing base dependencies allowed shaping edits");
    click("tabInterface");if(!hex::test::controls.count("outfitRange"))throw std::runtime_error("outfit update range is missing from Settings");
    click("dependencyHeader");if(!hex::test::controls.count("pathManager"))throw std::runtime_error("dependency settings unavailable when shaping is disabled");
    click("dependencyHeader");
    auto withSkin=unverified;
    withSkin.skinTone={"character/texture/face.dds","fixture/face.dds","Custom Damiane face texture detected."};
    r.publishDependencies(withSkin);frame();click("skinHeader");
    if(!hex::test::controls.count("skinTone")||r.skinToneEnabled)throw std::runtime_error("VENUS skin matching must start off");
    click("skinTone");
    if(!r.skinToneEnabled)throw std::runtime_error("VENUS skin matching could not be enabled");
    r.publishDependencies(vanilla);frame();
    if(hex::test::controls.count("skinHeader")||hex::test::controls.count("skinTone"))
        throw std::runtime_error("Vanilla exposed skin matching with a previously saved VENUS setting");
    r.publishDependencies(withSkin);frame();click("skinTone");
    click("skinHeader");
    auto logged=unverified;logged.mountedMods["Example Outfit Mod"]=2;
    logged.relevantRoutes.push_back({"character/example.app_xml","Example Outfit Mod","fixture/example.app_xml"});
    logged.underwearSources[0]="Example Underwear Mod";r.publishDependencies(logged);
    click("tabLog");
    if(!hex::test::controls.count("saveLogReport"))throw std::runtime_error("Log tab cannot save a report");
    const auto report=r.diagnosticReport();
    if(report.find("Example Outfit Mod (2)")==std::string::npos||report.find("Example Underwear Mod")==std::string::npos||report.find("character/example.app_xml")==std::string::npos)throw std::runtime_error("Log report lost mounted names or routes");
    std::filesystem::path reportFile;
    if(!r.saveDiagnosticReport(reportFile,error)||!std::filesystem::is_regular_file(reportFile))throw std::runtime_error("Log report was not saved");
    r.publishDependencies({});
    r.shapingAvailable=true;r.hexAvailable=true;
    click("tabShape");
    click("tabWardrobe");
    const auto mask=r.underwearHiddenMask.load();
    click("underwear");
    if(r.underwearHiddenMask==mask)throw std::runtime_error("waiting for native underwear blocked saving the preference");
    click("underwear");
    click("target-1");
    r.reportCompatibility(hex::Feature::Wardrobe,hex::CompatibilityState::Waiting,"Waiting for the native appearance route.");
    frame();click("outfit");
    if(!hex::test::controls.count("outfitProfile"+std::to_string(int(hex::SlotOutfit::Game)))||
       !hex::test::controls.count("outfitProfile"+std::to_string(int(hex::SlotOutfit::Default))))
        throw std::runtime_error("the All characters selector hid Follow game or Vanilla while waiting");
    click("outfitProfile"+std::to_string(int(hex::SlotOutfit::Game)));
    if(hex::test::controls.count("equipmentLookup"))throw std::runtime_error("F9 Wardrobe exposed the spouse item picker");
    click("target-1");click("editWardrobe");frame();
    if(hex::test::controls.count("equipmentLookup"))throw std::runtime_error("the Wardrobe editor accepted All Witches");
    click("target3");click("editWardrobe");frame();
    if(r.model.target!=3||!hex::test::controls.count("equipmentLookup")||hex::test::controls.count("tabWardrobe"))throw std::runtime_error("F9 did not open the separate Wardrobe editor for Elowen");
    click("closeSpouseWardrobe");frame();
    if(!ml::gui::WantsDraw()||r.model.target!=3||!hex::test::controls.count("editWardrobe")||hex::test::controls.count("equipmentLookup"))throw std::runtime_error("closing the separate editor did not return to the same F9 wardrobe");
    auto removedMesh=vanilla;
    removedMesh.missingUnderwearMeshMask=1u<<4;
    removedMesh.underwearSources[4]="Example remover";
    r.publishDependencies(removedMesh);frame();click("target4");
    if(!hex::test::controls.count("underwearMeshMissing"))throw std::runtime_error("missing underwear explanation must appear while delegated");
    const bool externalBefore=r.allowExternalUnderwear.exchange(false);
    frame();const auto missingMeshPreference=r.underwearHiddenMask.load();click("underwear");
    if(!hex::test::controls.count("underwearMeshMissing")||r.underwearHiddenMask!=missingMeshPreference)
        throw std::runtime_error("taking back control enabled a missing base underwear mesh");
    expectHover("underwear","Example remover");
    click("target3");click("underwear");
    if(hex::test::controls.count("underwearMeshMissing")||r.underwearHiddenMask!=(missingMeshPreference^(1u<<3)))
        throw std::runtime_error("a separate witch lost her base underwear control");
    click("underwear");r.allowExternalUnderwear=externalBefore;
    r.publishDependencies({});frame();
    click("tabInterface");click("pinHeader");click("pin8");if(!(r.quickPins&hex::PinHeight))throw std::runtime_error("Height pin checkbox did not persist");
    click("compact");frame();if(!r.compact||!hex::test::controls.count("quickWitch")||!hex::test::controls.count("height")||hex::test::controls.count("pad"))throw std::runtime_error("quick panel did not restrict itself to pinned controls");
    rightClick("height");click("pinAction8");frame();if(r.quickPins&hex::PinHeight||hex::test::controls.count("height"))throw std::runtime_error("per-item context unpin failed");
    rightClick("underwear");click("pinAction32");frame();if(r.quickPins&hex::PinUnderwear||hex::test::controls.count("underwear"))throw std::runtime_error("unavailable controls cannot be unpinned individually");
    if(!r.blockGameInput)throw std::runtime_error("input blocking is not on by default");click("blockInput");if(r.blockGameInput||ml::State::Get().blockGameInput)throw std::runtime_error("header toggle did not enable pass-through");
    if(GetPrivateProfileIntW(L"Interface",L"BlockGameInput",1,(r.profiles.parent_path()/L"interface.ini").c_str()))throw std::runtime_error("input preference did not save");
    const auto priorTarget=r.model.target;
    if(ml::gui::RequestSpouseWardrobe(-1)||ml::gui::RequestSpouseWardrobe(6))throw std::runtime_error("spouse interface accepted an invalid witch");
    if(!ml::gui::RequestSpouseWardrobe(3))throw std::runtime_error("spouse interface rejected Elowen");
    ml::gui::PollToggle();frame();
    if(r.model.target!=3||!r.compact||!ml::gui::WantsDraw()||hex::test::controls.count("quickWitch")||hex::test::controls.count("tabShape")||hex::test::controls.count("outfit")||!hex::test::controls.count("equipmentLookup")||!hex::test::controls.count("applySpouseWardrobe"))throw std::runtime_error("spouse editor did not isolate equipment editing from F9 switching");
    if(!ml::gui::RequestSpouseWardrobe(4))throw std::runtime_error("spouse interface rejected Sylvia");
    ml::gui::PollToggle();frame();
    if(r.model.target!=4)throw std::runtime_error("spouse interface did not follow the new interaction");
    click("closeSpouseWardrobe");
    if(ml::gui::WantsDraw()||r.model.target!=priorTarget)throw std::runtime_error("closing spouse Wardrobe lost the earlier F9 selection");
    ml::State::Get().menuOpen=true;frame();
    click("compact");frame();click("tabShape");r.model.draft=copied;io.AddMousePosEvent(1100,10);frame();frame();output(argv[1]);
    if(argc==4){
        const int before=r.model.target;
        hex::initializeCharacterAddons(argv[3]);
        const int maegu=hex::addonTargetForRecord("NHW_Unique_Maegu_3452");
        const int woosa=hex::addonTargetForRecord("NHW_Unique_Woosa_3453");
        if(maegu<0||woosa<0)throw std::runtime_error("the independent NPC picker requires Woosa and Maegu");
        const auto packRow="target"+std::to_string(maegu);
        const auto duplicateRow="target"+std::to_string(woosa);
        const auto activeBefore=hex::enabledAddonTargets();
        hex::Profile maeguShape,woosaShape;
        maeguShape.height=.15f;
        woosaShape.height=-.1f;
        std::string issue;
        if(!hex::saveAddonBodyProfile(maegu,maeguShape,issue)||!hex::saveAddonBodyProfile(woosa,woosaShape,issue))
            throw std::runtime_error(issue);
        const hex::OutfitSlots maeguLook{{"Upper body",{"cd_phw_00_ub_00_0137"}}};
        const hex::OutfitSlots woosaLook{{"Upper body",{"cd_phw_00_ub_00_0158"}}};
        const auto seedLook=[&](int target,const hex::OutfitSlots& look){
            const auto dir=r.profiles.parent_path()/"CharacterWardrobe";
            std::filesystem::create_directories(dir);
            const auto file=dir/("npc_"+hex::characterAddon(target)->id+".txt");
            hex::WardrobeSlots saved;
            if(!saved.load(file,issue,true)||!saved.save(0,hex::SlotOutfit::Saved,look,issue,true))
                throw std::runtime_error(issue);
        };
        seedLook(maegu,maeguLook);
        seedLook(woosa,woosaLook);
        frame();frame();click("tabInterface");click("characterPatches");frame();
        if(!hex::test::controls.count("patch.maegu-woosa")||!hex::test::controls.count(packRow)||
           hex::test::controls.count(duplicateRow))
            throw std::runtime_error("Woosa and Maegu must share one sidebar row");
        click(packRow);
        click("tabShape");
        if(!hex::test::controls.count("pad")||!hex::test::controls.count("height")||!hex::test::controls.count("weight"))
            throw std::runtime_error("add-on shape controls are missing");
        if(!hex::test::controls.count("addonRecord")||hex::test::controls.count("shareVariants.ha_maegu"))
            throw std::runtime_error("the independent NPC picker must appear without a Combine instances checkbox");
        click("addonRecord");
        frame();
        click("packNpc"+std::to_string(woosa));
        frame();
        click(packRow);
        if(!hex::test::controls.count("selectedAddon"+std::to_string(woosa)))
            throw std::runtime_error("the pack sidebar row must keep the NPC chosen in its dropdown");
        click("height",.9f);
        if(!(hex::addonBodyProfile(maegu).profile==maeguShape)||hex::addonBodyProfile(woosa).profile==woosaShape)
            throw std::runtime_error("selecting Woosa must direct the Shape edit only to Woosa. Heights "+
                std::to_string(hex::addonBodyProfile(maegu).profile.height)+" / "+
                std::to_string(hex::addonBodyProfile(woosa).profile.height)+". "+r.model.message);
        const auto editedWoosa=hex::addonBodyProfile(woosa).profile;
        click("addonRecord");
        frame();
        click("packNpc"+std::to_string(maegu));
        frame();
        click("height",.25f);
        const auto maeguWardrobe=hex::addonWardrobeStatus(maegu);
        const auto woosaWardrobe=hex::addonWardrobeStatus(woosa);
        if(hex::enabledAddonTargets()!=activeBefore||
           !(hex::addonBodyProfile(woosa).profile==editedWoosa)||hex::addonBodyProfile(maegu).profile==maeguShape||
           maeguWardrobe.selected!=hex::SlotOutfit::Saved||maeguWardrobe.savedSlots!=maeguLook||
           woosaWardrobe.selected!=hex::SlotOutfit::Saved||woosaWardrobe.savedSlots!=woosaLook)
            throw std::runtime_error("switching NPCs interrupted the active roster or changed another NPC's saved Shape or outfit");
        click("tabWardrobe");
        for(const auto* control:{"outfit","editWardrobe","outfitSlots","outfitHidden","underwearOwner","underwear"})
            if(!hex::test::controls.count(control))throw std::runtime_error(std::string("shared Wardrobe missing ")+control);
        click("editWardrobe");
        frame();
        if(!hex::test::controls.count("equipmentLookup")||!hex::test::controls.count("applySpouseWardrobe"))
            throw std::runtime_error("add-on did not use the shared equipment editor");
        click("closeSpouseWardrobe");
        frame();
        click("tabInterface");
        click("patch.maegu-woosa");frame();
        if(hex::test::controls.count(packRow)||hex::test::controls.count(duplicateRow)||r.model.target!=-1)
            throw std::runtime_error("disabling the selected patch must remove its sidebar row and select all");
        click("patch.maegu-woosa");frame();
        if(!hex::test::controls.count(packRow)||hex::test::controls.count(duplicateRow))
            throw std::runtime_error("re-enabling the patch must restore a single sidebar row");
        click(packRow);
        if(!hex::test::controls.count("addonRecord")||hex::test::controls.count("shareVariants.ha_maegu"))
            throw std::runtime_error("the independent NPC picker should return when the pack is re-enabled");
        const auto naira=hex::addonTargetForRecord("Nairah");
        if(naira>=hex::firstAddonCharacter){
            const std::string combine="shareVariants.ha_naira";
            const auto checkInline=[&](const std::string& name){
                const auto header=hex::test::controls.at(name);
                const auto checkbox=hex::test::controls.at(combine);
                if(header.max.x>checkbox.min.x||header.min.y>=checkbox.max.y||header.max.y<=checkbox.min.y)
                    throw std::runtime_error("the instance checkbox is not to the right of the character header");
            };
            click("target"+std::to_string(naira));
            checkInline("characterName");
            click(combine);frame();
            if(hex::addonGroupShared(naira))throw std::runtime_error("the checkbox did not split Naira's settings");
            checkInline("addonRecord");
            click("addonRecord");
            click("instance"+std::to_string(hex::addonTargetForRecord("NHW_Unique_Nairah_DualSword_444")));
            click("compact");frame();
            checkInline("quickWitch");
            click(combine);frame();
            if(!hex::addonGroupShared(naira))throw std::runtime_error("the compact checkbox did not recombine Naira's settings");
            click("compact");frame();
        }
        click("target"+std::to_string(before));
        click("tabShape");
        if(!hex::test::controls.count("pad")||!hex::test::controls.count("characterName")||r.model.target!=before)
            throw std::runtime_error("returning from Characters changed the witch interface");
    }
    verifyScale(r,argv[1]);
    ImGui::DestroyContext();std::cout<<"PASS: shape controls, dependency gating, pins, input preferences, and repeated 4K scale changes with one font atlas.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
