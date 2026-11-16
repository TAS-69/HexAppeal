#include "runtime.h"
#include "underwear.h"
#include "wardrobe.h"
#include "wardrobe_native.h"
#include "dancer_palette.h"
#include "wardrobe_api.h"
#include "gui/menu.h"
#include "core/state.h"
#include "hooks/input.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#ifdef HEX_MENU_TESTS
#include "../tests/controls.h"
static void record(const std::string& name){hex::test::controls[name]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};}
#else
static void record(const std::string&){}
#endif

namespace ml::gui {
static float scale=1.f,pending=0.f;
static ImFont *bodyFont=nullptr,*titleFont=nullptr;
static constexpr ImVec4 accent{.91f,.38f,.66f,1},gold{.94f,.77f,.48f,1},muted{.60f,.55f,.65f,1},green{.48f,.84f,.69f,1};
static constexpr const char* defaultOutfitLabel="Default witch outfit";
static constexpr const char* dancerOutfitLabel="Sexy Witches of Pywel Reshaped / Dancer Dress";
static std::string baseVersionWarning,hexVersionWarning,weightVersionWarning,underwearVersionWarning;
static bool sourceWeightReady=false;
static void versionDecoration(const std::string& warning,bool tooltip=true,int placement=0){
    if(warning.empty())return;const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(lo,hi,IM_COL32(239,161,97,18),5*scale);draw->AddRect(lo,hi,IM_COL32(239,175,99,125),5*scale);
    const ImVec2 centre{hi.x+(placement==1?13:placement==3?-41:-13)*scale,lo.y+(placement==2?33:11)*scale};draw->AddCircleFilled(centre,7*scale,IM_COL32(229,166,91,235));draw->AddText({centre.x-2*scale,centre.y-7*scale},IM_COL32(36,22,28,255),"!");
    if(tooltip&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)){ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*25);ImGui::TextUnformatted(warning.c_str());ImGui::PopTextWrapPos();ImGui::EndTooltip();}
}
static void tip(const char* text){if(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))return;if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort|ImGuiHoveredFlags_AllowWhenDisabled)){ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*23);ImGui::TextUnformatted(text);ImGui::PopTextWrapPos();ImGui::EndTooltip();}}
void InitStyle(float value) {
    scale=std::clamp(value,.65f,1.8f);auto& s=ImGui::GetStyle();s=ImGuiStyle();ImGui::StyleColorsDark();
    s.WindowRounding=15;s.ChildRounding=10;s.FrameRounding=5;s.GrabRounding=5;s.PopupRounding=8;
    s.WindowPadding={18,16};s.ItemSpacing={9,8};s.FramePadding={10,7};s.WindowBorderSize=1;s.ChildBorderSize=1;
    s.Colors[ImGuiCol_WindowBg]={.045f,.025f,.059f,.98f};s.Colors[ImGuiCol_ChildBg]={.068f,.04f,.085f,1};
    s.Colors[ImGuiCol_Border]={.27f,.13f,.27f,1};s.Colors[ImGuiCol_Text]={.98f,.93f,.96f,1};s.Colors[ImGuiCol_TextDisabled]=muted;
    s.Colors[ImGuiCol_FrameBg]={.13f,.075f,.16f,1};s.Colors[ImGuiCol_FrameBgHovered]={.24f,.10f,.24f,1};s.Colors[ImGuiCol_FrameBgActive]={.33f,.12f,.28f,1};
    s.Colors[ImGuiCol_Button]={.17f,.075f,.20f,1};s.Colors[ImGuiCol_ButtonHovered]={.36f,.12f,.29f,1};s.Colors[ImGuiCol_ButtonActive]={.48f,.16f,.35f,1};
    s.Colors[ImGuiCol_Header]={.30f,.09f,.25f,1};s.Colors[ImGuiCol_HeaderHovered]={.38f,.12f,.29f,1};s.Colors[ImGuiCol_HeaderActive]={.45f,.15f,.34f,1};
    s.Colors[ImGuiCol_CheckMark]=gold;s.Colors[ImGuiCol_SliderGrab]=accent;s.Colors[ImGuiCol_SliderGrabActive]=gold;
    s.Colors[ImGuiCol_Separator]={.29f,.12f,.27f,1};s.Colors[ImGuiCol_Tab]={.12f,.06f,.15f,1};s.Colors[ImGuiCol_TabHovered]={.36f,.12f,.29f,1};s.Colors[ImGuiCol_TabSelected]={.35f,.105f,.28f,1};
    s.ScaleAllSizes(scale);auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.ConfigFlags&=~(ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad);
    io.Fonts->Clear();bodyFont=titleFont=nullptr;char windows[MAX_PATH]{};GetWindowsDirectoryA(windows,MAX_PATH);
    const auto fonts=std::string(windows)+"\\Fonts\\";
    if(std::filesystem::exists(fonts+"segoeui.ttf"))bodyFont=io.Fonts->AddFontFromFileTTF((fonts+"segoeui.ttf").c_str(),16*scale);
    if(!bodyFont){ImFontConfig config;config.SizePixels=16*scale;bodyFont=io.Fonts->AddFontDefault(&config);}
    if(std::filesystem::exists(fonts+"segoeuib.ttf"))titleFont=io.Fonts->AddFontFromFileTTF((fonts+"segoeuib.ttf").c_str(),27*scale);
    if(!titleFont)titleFont=bodyFont;io.FontDefault=bodyFont;
}
void SetDisplayHeight(float height){const auto n=std::clamp(height/1080.f,.75f,1.5f)*hex::Runtime::get().uiScalePercent/100.f;pending=std::fabs(n-scale)>.025f?n:0.f;}
bool FontsNeedRebuild(){return pending>0;}
void RebuildFonts(){const auto n=pending>0?pending:scale;pending=0;InitStyle(n);}
static void setOpen(bool open){auto& state=State::Get();if(state.menuOpen==open)return;state.menuOpen=open;if(open)input::MenuOpened();else {hex::Runtime::get().tickAuto(GetTickCount64(),true);input::MenuClosed();}}
void PollToggle(){
    auto& r=hex::Runtime::get();if(r.preview)return;r.tickAuto();
    static bool before=false;const auto down=[](int key){return (input::AsyncKeyRaw(key)&0x8000)!=0;};
    const bool held=State::ForegroundIsOurs()&&down(r.key)&&(down(VK_CONTROL)==r.control)&&(down(VK_MENU)==r.alt)&&(down(VK_SHIFT)==r.shift)&&!down(VK_LWIN)&&!down(VK_RWIN);
    if(held&&!before)setOpen(!State::Get().menuOpen);before=held;
}
bool WantsDraw(){return State::Get().menuOpen;}
void FeedInput(){if(!hex::Runtime::get().preview)input::FeedInput(ImGui::GetIO());}
static const char* name(hex::Preset p){return p==hex::Preset::Hex?"HEX":p==hex::Preset::Flat?"FLAT":"VENUS";}
static bool choice(const char* text,bool selected,ImVec2 size){if(selected){ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.47f,.14f,.34f,1));ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,.88f,.64f,1));}const bool clicked=ImGui::Button(text,size);if(selected)ImGui::PopStyleColor(2);return clicked;}
static void pinContext(uint32_t bit){
    auto& r=hex::Runtime::get();const auto id=std::string("pin-menu-")+std::to_string(bit);
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)&&ImGui::IsMouseReleased(1))ImGui::OpenPopup(id.c_str());
    if(ImGui::BeginPopup(id.c_str())){const bool pinned=(r.quickPins&bit)!=0;if(ImGui::MenuItem(pinned?"Unpin from quick panel":"Pin to quick panel")){r.quickPins^=bit;r.saveInterface();}record("pinAction"+std::to_string(bit));ImGui::EndPopup();}
}
static bool region(const char* label,hex::Preset& preset,float& blend,bool available,bool hexAvailable){
    bool changed=false;ImGui::PushID(label);ImGui::BeginGroup();ImGui::AlignTextToFramePadding();ImGui::TextColored(gold,"%s",label);ImGui::SameLine(70*scale);
    const float width=std::max(35.f,(ImGui::GetContentRegionAvail().x-2*ImGui::GetStyle().ItemSpacing.x)/3);
    for(int i=0;i<3;++i){if(i)ImGui::SameLine();const bool disabled=!available||(i==1&&!hexAvailable);ImGui::BeginDisabled(disabled);if(choice(i==0?"VENUS":i==1?"HEX":"FLAT",int(preset)==i,{width,28*scale})){preset=hex::Preset(i);blend=1.f;changed=true;}record(std::string(label)+".preset"+std::to_string(i));ImGui::EndDisabled();const auto warning=baseVersionWarning+(i==1?hexVersionWarning:"");versionDecoration(warning);if(disabled&&warning.empty())tip(!available?"Install and enable the required body/editor files, then check Dependencies in Settings.":"Locate Hex's separately installed preset in Settings to enable HEX.");}
    ImGui::EndGroup();pinContext(label[0]=='T'?hex::PinTop:hex::PinBottom);ImGui::PopID();return changed;
}
static bool shapePad(hex::Profile& profile,float maximum,bool available,bool hexAvailable){
    const float side=std::floor(std::max(150.f,std::min(ImGui::GetContentRegionAvail().x,std::min(480*scale,maximum))));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(ImGui::GetContentRegionAvail().x-side)*.5f));
    const auto unaligned=ImGui::GetCursorScreenPos();ImGui::SetCursorScreenPos({std::floor(unaligned.x),std::floor(unaligned.y)});
    const auto start=ImGui::GetCursorScreenPos();const ImVec2 end{start.x+side,start.y+side};auto* draw=ImGui::GetWindowDrawList();
    ImGui::BeginDisabled(!available);ImGui::InvisibleButton("##shapePad",{side,side});record("pad");ImGui::EndDisabled();bool changed=false;static int detent=-1;
    if(!ImGui::IsItemActive()||ImGui::IsItemActivated())detent=-1;
    if(available&&ImGui::IsItemActive()&&ImGui::IsMouseDown(0)){
        float x=std::clamp((ImGui::GetIO().MousePos.x-start.x)/side*2-1,-1.f,1.f),y=std::clamp((ImGui::GetIO().MousePos.y-start.y)/side*2-1,-1.f,1.f);
        hex::snapShapePad(x,y,side,detent);
        if(hexAvailable||(x>=0&&y>=0)){const auto before=profile;hex::moveShapePad(profile,x,y);changed=!(before==profile);}
    }
    draw->AddRectFilledMultiColor(start,end,IM_COL32(98,25,75,255),IM_COL32(58,28,78,255),IM_COL32(25,27,47,255),IM_COL32(50,20,58,255));
    draw->AddRect(start,end,IM_COL32(168,89,135,255),8*scale,0,1.5f*scale);
    for(int i=1;i<4;++i){const float n=side*i/4;draw->AddLine({start.x+n,start.y},{start.x+n,end.y},IM_COL32(199,149,176,i==2?64:24));draw->AddLine({start.x,start.y+n},{end.x,start.y+n},IM_COL32(199,149,176,i==2?64:24));}
    const ImVec2 centre{start.x+side*.5f,start.y+side*.5f};
    draw->AddCircle(centre,side*.32f,IM_COL32(215,155,187,23),96);draw->AddCircle(centre,side*.16f,IM_COL32(215,155,187,27),64);
    for(int y=0;y<3;++y)for(int x=0;x<3;++x)draw->AddCircleFilled({start.x+x*side*.5f,start.y+y*side*.5f},3*scale,IM_COL32(225,183,160,180));
    const auto label=[&](const char* text,float x,float y,bool right=false){const auto sz=ImGui::CalcTextSize(text);draw->AddText({start.x+x-(right?sz.x:0),start.y+y},IM_COL32(243,207,174,245),text);};
    label("HEX / HEX",10*scale,8*scale);label("HEX / FLAT",side-10*scale,8*scale,true);
    label("FLAT / HEX",10*scale,side-26*scale);label("FLAT / FLAT",side-10*scale,side-26*scale,true);
    if(!available)draw->AddRectFilled(start,end,IM_COL32(10,10,14,160));
    else if(!hexAvailable){draw->AddRectFilled(start,{centre.x,end.y},IM_COL32(10,10,14,170));draw->AddRectFilled({centre.x,start.y},{end.x,centre.y},IM_COL32(10,10,14,170));}
    const auto sz=ImGui::CalcTextSize("VENUS");draw->AddText({centre.x-sz.x*.5f,centre.y+12*scale},IM_COL32(226,184,147,205),"VENUS");
    const ImVec2 marker{start.x+(hex::regionCoordinate(profile.bottom,profile.bottomBlend)+1)*.5f*side,start.y+(hex::regionCoordinate(profile.top,profile.topBlend)+1)*.5f*side};
    draw->AddLine(centre,marker,IM_COL32(244,186,136,100),scale);
    draw->AddCircleFilled(marker,16*scale,IM_COL32(235,83,157,40));draw->AddCircleFilled(marker,10*scale,IM_COL32(22,8,26,255));draw->AddCircle(marker,10*scale,IM_COL32(255,208,137,255),40,2*scale);draw->AddCircleFilled(marker,4*scale,IM_COL32(255,227,183,255));
    const bool hovered=ImGui::IsItemHovered()||ImGui::IsItemActive();pinContext(hex::PinPad);
    versionDecoration(baseVersionWarning+hexVersionWarning,false,2);
    if(hovered&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)){
        ImGui::BeginTooltip();ImGui::Text("TOP  %s %.0f%%",name(profile.top),profile.top==hex::Preset::Venus?100:profile.topBlend*100);ImGui::Text("BOTTOM  %s %.0f%%",name(profile.bottom),profile.bottom==hex::Preset::Venus?100:profile.bottomBlend*100);if(!hexAvailable)ImGui::TextUnformatted("HEX disabled: locate its preset in Settings.");
        if(!(baseVersionWarning+hexVersionWarning).empty()){ImGui::PushTextWrapPos(ImGui::GetFontSize()*25);ImGui::TextUnformatted((baseVersionWarning+hexVersionWarning).c_str());ImGui::PopTextWrapPos();}ImGui::EndTooltip();
    }
    return changed;
}
static void dependencySettings(hex::Runtime& r){
    const bool expanded=ImGui::CollapsingHeader("Dependencies & paths");record("dependencyHeader");if(!expanded)return;
    const auto status=r.dependencyStatus();
    const auto state=[&](const char* label,bool available,const std::string& detail){ImGui::TextColored(available?green:muted,"%s  /  %s",label,available?"Ready":"Unavailable");tip(detail.c_str());};
    const auto fileState=[&](const char* label,const hex::DependencyFile& file){ImGui::TextColored(!file.valid?muted:file.verified?green:gold,"%s / %s%s",label,!file.valid?"File needed":file.verified?"Verified":"Unverified",file.valid&&!file.verified?"  (!)":"");tip(file.detail.c_str());};
    fileState("Crimson Desert",status.game);fileState("Instant Surgery",status.instant);fileState("VENUS Elf Body",status.venus);fileState("Hex's Hentai Preset",status.hex);fileState("Sexy Witches reference",status.weight);if(status.physics.valid)fileState("Body Physics",status.physics);if(status.creator.valid)fileState("Character Creator",status.creator);
    if(!status.bodyWarning.empty()){ImGui::TextColored(gold,"Active body routes / unverified  (!)");tip(status.bodyWarning.c_str());}
    static std::array<std::array<char,32768>,6> buffers{};static std::array<std::string,6> synced{};static std::array<bool,6> editing{};
    auto paths=r.configuredDependencyPaths();bool changed=false;
    const auto field=[&](int i,const char* label,const char* control,std::filesystem::path& path){
        const auto value=path.u8string();if(!editing[i]&&synced[i]!=value){snprintf(buffers[i].data(),buffers[i].size(),"%s",value.c_str());synced[i]=value;}
        const std::string inputId=std::string("##")+control;
        ImGui::SetNextItemWidth(-1);ImGui::TextDisabled("%s",label);ImGui::InputTextWithHint(inputId.c_str(),"Auto-detect",buffers[i].data(),buffers[i].size());record(control);editing[i]=ImGui::IsItemActive();
        if(ImGui::IsItemDeactivatedAfterEdit()){path=std::filesystem::u8path(buffers[i].data());synced[i]=path.u8string();changed=true;}
    };
    field(0,"Mod manager config","pathManager",paths.managerConfig);field(1,"Mods folder","pathMods",paths.modsDirectory);field(2,"Active file list (DMM manifest.tsv)","pathManifest",paths.manifest);
    field(3,"Hex preset file","pathHex",paths.hexPreset);field(4,"VENUS body file (optional source override)","pathVenus",paths.venusBody);field(5,"Weight reference file or mod folder","pathWeight",paths.weightReference);
    if(changed){std::string error;if(!r.setDependencyPaths(paths,error))r.model.message=error;}
    if(ImGui::Button("Detect again"))++r.dependencyRevision;record("dependencyRescan");tip("Paths are saved automatically. Empty fields use detection. Active body verification currently requires DMM's loaded runtime and file list; another manager's source folder alone cannot prove which body the game uses. Installing or changing runtime plugins requires a restart.");
}
static void pinSettings(hex::Runtime& r){
    const bool open=ImGui::CollapsingHeader("Quick panel pins");record("pinHeader");if(!open)return;
    struct Item{const char* label;uint32_t bit;};
    const Item items[]={{"Top selector",hex::PinTop},{"Bottom selector",hex::PinBottom},{"Shape pad",hex::PinPad},{"Height",hex::PinHeight},{"Weight",hex::PinWeight},{"Base underwear",hex::PinUnderwear},{"Outfit switch",hex::PinOutfit},{"Copy",hex::PinCopy},{"Paste",hex::PinPaste},{"Reset",hex::PinReset},{"Dancer colour (global)",hex::PinDancerColour}};
    for(const auto& item:items){bool selected=(r.quickPins&item.bit)!=0;if(ImGui::Checkbox(item.label,&selected)){r.quickPins^=item.bit;r.saveInterface();}record("pin"+std::to_string(item.bit));}
    ImGui::TextDisabled("Or right-click a control to pin it.");
}
static void underwearControl(hex::Runtime& r,bool available){
    const auto status=hex::underwearStatus(r.model.target);const auto dependency=r.dependencyStatus();const bool filesReady=dependency.scanned&&dependency.game.valid&&dependency.instant.valid&&dependency.venus.valid&&dependency.bodyVerified;bool hidden=status.hidden;ImGui::BeginDisabled(!available||(!status.ready&&!filesReady));
    if(ImGui::Checkbox("Hide base bra & panties",&hidden)){std::string error;if(!hex::setUnderwearHidden(r.model.target,hidden,error))r.model.message=error;}
    record("underwear");ImGui::EndDisabled();pinContext(hex::PinUnderwear);versionDecoration(underwearVersionWarning,true,1);if(underwearVersionWarning.empty())tip("Hides the game's base underwear and compatible Wardrobe bra/panties. Built-in outfit pieces need provider support.");
    if(status.mixed){ImGui::SameLine();ImGui::TextColored(gold,"Mixed");}
}
static void outfitControl(){
    auto& runtime=hex::Runtime::get();const auto status=hex::wardrobeEditorStatus(runtime.model.target);
    const auto selected=hex::wardrobeOutfitId(status.selected);const auto providers=hex::wardrobeProviderOutfits();
    std::string label=selected=="hexappeal/default"?defaultOutfitLabel:selected=="hexappeal/dancer"?dancerOutfitLabel:"Provider unavailable";
    for(const auto& provider:providers)if(provider.id==selected)label=provider.label;
    const auto dependencies=runtime.dependencyStatus();const bool uiReady=status.available.switchReady||(dependencies.scanned&&dependencies.game.valid);
    ImGui::BeginDisabled(!uiReady);ImGui::SetNextItemWidth(-1);
    const bool opened=ImGui::BeginCombo("##outfit",status.mixedOutfit?"Mixed":label.c_str());record("outfit");
    if(opened){
        for(int choice=0;choice<2;++choice){
            const bool missing=choice==1&&!status.available.dancerAssets&&!status.resourceFilesReady;ImGui::BeginDisabled(missing);
            const char* id=choice?"hexappeal/dancer":"hexappeal/default";
            if(ImGui::Selectable(choice?dancerOutfitLabel:defaultOutfitLabel,selected==id)){std::string error;if(!hex::requestWardrobeOutfit(runtime.model.target,id,error))runtime.model.message=error;}
            ImGui::EndDisabled();tip(missing?status.dependencyDetail.c_str():choice?"Sexy Witches of Pywel Reshaped - Color Variants, by Vincent1681. Dancer Dress; supplied colours appear below.":"Uses the currently mounted default witch outfit, including Hex Appeal's separate refit add-on when enabled.");
        }
        for(const auto& provider:providers){if(provider.id=="hexappeal/dancer"||(provider.removed&&provider.id!=selected))continue;ImGui::PushID(provider.id.c_str());ImGui::BeginDisabled(!provider.available);
            if(ImGui::Selectable(provider.label.c_str(),selected==provider.id)){std::string error;if(!hex::requestWardrobeOutfit(runtime.model.target,provider.id,error))runtime.model.message=error;}
            ImGui::EndDisabled();tip((provider.available?provider.requirements:provider.reason).c_str());ImGui::PopID();}
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();if(!opened){pinContext(hex::PinOutfit);const auto warning=dependencies.game.valid&&!dependencies.game.verified?dependencies.game.detail:std::string{};versionDecoration(warning+((selected=="hexappeal/dancer")?status.versionWarning:""),true,3);if(warning.empty()&&status.versionWarning.empty())tip(status.detail.c_str());}
    if(status.mixedOutfit)ImGui::TextColored(gold,"Mixed outfits");
    ImGui::TextColored(status.warning?gold:muted,"%s",status.caption.c_str());tip(status.detail.c_str());
}
static void providerWardrobeControls(hex::Runtime& runtime,bool pinnedOnly=false){
    const auto state=hex::wardrobeEditorStatus(runtime.model.target);if(state.mixedOutfit){if(!pinnedOnly)ImGui::TextDisabled("Choose one outfit to edit its options.");return;}
    const auto selected=hex::wardrobeOutfitId(state.selected);const auto providers=hex::wardrobeProviderOutfits();
    for(const auto& outfit:providers)if(outfit.id==selected){
        for(const auto& control:outfit.controls){
            const bool pinned=hex::wardrobeControlPinned(outfit.id,control.id);if(pinnedOnly&&!pinned)continue;
            uint32_t value=control.defaultValue;uint64_t revision=0;const int target=control.scope==HA_GLOBAL?-1:runtime.model.target<0?0:runtime.model.target;
            if(!hex::readWardrobeControl(target,outfit.id,control.id,value,revision))continue;
            bool mixed=false;if(control.scope==HA_PER_WITCH&&runtime.model.target<0)for(int i=1;i<6;++i){uint32_t other=0;uint64_t stamp=0;if(hex::readWardrobeControl(i,outfit.id,control.id,other,stamp)&&other!=value)mixed=true;}
            ImGui::PushID(control.id.c_str());ImGui::BeginDisabled(!outfit.available);bool changed=false;
            if(control.type==HA_TOGGLE){bool enabled=value!=0;if(ImGui::Checkbox(control.label.c_str(),&enabled)){value=enabled;changed=true;}}
            else{ImGui::TextDisabled("%s%s",control.label.c_str(),control.scope==HA_GLOBAL?" (global)":"");ImGui::SetNextItemWidth(-1);
                if(ImGui::BeginCombo("##variant",mixed?"Mixed":control.options[value].label.c_str())){for(uint32_t i=0;i<control.options.size();++i)if(ImGui::Selectable(control.options[i].label.c_str(),value==i)){value=i;changed=true;}ImGui::EndCombo();}}
            record("provider."+outfit.id+"."+control.id);ImGui::EndDisabled();
            const auto pinId="provider-pin-"+outfit.id+'/'+control.id;
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)&&ImGui::IsMouseReleased(1))ImGui::OpenPopup(pinId.c_str());
            if(ImGui::BeginPopup(pinId.c_str())){if(ImGui::MenuItem(pinned?"Unpin from quick panel":"Pin to quick panel")){std::string error;if(!hex::pinWardrobeControl(outfit.id,control.id,!pinned,error))runtime.model.message=error;}ImGui::EndPopup();}
            const auto help=(!outfit.available?outfit.reason+"\n":"")+control.help+((control.flags&HA_FOLLOW_BASE_UNDERWEAR)?"\nFollows Base underwear. This preference returns when Base underwear is shown.":"");tip(help.c_str());
            if(control.type==HA_TOGGLE&&control.scope==HA_GLOBAL){ImGui::SameLine();ImGui::TextDisabled("Global");}
            if(mixed&&control.type==HA_TOGGLE){ImGui::SameLine();ImGui::TextColored(gold,"Mixed");}
            if(changed){std::string error;if(!hex::writeWardrobeControl(control.scope==HA_GLOBAL?-1:runtime.model.target,outfit.id,control.id,value,error))runtime.model.message=error;}
            ImGui::PopID();
        }return;
    }
}
static void wardrobeColour(hex::Runtime& runtime,bool dancer){
    if(dancer){
        const auto status=hex::globalDancerColourStatus();
        ImGui::TextDisabled("Dancer colour (global)");ImGui::SetNextItemWidth(-1);ImGui::BeginDisabled(!status.ready);
        if(ImGui::BeginCombo("##globalDressColour",status.selected<0?"Unavailable":hex::dancerColours[status.selected].label)){
            for(int i=0;i<int(hex::dancerColours.size());++i)if(ImGui::Selectable(hex::dancerColours[i].label,status.selected==i)){
                std::string error;if(!hex::changeGlobalDancerColour(i,error))runtime.model.message=error;
            }ImGui::EndCombo();
        }
        record("dancerColour");ImGui::EndDisabled();pinContext(hex::PinDancerColour);versionDecoration(status.versionWarning,true,3);
        if(status.versionWarning.empty())tip("Uses the separate Sexy Witches colour textures globally. Saves first, then refreshes visible owned Dancer outfits through Default and back. A restart is a fallback if another mod retains a cached texture.");
        ImGui::TextColored(status.selected!=status.startup?gold:muted,"%s",status.detail.c_str());return;
    }
    const auto status=hex::wardrobeEditorStatus(runtime.model.target);const auto pin=dancer?hex::PinDancerColour:hex::PinUnderwearColour;
    const char* choices[]={"Original","White","Black","Crimson","Pink","Emerald"};
    int selected=int(dancer?status.selected.dancer:status.selected.underwear);
    ImGui::TextDisabled(dancer?"Dress colour":"Underwear colour");ImGui::SetNextItemWidth(-1);
    ImGui::BeginDisabled(!(dancer?status.available.dancerColourReady:status.available.underwearColourReady));
    if(ImGui::Combo(dancer?"##dressColour":"##underwearColour",&selected,choices,6)){std::string error;if(!hex::changeWardrobeChoice(runtime.model.target,dancer?hex::WardrobeField::DancerColour:hex::WardrobeField::UnderwearColour,selected,error))runtime.model.message=error;}
    record(dancer?"dancerColour":"underwearColour");ImGui::EndDisabled();pinContext(pin);tip("Per-witch material ownership is not verified yet. Colour changes remain disabled.");
}
static bool proportions(hex::Runtime& r,bool available,bool height,bool weight){
    bool changed=false;
    if(height){ImGui::BeginDisabled(!available);float value=r.model.draft.height*10;ImGui::SetNextItemWidth(std::max(100.f,ImGui::GetContentRegionAvail().x-70*scale));if(ImGui::SliderFloat("Height",&value,-10,10,"%+.1f%%",ImGuiSliderFlags_AlwaysClamp)){r.model.draft.height=value/10;changed=true;}record("height");ImGui::EndDisabled();pinContext(hex::PinHeight);versionDecoration(baseVersionWarning,true,1);if(baseVersionWarning.empty())tip("Scales the complete rig and fitted clothing. Independent of Weight; Ctrl-click for an exact value.");}
    if(weight){ImGui::BeginDisabled(!available||!sourceWeightReady);float value=r.model.draft.weight*100;ImGui::SetNextItemWidth(std::max(100.f,ImGui::GetContentRegionAvail().x-70*scale));if(ImGui::SliderFloat("Weight",&value,-100,100,"%+.0f%%",ImGuiSliderFlags_AlwaysClamp)){r.model.draft.weight=value/100;changed=true;}record("weight");ImGui::EndDisabled();pinContext(hex::PinWeight);versionDecoration(baseVersionWarning+weightVersionWarning,true,1);if(baseVersionWarning.empty()&&weightVersionWarning.empty())tip(sourceWeightReady?"Limbs and waist, independent of Height and breast/butt presets.":"Locate the optional Sexy Witches reference in Settings. Height remains available.");}
    return changed;
}
static bool profileActions(hex::Runtime& r,uint32_t mask){
    bool changed=false;const auto item=[&](const char* label,uint32_t bit){if(!(mask&bit))return;
        if(bit==hex::PinPaste)ImGui::BeginDisabled(!r.hasCopied);
        if(ImGui::SmallButton(label)){if(bit==hex::PinCopy){r.copied=r.model.draft;r.hasCopied=true;}else if(bit==hex::PinPaste){r.model.draft=r.copied;changed=true;}else{r.model.draft=hex::Profile{};changed=true;}}
        record(bit==hex::PinCopy?"copy":bit==hex::PinPaste?"paste":"reset");if(bit==hex::PinPaste)ImGui::EndDisabled();pinContext(bit);
    };
    item("Copy",hex::PinCopy);if(mask&hex::PinCopy&&mask&(hex::PinPaste|hex::PinReset))ImGui::SameLine();item("Paste",hex::PinPaste);if(mask&hex::PinPaste&&mask&hex::PinReset)ImGui::SameLine();item("Reset",hex::PinReset);return changed;
}
void Render(){
    auto& r=hex::Runtime::get();r.tickAuto();auto& model=r.model;auto& state=State::Get();state.lastDrawAt=GetTickCount();state.blockGameInput=r.blockGameInput;
    const auto dependencies=r.dependencyStatus();baseVersionWarning.clear();hexVersionWarning.clear();weightVersionWarning.clear();underwearVersionWarning.clear();
    const auto appendWarning=[](std::string& out,const hex::DependencyFile& file){if(file.valid&&!file.verified){if(!out.empty())out+="\n\n";out+=file.detail;}};
    for(const auto* file:{&dependencies.game,&dependencies.instant,&dependencies.venus,&dependencies.physics})appendWarning(baseVersionWarning,*file);
    if(!dependencies.bodyWarning.empty()){if(!baseVersionWarning.empty())baseVersionWarning+="\n\n";baseVersionWarning+=dependencies.bodyWarning;}
    appendWarning(hexVersionWarning,dependencies.hex);appendWarning(weightVersionWarning,dependencies.weight);underwearVersionWarning=baseVersionWarning;appendWarning(underwearVersionWarning,dependencies.creator);
    const bool shapingReady=dependencies.scanned?dependencies.game.valid&&dependencies.instant.valid&&dependencies.venus.valid&&dependencies.bodyVerified:r.shapingAvailable.load(),hexReady=dependencies.scanned?dependencies.hex.valid:r.hexAvailable.load();sourceWeightReady=dependencies.scanned?dependencies.weight.valid:r.weightAvailable.load();
    auto& io=ImGui::GetIO();io.MouseDrawCursor=true;const float maxW=std::max(300.f,io.DisplaySize.x-24),maxH=std::max(200.f,io.DisplaySize.y-24);
    static bool first=true,previous=false;static uint32_t previousPins=0;static size_t previousProviderPins=0;
    float quickHeight=190;for(uint32_t bit=1;bit<=hex::PinDancerColour;bit<<=1)if(r.quickPins&bit)quickHeight+=bit==hex::PinPad?260:bit==hex::PinTop||bit==hex::PinBottom?40:bit==hex::PinOutfit||bit==hex::PinUnderwearColour||bit==hex::PinDancerColour?65:35;
    size_t providerPins=0;const auto outfitId=hex::wardrobeOutfitId(hex::wardrobeEditorStatus(r.model.target).selected);for(const auto& provider:hex::wardrobeProviderOutfits())if(provider.id==outfitId)for(const auto& control:provider.controls)if(hex::wardrobeControlPinned(provider.id,control.id))++providerPins;quickHeight+=providerPins*65;
    if(first||previous!=r.compact||(r.compact&&(previousPins!=r.quickPins||previousProviderPins!=providerPins))){ImGui::SetNextWindowSize({std::min((r.compact?520.f:810.f)*scale,maxW),std::min((r.compact?quickHeight:780.f)*scale,maxH)},ImGuiCond_Always);first=false;previous=r.compact;previousPins=r.quickPins;previousProviderPins=providerPins;}
    ImGui::SetNextWindowSizeConstraints({std::min(400*scale,maxW),std::min((r.compact?220.f:330.f)*scale,maxH)},{maxW,maxH});ImGui::SetNextWindowPos({24*scale,24*scale},ImGuiCond_FirstUseEver);
    bool open=true;ImGui::Begin("Hex Appeal",&open,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar);
    const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();state.panelLeft=pos.x;state.panelTop=pos.y;state.panelRight=pos.x+size.x;state.panelBottom=pos.y+size.y;
    auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled({pos.x+16*scale,pos.y+10*scale},{pos.x+size.x-16*scale,pos.y+13*scale},ImGui::ColorConvertFloat4ToU32(accent),2*scale);
    ImGui::PushFont(titleFont);ImGui::TextUnformatted("HEX APPEAL");ImGui::PopFont();
    const float titleWidth=ImGui::GetItemRectSize().x,toolbarWidth=ImGui::CalcTextSize("Block game input").x+ImGui::GetFrameHeight()+ImGui::CalcTextSize("Minimise").x+70*scale;
    if(size.x>titleWidth+toolbarWidth+55*scale)ImGui::SameLine(size.x-toolbarWidth-18*scale);
    if(ImGui::Checkbox("Block game input",&r.blockGameInput)){state.blockGameInput=r.blockGameInput;input::InputModeChanged();r.saveInterface();}record("blockInput");tip("On: block gameplay input while this panel is visible. Off: allow gameplay outside panel gestures and text entry. Saved automatically.");
    ImGui::SameLine();if(ImGui::SmallButton(r.compact?"Restore":"Minimise")){r.compact=!r.compact;r.saveInterface();}record("compact");ImGui::SameLine();if(ImGui::SmallButton("X"))open=false;record("close");
    ImGui::Separator();bool changed=false;
    ImGui::BeginChild("workspace",{0,-37*scale},ImGuiChildFlags_None);
    if(!r.compact){
        ImGui::BeginChild("witches",{125*scale,0},ImGuiChildFlags_None);
        for(int target=-1;target<6;++target){ImGui::PushID(target);if(ImGui::Selectable(target<0?"ALL WITCHES":hex::witches[target].name,model.target==target,0,{0,33*scale}))model.selectTarget(target);record("target"+std::to_string(target));ImGui::PopID();}ImGui::EndChild();ImGui::SameLine();
    }
    ImGui::BeginChild("editor",{0,0},ImGuiChildFlags_None);
    if(r.compact){const char* targets[]={"All witches","Lyselia","Bari","Areciel","Elowen","Sylvia","White Crow"};int selected=model.target+1;ImGui::SetNextItemWidth(-1);if(ImGui::Combo("##witch",&selected,targets,7))model.selectTarget(selected-1);record("quickWitch");
        bool mixed=false;if(model.target<0)for(const auto& profile:model.saved)if(!(profile==model.saved[0]))mixed=true;
        if(mixed)ImGui::TextColored(gold,"Mixed profiles");else ImGui::TextColored(gold,"%s %.0f%%  /  %s %.0f%%",name(model.draft.top),100*model.draft.topBlend,name(model.draft.bottom),100*model.draft.bottomBlend);
        if(r.quickPins&hex::PinTop)changed|=region("TOP",model.draft.top,model.draft.topBlend,shapingReady,hexReady);
        if(r.quickPins&hex::PinBottom)changed|=region("BOTTOM",model.draft.bottom,model.draft.bottomBlend,shapingReady,hexReady);
        if(r.quickPins&hex::PinPad)changed|=shapePad(model.draft,250*scale,shapingReady,hexReady);
        changed|=proportions(r,shapingReady,(r.quickPins&hex::PinHeight)!=0,(r.quickPins&hex::PinWeight)!=0);
        if(r.quickPins&hex::PinUnderwear)underwearControl(r,shapingReady);
        if(r.quickPins&hex::PinOutfit)outfitControl();
        if(r.quickPins&hex::PinDancerColour)wardrobeColour(r,true);
        providerWardrobeControls(r,true);
        changed|=profileActions(r,r.quickPins);
    }
    else if(ImGui::BeginTabBar("pages")){
        const bool shape=ImGui::BeginTabItem("Shape");record("tabShape");
        if(shape){
            ImGui::Spacing();changed|=region("TOP",model.draft.top,model.draft.topBlend,shapingReady,hexReady);changed|=region("BOTTOM",model.draft.bottom,model.draft.bottomBlend,shapingReady,hexReady);ImGui::Spacing();
            changed|=shapePad(model.draft,ImGui::GetContentRegionAvail().y-108*scale,shapingReady,hexReady);ImGui::Spacing();
            changed|=proportions(r,shapingReady,true,true);
            changed|=profileActions(r,hex::PinCopy|hex::PinPaste|hex::PinReset);
            ImGui::EndTabItem();
        }
        const bool wardrobe=ImGui::BeginTabItem("Wardrobe");record("tabWardrobe");
        if(wardrobe){
            ImGui::Spacing();ImGui::PushFont(titleFont);ImGui::TextUnformatted("WARDROBE");ImGui::PopFont();ImGui::Spacing();
            ImGui::TextColored(gold,"OUTFIT");outfitControl();providerWardrobeControls(r);
            if(hex::wardrobeEditorStatus(r.model.target).selected.outfit==hex::WitchOutfit::Dancer){ImGui::Spacing();wardrobeColour(r,true);}
            ImGui::Spacing();ImGui::Separator();ImGui::Spacing();ImGui::TextColored(gold,"BASE UNDERWEAR");
            underwearControl(r,shapingReady);ImGui::TextDisabled("Hides base underwear and linked outfit parts.");
            ImGui::EndTabItem();
        }
        const bool settings=ImGui::BeginTabItem("Settings");record("tabInterface");
        if(settings){
            ImGui::Spacing();const int keys[]={VK_F9,VK_F8,VK_F10,VK_F11,VK_INSERT};const char* labels[]={"F9","F8","F10","F11","Insert"};int selected=0;for(int i=0;i<5;++i)if(r.key==keys[i])selected=i;
            ImGui::SetNextItemWidth(150*scale);if(ImGui::Combo("Shortcut",&selected,labels,5)){r.key=keys[selected];r.control=r.alt=r.shift=false;r.saveInterface();}record("hotkey");tip("Choose another key if a different mod uses this shortcut.");
            ImGui::SetNextItemWidth(200*scale);ImGui::SliderInt("Scale",&r.uiScalePercent,80,130,"%d%%");if(ImGui::IsItemDeactivatedAfterEdit())r.saveInterface();record("scale");
            pinSettings(r);
            dependencySettings(r);
            if(ImGui::CollapsingHeader("Diagnostics")){ImGui::TextWrapped("%s",r.native.shapingDetail.c_str());ImGui::TextWrapped("%s",model.message.c_str());const auto probe=hex::wardrobeProbeStatus();if(probe.ready){ImGui::BeginDisabled(probe.pending);if(ImGui::Button("Refresh selected outfit")){std::string error;if(!hex::requestWardrobeRefresh(model.target,error))model.message=error;}ImGui::EndDisabled();ImGui::TextWrapped("%s",probe.detail.c_str());}}
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();ImGui::EndChild();
    model.draft.autoFit=true;if(changed)r.selectionChanged();r.tickAuto(GetTickCount64(),!io.MouseDown[0]);
    ImGui::Separator();model.applied=model.draftIsApplied();
    const auto dependencyIssue=hex::profileDependencyIssue(model.draft,r.shapingAvailable,r.hexAvailable,r.weightAvailable);
    const bool waiting=(r.queuedMask&(model.target<0?63u:1u<<model.target))||model.pendingTransaction;ImGui::TextColored(r.autoError?gold:waiting||!dependencyIssue.empty()?muted:green,r.savePending?"Shape / saving...":!dependencyIssue.empty()?"Shape / dependency needed":waiting?"Shape / waiting":model.applied?"Shape / active":"Shape / saved");
    char shortcut[32]{};GetKeyNameTextA(LONG(MapVirtualKeyA(UINT(r.key),MAPVK_VK_TO_VSC)<<16),shortcut,32);
    tip(dependencyIssue.empty()?model.message.c_str():dependencyIssue.c_str());ImGui::SameLine();ImGui::TextDisabled("AUTO");ImGui::SameLine(ImGui::GetWindowWidth()-82*scale);ImGui::TextDisabled("%s  |  ?",shortcut[0]?shortcut:"Menu");tip("Changes apply and save automatically. Right-click a control to pin it. Use Block game input to choose whether gameplay receives input while open.");
    state.mouseActive=ImGui::IsAnyItemActive()||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId);state.textEditing=io.WantTextInput;
    ImGui::End();if(!open)setOpen(false);
}
}
