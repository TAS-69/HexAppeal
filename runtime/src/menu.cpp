#include "runtime.h"
#include "underwear.h"
#include "wardrobe.h"
#include "wardrobe_native.h"
#include "wardrobe_api.h"
#include "wardrobe_items.h"
#include "wardrobe_snapshot.h"
#include "spouse_interaction.h"
#include "character_addons.h"
#include "core/log.h"
#include "gui/menu.h"
#include "core/state.h"
#include "hooks/input.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string_view>
#ifdef HEX_MENU_TESTS
#include "../tests/controls.h"
static void record(const std::string& name){hex::test::controls[name]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};}
#else
static void record(const std::string&){}
#endif

namespace ml::gui {
static float scale=1.f;
static constexpr float maximumDisplayScale=2.f;
static constexpr float fontAtlasScale=3.f;
static bool vanillaBody=false,liveBody=false,customPreset=false;
static std::atomic<int> requestedSpouse{-1};
static std::atomic<bool> nativeWardrobeInterrupted{false};
static bool spouseWardrobeOpen=false;
static bool wardrobeEditorFromTab=false;
static int previousTarget=-1;
static std::string wardrobeGearNotice;
static std::string wardrobeEditorNotice;
static std::string wardrobeSelectionNotice;
static uint64_t nextWardrobeEditRetryAt=0;
static uint64_t nextWardrobeHudRetryAt=0;
static int addonCharacterTarget=-1;
static int addonEditorTarget=-1;
static hex::Profile addonDraft;
static int pendingAddonTarget=-2;
static hex::Profile pendingAddonProfile;
static void savePendingAddonShape(hex::Runtime& runtime){
    if(pendingAddonTarget==-2)return;
    const auto targets=pendingAddonTarget<0?hex::enabledAddonSettingsTargets():std::vector<int>{pendingAddonTarget};
    for(int target:targets){
        std::string error;
        if(hex::characterAddonEnabled(target)&&!hex::saveAddonBodyProfile(target,pendingAddonProfile,error))
            runtime.model.message=std::move(error);
    }
    pendingAddonTarget=-2;
}
static ImFont *bodyFont=nullptr,*titleFont=nullptr;
static constexpr ImVec4 accent{.91f,.38f,.66f,1},gold{.94f,.77f,.48f,1},muted{.60f,.55f,.65f,1},green{.48f,.84f,.69f,1};
static constexpr ImVec4 errorColour{.96f,.43f,.43f,1};
static constexpr const char* defaultOutfitLabel="[CD] Vanilla";
static constexpr const char* barberOutfitLabel="[SO] Barbers";
static constexpr const char* combatOutfitLabel="[SO] Combat";
static std::string baseVersionWarning,hexVersionWarning,underwearVersionWarning;
static std::string shapeBlockReason,hexBlockReason,weightBlockReason;
static bool weightReady=false;
static void versionDecoration(const std::string& warning,bool tooltip=true,int placement=0){
    if(warning.empty())return;const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();auto* draw=ImGui::GetWindowDrawList();
    if(placement!=1){draw->AddRectFilled(lo,hi,IM_COL32(239,161,97,18),5*scale);draw->AddRect(lo,hi,IM_COL32(239,175,99,125),5*scale);}
    const ImVec2 centre{hi.x+(placement==1?13:placement==3?-41:-13)*scale,placement==2?lo.y+33*scale:(lo.y+hi.y)*.5f};draw->AddCircleFilled(centre,7*scale,IM_COL32(229,166,91,235));
    auto* font=ImGui::GetFont();const auto* glyph=font->FindGlyph('!');const float fontScale=ImGui::GetFontSize()/font->FontSize;
    const ImVec2 mark{centre.x-(glyph->X0+glyph->X1)*.5f*fontScale,centre.y-(glyph->Y0+glyph->Y1)*.5f*fontScale};draw->AddText(mark,IM_COL32(36,22,28,255),"!");
    if(tooltip&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)){ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*25);ImGui::TextUnformatted(warning.c_str());ImGui::PopTextWrapPos();ImGui::EndTooltip();}
}
static void tip(const char* text){
    if(!text||!*text||!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort|ImGuiHoveredFlags_AllowWhenDisabled))return;
#ifdef HEX_MENU_TESTS
    hex::test::hoveredHelp=text;
#endif
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize()*28);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}
static float displayScale(float height) {
    const float resolutionScale=height>0.f?std::clamp(height/1080.f,.75f,maximumDisplayScale):1.f;
    const int preference=std::clamp(hex::Runtime::get().uiScalePercent,hex::minimumUiScalePercent,hex::maximumUiScalePercent);
    return resolutionScale*preference/100.f;
}
static void applyStyle(float value) {
    scale=value;auto& s=ImGui::GetStyle();s=ImGuiStyle();ImGui::StyleColorsDark();
    s.WindowRounding=15;s.ChildRounding=10;s.FrameRounding=5;s.GrabRounding=5;s.PopupRounding=8;
    s.WindowPadding={18,16};s.ItemSpacing={9,8};s.FramePadding={10,7};s.WindowBorderSize=1;s.ChildBorderSize=1;
    s.Colors[ImGuiCol_WindowBg]={.045f,.025f,.059f,.98f};s.Colors[ImGuiCol_ChildBg]={.068f,.04f,.085f,1};
    s.Colors[ImGuiCol_Border]={.27f,.13f,.27f,1};s.Colors[ImGuiCol_Text]={.98f,.93f,.96f,1};s.Colors[ImGuiCol_TextDisabled]=muted;
    s.Colors[ImGuiCol_FrameBg]={.13f,.075f,.16f,1};s.Colors[ImGuiCol_FrameBgHovered]={.24f,.10f,.24f,1};s.Colors[ImGuiCol_FrameBgActive]={.33f,.12f,.28f,1};
    s.Colors[ImGuiCol_Button]={.17f,.075f,.20f,1};s.Colors[ImGuiCol_ButtonHovered]={.36f,.12f,.29f,1};s.Colors[ImGuiCol_ButtonActive]={.48f,.16f,.35f,1};
    s.Colors[ImGuiCol_Header]={.30f,.09f,.25f,1};s.Colors[ImGuiCol_HeaderHovered]={.38f,.12f,.29f,1};s.Colors[ImGuiCol_HeaderActive]={.45f,.15f,.34f,1};
    s.Colors[ImGuiCol_CheckMark]=gold;s.Colors[ImGuiCol_SliderGrab]=accent;s.Colors[ImGuiCol_SliderGrabActive]=gold;
    s.Colors[ImGuiCol_Separator]={.29f,.12f,.27f,1};s.Colors[ImGuiCol_Tab]={.12f,.06f,.15f,1};s.Colors[ImGuiCol_TabHovered]={.36f,.12f,.29f,1};s.Colors[ImGuiCol_TabSelected]={.35f,.105f,.28f,1};
    s.ScaleAllSizes(scale);
    ImGui::GetIO().FontGlobalScale=scale/fontAtlasScale;
}
void InitStyle(float displayHeight) {
    applyStyle(displayScale(displayHeight));
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.ConfigFlags&=~(ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad);
    char windows[MAX_PATH]{};GetWindowsDirectoryA(windows,MAX_PATH);
    const auto fonts=std::string(windows)+"\\Fonts\\";
    static constexpr ImWchar glyphs[]={0x0020,0x00ff,0x2000,0x206f,0};
    bodyFont=nullptr;
    titleFont=nullptr;
    if(std::filesystem::exists(fonts+"segoeui.ttf"))bodyFont=io.Fonts->AddFontFromFileTTF((fonts+"segoeui.ttf").c_str(),16*fontAtlasScale,nullptr,glyphs);
    if(!bodyFont){ImFontConfig config;config.SizePixels=16*fontAtlasScale;bodyFont=io.Fonts->AddFontDefault(&config);}
    if(std::filesystem::exists(fonts+"segoeuib.ttf"))titleFont=io.Fonts->AddFontFromFileTTF((fonts+"segoeuib.ttf").c_str(),27*fontAtlasScale);
    if(!titleFont)titleFont=bodyFont;io.FontDefault=bodyFont;
}
void SetDisplayHeight(float height) {
    const float nextScale=displayScale(height);
    if(nextScale!=scale)applyStyle(nextScale);
}
static void finishWardrobeEditor(hex::Runtime& runtime,bool releaseInteraction=true){
    if(!wardrobeEditorFromTab&&!runtime.preview)hex::setWardrobePreviewHudHidden(false);
    if(addonEditorTarget>=0){
        hex::cancelSavedWardrobe(addonEditorTarget);
        addonEditorTarget=-1;
    }else{
        if(!runtime.preview)hex::cancelSavedWardrobe(runtime.model.target);
        runtime.model.selectTarget(previousTarget);
    }
    if(!wardrobeEditorFromTab&&releaseInteraction)hex::requestWardrobeInteractionClose();
    spouseWardrobeOpen=false;
    wardrobeEditorFromTab=false;
}
static void setOpen(bool open,bool releaseInteraction=true){
    auto& state=State::Get();
    if(state.menuOpen==open&&(open||!spouseWardrobeOpen))return;
    state.menuOpen=open;
    if(open)input::MenuOpened();
    else {
        auto& runtime=hex::Runtime::get();
        savePendingAddonShape(runtime);
        runtime.tickAuto(GetTickCount64(),true);
        if(spouseWardrobeOpen)finishWardrobeEditor(runtime,releaseInteraction);
        if(addonCharacterTarget>=0)hex::cancelAddonWardrobe(addonCharacterTarget);
        input::MenuClosed();
    }
}
bool RequestSpouseWardrobe(int witch) noexcept {
    if(witch<0||(witch>=int(hex::witches.size())&&!hex::characterAddonEnabled(witch)))return false;
    requestedSpouse.store(witch);
    return true;
}
static std::atomic<bool> overlayInterrupted{false};
void OverlayInterrupted() noexcept {
    requestedSpouse.store(-1);
    overlayInterrupted.store(true);
}
void CancelNativeWardrobe() noexcept {
    requestedSpouse.store(-1);
    nativeWardrobeInterrupted.store(true);
}
void NoteNativeWardrobeDrawn() noexcept {
    if(!spouseWardrobeOpen||wardrobeEditorFromTab||!State::Get().menuOpen)return;
    const auto now=GetTickCount64();
    State::Get().lastNativeWardrobeDrawAt.store(now);
    if(!hex::Runtime::get().preview&&now>=nextWardrobeHudRetryAt){
        nextWardrobeHudRetryAt=now+500;
        hex::setWardrobePreviewHudHidden(true);
    }
}
static void tryStartWardrobeEdit(hex::Runtime& runtime,int target){
    const auto now=GetTickCount64();
    if(runtime.preview||now<nextWardrobeEditRetryAt)return;
    nextWardrobeEditRetryAt=now+500;
    std::string error;
    const bool opened=target>=hex::firstAddonCharacter?
        hex::openAddonWardrobe(target,error,wardrobeEditorFromTab):hex::openSavedWardrobe(target,error);
    if(opened)wardrobeEditorNotice.clear();
    else if(wardrobeEditorNotice!=error){
        LOG("Wardrobe is waiting for character %d: %s",target,error.c_str());
        wardrobeEditorNotice=std::move(error);
    }
}
static void enterWardrobeEditor(hex::Runtime& runtime,int witch,bool fromTab){
    if(witch>=hex::firstAddonCharacter){
        if(!hex::characterAddonEnabled(witch))return;
        if(spouseWardrobeOpen)finishWardrobeEditor(runtime);
        addonEditorTarget=witch;
        spouseWardrobeOpen=true;
        wardrobeEditorFromTab=fromTab;
        wardrobeGearNotice.clear();
        wardrobeEditorNotice.clear();
        nextWardrobeEditRetryAt=0;
        tryStartWardrobeEdit(runtime,witch);
        setOpen(true);
        nextWardrobeHudRetryAt=0;
        return;
    }
    if(addonEditorTarget>=0)finishWardrobeEditor(runtime);
    if(!spouseWardrobeOpen)previousTarget=runtime.model.target;
    else if(witch!=runtime.model.target&&!runtime.preview)hex::cancelSavedWardrobe(runtime.model.target);
    if(runtime.model.target!=witch)runtime.model.selectTarget(witch);
    spouseWardrobeOpen=true;
    wardrobeEditorFromTab=fromTab;
    wardrobeGearNotice.clear();
    wardrobeEditorNotice.clear();
    nextWardrobeEditRetryAt=0;
    tryStartWardrobeEdit(runtime,witch);
    setOpen(true);
    if(!runtime.preview&&fromTab)hex::setWardrobePreviewHudHidden(false);
    nextWardrobeHudRetryAt=0;
}
void PollToggle(){
    auto& r=hex::Runtime::get();
    if(overlayInterrupted.exchange(false))setOpen(false);
    if(nativeWardrobeInterrupted.exchange(false)&&spouseWardrobeOpen&&!wardrobeEditorFromTab)
        setOpen(false,false);
    const auto spouse=requestedSpouse.exchange(-1);
    if(spouse>=0)enterWardrobeEditor(r,spouse,false);
    if(r.preview)return;
    r.tickAuto();
    static bool before=false;const auto down=[](int key){return (input::AsyncKeyRaw(key)&0x8000)!=0;};
    const bool held=State::ForegroundIsOurs()&&down(r.key)&&(down(VK_CONTROL)==r.control)&&(down(VK_MENU)==r.alt)&&(down(VK_SHIFT)==r.shift)&&!down(VK_LWIN)&&!down(VK_RWIN);
    if(spouse<0&&held&&!before)setOpen(!State::Get().menuOpen);before=held;
}
bool WantsDraw(){return State::Get().menuOpen;}
void FeedInput(){if(!hex::Runtime::get().preview)input::FeedInput(ImGui::GetIO());}
static const char* name(hex::Preset p){return p==hex::Preset::Hex?(customPreset?"PRESET":"HEX"):p==hex::Preset::Flat?"FLAT":vanillaBody?"VANILLA":liveBody||(addonCharacterTarget>=0&&!hex::addonVenusBodyActive(addonCharacterTarget))?"ORIGINAL":"VENUS";}
static bool choice(const char* text,bool selected,ImVec2 size){if(selected){ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.47f,.14f,.34f,1));ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,.88f,.64f,1));}const bool clicked=ImGui::Button(text,size);if(selected)ImGui::PopStyleColor(2);return clicked;}
static void pinContext(uint32_t bit){
    auto& r=hex::Runtime::get();const auto id=std::string("pin-menu-")+std::to_string(bit);
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)&&ImGui::IsMouseReleased(1))ImGui::OpenPopup(id.c_str());
    if(ImGui::BeginPopup(id.c_str())){const bool pinned=(r.quickPins&bit)!=0;if(ImGui::MenuItem(pinned?"Unpin from quick panel":"Pin to quick panel")){r.quickPins^=bit;r.saveInterface();}record("pinAction"+std::to_string(bit));ImGui::EndPopup();}
}
static bool region(const char* label,hex::Preset& preset,float& blend,bool available,bool hexAvailable){
    bool changed=false;ImGui::PushID(label);ImGui::BeginGroup();ImGui::AlignTextToFramePadding();ImGui::TextColored(gold,"%s",label);ImGui::SameLine(70*scale);
    const float width=std::max(35.f,(ImGui::GetContentRegionAvail().x-2*ImGui::GetStyle().ItemSpacing.x)/3);
    for(int i=0;i<3;++i){
        if(i)ImGui::SameLine();
        const bool disabled=!available||(i==1&&!hexAvailable);
        ImGui::BeginDisabled(disabled);
        if(choice(name(hex::Preset(i)),int(preset)==i,{width,28*scale})){preset=hex::Preset(i);blend=1.f;changed=true;}
        record(std::string(label)+".preset"+std::to_string(i));
        ImGui::EndDisabled();
        versionDecoration(baseVersionWarning+(i==1?hexVersionWarning:""),!disabled);
        if(disabled)tip((!available?shapeBlockReason:hexBlockReason).c_str());
    }
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
    const auto baseline=name(hex::Preset::Venus);const auto sz=ImGui::CalcTextSize(baseline);draw->AddText({centre.x-sz.x*.5f,centre.y+12*scale},IM_COL32(226,184,147,205),baseline);
    const ImVec2 marker{start.x+(hex::regionCoordinate(profile.bottom,profile.bottomBlend)+1)*.5f*side,start.y+(hex::regionCoordinate(profile.top,profile.topBlend)+1)*.5f*side};
    draw->AddLine(centre,marker,IM_COL32(244,186,136,100),scale);
    draw->AddCircleFilled(marker,16*scale,IM_COL32(235,83,157,40));draw->AddCircleFilled(marker,10*scale,IM_COL32(22,8,26,255));draw->AddCircle(marker,10*scale,IM_COL32(255,208,137,255),40,2*scale);draw->AddCircleFilled(marker,4*scale,IM_COL32(255,227,183,255));
    const bool hovered=ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)||ImGui::IsItemActive();
    if(!available)tip(shapeBlockReason.c_str());
    pinContext(hex::PinPad);
    versionDecoration(baseVersionWarning+hexVersionWarning,false,2);
    if(available&&hovered&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)){
        ImGui::BeginTooltip();ImGui::Text("TOP  %s %.0f%%",name(profile.top),profile.top==hex::Preset::Venus?100:profile.topBlend*100);ImGui::Text("BOTTOM  %s %.0f%%",name(profile.bottom),profile.bottom==hex::Preset::Venus?100:profile.bottomBlend*100);
        if(!hexAvailable){ImGui::PushTextWrapPos(ImGui::GetFontSize()*28);ImGui::TextUnformatted(hexBlockReason.c_str());ImGui::PopTextWrapPos();}
        if(!(baseVersionWarning+hexVersionWarning).empty()){ImGui::PushTextWrapPos(ImGui::GetFontSize()*25);ImGui::TextUnformatted((baseVersionWarning+hexVersionWarning).c_str());ImGui::PopTextWrapPos();}ImGui::EndTooltip();
    }
    return changed;
}
static void dependencySettings(hex::Runtime& r){
    const bool expanded=ImGui::CollapsingHeader("Dependencies & paths");record("dependencyHeader");if(!expanded)return;
    const auto dependency=r.dependencyStatus();
    const auto& status=*dependency;
    const auto state=[&](const char* label,bool available,const std::string& detail){ImGui::TextColored(available?green:muted,"%s  /  %s",label,available?"Ready":"Unavailable");tip(detail.c_str());};
    const auto fileState=[&](const char* label,const hex::DependencyFile& file){ImGui::TextColored(!file.valid?muted:file.verified?green:gold,"%s / %s%s",label,!file.valid?"File needed":file.verified?"Verified":"Unverified",file.valid&&!file.verified?"  (!)":"");const auto detail=file.detail+(file.path.empty()?std::string{}:"\n"+file.path.u8string());tip(detail.c_str());};
    fileState("Crimson Desert",status.game);fileState("Instant Surgery",status.instant);
    if(status.bodyMode==hex::BodyMode::Vanilla)state("Vanilla body",status.bodyVerified,status.bodyDetail);
    else if(status.bodyMode==hex::BodyMode::Live)state("Live body",status.canShapeBody(),status.bodyDetail);
    else fileState("Venus Body for Damiane",status.venus);
    if(status.customPreset)state("Custom preset",r.hexAvailable,r.featureCompatibility(hex::Feature::Hex).detail);else fileState("Hex's Hentai Preset",status.hex);if(status.physics.valid)fileState("Two Reasons Body Physics",status.physics);if(status.creator.valid)fileState("Character Creator",status.creator);
    ImGui::Spacing();ImGui::TextDisabled("Feature checks");const auto capabilities=r.compatibilityStatus();
    for(size_t i=0;i<capabilities.size();++i){const auto& feature=capabilities[i];ImGui::TextColored(feature.state==hex::CompatibilityState::Ready?green:feature.state==hex::CompatibilityState::Blocked?errorColour:muted,"%s / %s%s",hex::featureName(hex::Feature(i)),feature.state==hex::CompatibilityState::Ready?"Ready":feature.state==hex::CompatibilityState::Blocked?"Blocked":"Checking",feature.state==hex::CompatibilityState::Blocked?"  (!)":"");tip(feature.detail.c_str());record("feature"+std::to_string(i));}
    {
        std::string routes=status.bodyWarning;
        const auto addText=[&](const std::string& text){if(text.empty())return;if(!routes.empty())routes+="\n\n";routes+=text;};
        for(size_t i=0;i<status.witchBodyWarnings.size();++i)
            if(!status.witchBodyWarnings[i].empty())addText(std::string(hex::witches[i].name)+": "+status.witchBodyWarnings[i]);
        addText(status.bodySkipped);
        if(!routes.empty()){ImGui::TextColored(gold,"Active body routes / unverified  (!)");tip(routes.c_str());}
    }
    if(!status.bodyNote.empty())ImGui::TextWrapped("%s",status.bodyNote.c_str());
    static std::array<std::array<char,32768>,6> buffers{};static std::array<std::string,6> synced{};static std::array<bool,6> editing{};
    auto paths=r.configuredDependencyPaths();bool changed=false;
    const auto field=[&](int i,const char* label,const char* control,std::filesystem::path& path){
        const auto value=path.u8string();if(!editing[i]&&synced[i]!=value){snprintf(buffers[i].data(),buffers[i].size(),"%s",value.c_str());synced[i]=value;}
        const std::string inputId=std::string("##")+control;
        ImGui::SetNextItemWidth(-1);ImGui::TextDisabled("%s",label);ImGui::InputTextWithHint(inputId.c_str(),"Auto-detect",buffers[i].data(),buffers[i].size());record(control);editing[i]=ImGui::IsItemActive();
        if(ImGui::IsItemDeactivatedAfterEdit()){path=std::filesystem::u8path(buffers[i].data());synced[i]=path.u8string();changed=true;}
    };
    field(0,"Mod manager config","pathManager",paths.managerConfig);field(1,"Mods folder","pathMods",paths.modsDirectory);field(2,"Active file list (DMM manifest.tsv)","pathManifest",paths.manifest);
    ImGui::TextDisabled("Shape preset source");
    const auto source=paths.hexPreset.empty()?std::string("Automatic (Hex)"):paths.hexPreset.filename().u8string();
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##presetSource",source.c_str())){
        if(ImGui::Selectable("Automatic (Hex)",paths.hexPreset.empty())){paths.hexPreset.clear();changed=true;}
        for(const auto& preset:status.surgeryPresets){
            ImGui::PushID(preset.u8string().c_str());
            const auto label=preset.filename()==L"Preset.ini"&&preset.parent_path().filename()==L"InstantSurgery"?"Instant Surgery / saved body":preset.filename().u8string();
            if(ImGui::Selectable(label.c_str(),paths.hexPreset==preset)){paths.hexPreset=preset;changed=true;}
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    tip("Choose a saved Instant Surgery preset for the top-left of the shape pad. The selection applies anywhere a profile uses this preset slot.");
    field(3,"Preset .ini path (or a folder to find Hex's Hentai Preset)","pathHex",paths.hexPreset);if(status.bodyMode!=hex::BodyMode::Vanilla)field(4,"Venus Body for Damiane file (optional source override)","pathVenus",paths.venusBody);
    if(changed){std::string error;if(!r.setDependencyPaths(paths,error))r.model.message=error;}
    if(ImGui::Button("Detect again"))++r.dependencyRevision;record("dependencyRescan");tip("Paths are saved automatically. Empty fields use detection. Active body verification currently requires DMM's loaded runtime and file list; another manager's source folder alone cannot prove which body the game uses. Installing or changing runtime plugins requires a restart.");
}
static void pinSettings(hex::Runtime& r){
    const bool open=ImGui::CollapsingHeader("Quick panel pins");record("pinHeader");if(!open)return;
    struct Item{const char* label;uint32_t bit;};
    const Item items[]={{"Top selector",hex::PinTop},{"Bottom selector",hex::PinBottom},{"Shape pad",hex::PinPad},{"Height",hex::PinHeight},{"Weight",hex::PinWeight},{"Base underwear",hex::PinUnderwear},{"Hide outfit",hex::PinHideOutfit},{"Outfit switch",hex::PinOutfit},{"Copy",hex::PinCopy},{"Paste",hex::PinPaste},{"Reset",hex::PinReset}};
    for(const auto& item:items){bool selected=(r.quickPins&item.bit)!=0;if(ImGui::Checkbox(item.label,&selected)){r.quickPins^=item.bit;r.saveInterface();}record("pin"+std::to_string(item.bit));}
    ImGui::TextDisabled("Or right-click a control to pin it.");
}
static void skinSettings(hex::Runtime& runtime){
    const auto dependency=runtime.dependencyStatus();
    const auto& status=*dependency;
    if(!status.scanned||status.bodyMode!=hex::BodyMode::Venus)return;
    if(!ImGui::CollapsingHeader("Skin tone")){record("skinHeader");return;}
    record("skinHeader");
    const auto& source=status.skinTone;
    ImGui::BeginDisabled(!source.custom()&&!runtime.skinToneEnabled);
    if(ImGui::Checkbox("Match Damiane’s VENUS skin",&runtime.skinToneEnabled)&&!runtime.saveInterface())
        runtime.skinToneEnabled=!runtime.skinToneEnabled;
    record("skinTone");
    ImGui::EndDisabled();
    const auto failure=runtime.skinToneFailure();
    tip((runtime.skinToneAtStartup&&!failure.empty()?failure:source.detail).c_str());
    ImGui::TextWrapped("%s",source.detail.c_str());
    if(runtime.skinToneEnabled!=runtime.skinToneAtStartup)ImGui::TextWrapped("Restart Crimson Desert to apply this change.");
    else if(runtime.skinToneAtStartup&&runtime.skinToneFailed.load()){
        ImGui::TextWrapped("Skin match unavailable");
        tip(failure.c_str());
    }
    else if(runtime.skinToneAtStartup&&runtime.skinTonePrepared.load())
        ImGui::Text("%u of %u witch skin routes loaded",runtime.skinToneApplied.load(),runtime.skinTonePrepared.load());
    else if(runtime.skinToneAtStartup)ImGui::TextWrapped("Preparing witch-only skin routes…");
}
static int wardrobeTarget(const hex::Runtime& runtime){
    return addonCharacterTarget>=0?addonCharacterTarget:runtime.model.target;
}
static void underwearControl(hex::Runtime& r){
    const int target=wardrobeTarget(r);
    const bool addon=target>=hex::firstAddonCharacter;
    const auto addonStatus=addon?hex::addonWardrobeStatus(target):hex::AddonWardrobeStatus{};
    const bool addonFallback=addon&&!hex::addonVenusBodyActive(target);
    auto status=addonFallback?hex::UnderwearStatus{addonStatus.canSwitch,hex::addonUnderwearHidden(target),false,!addonStatus.actorSeen,{}}:
        hex::underwearStatus(target);
    const auto ownership=addonFallback?hex::UnderwearOwnership{addonStatus.externalUnderwear,addonStatus.externalUnderwear,
        addonStatus.actorSeen&&!addonStatus.underwearPresent,{}}:hex::underwearOwnership(target);
    const auto compatibility=r.featureCompatibility(hex::Feature::Underwear);
    const auto addons=target<0?hex::enabledAddonSettingsTargets():std::vector<int>{};
    for(int other:addons){
        const bool addonHidden=hex::addonUnderwearHidden(other);
        status.mixed|=status.hidden!=addonHidden;
        status.hidden&=addonHidden;
    }
    {
        bool external=r.allowExternalUnderwear.load();
        if(ImGui::Checkbox("Allow external control",&external)){
            std::string error;if(!hex::setExternalUnderwear(external,error))r.model.message=error;
        }
        record("underwearOwner");
        tip("On: defer to detected underwear mods and outfit rules for the characters they affect. Off: use Hex Appeal's saved controls. To show a hidden base part, hide the outfit first.");
        if(ownership.delegated)ImGui::TextWrapped(ownership.fullyExternal?"Controlled by %s":"Some witches controlled by %s",
            ownership.source.empty()?"game or outfit rules":ownership.source.c_str());
    }
    const auto missingReason=ownership.meshSource.empty()?std::string("The active appearance contains no base underwear mesh."):
        ownership.meshSource+" supplies the mounted body with its base underwear mesh removed.";
    const auto visibilityReason=ownership.meshMissing?missingReason:
        ownership.fullyExternal?(ownership.source.empty()?"The active game or outfit rule controls base underwear.":ownership.source+" controls base underwear."):
        !status.ready?(addon?addonStatus.detail:compatibility.detail):std::string{};
    bool hidden=status.hidden;
    ImGui::BeginDisabled(addons.empty()&&(ownership.meshMissing||ownership.fullyExternal));
    if(ImGui::Checkbox("Hide base bra & panties",&hidden)){
        std::string error;
        if(addon){if(!hex::saveAddonUnderwearHidden(target,hidden,error))r.model.message=error;}
        else{
            if(!ownership.fullyExternal&&!hex::setUnderwearHidden(target,hidden,error))r.model.message=error;
            for(int other:addons)if(!hex::saveAddonUnderwearHidden(other,hidden,error))r.model.message=error;
        }
    }
    record("underwear");ImGui::EndDisabled();
    if(!visibilityReason.empty())tip(visibilityReason.c_str());
    pinContext(hex::PinUnderwear);
    if(ownership.meshMissing){ImGui::TextWrapped("%s",missingReason.c_str());record("underwearMeshMissing");tip(missingReason.c_str());}
    else{
        if(!addon)versionDecoration(underwearVersionWarning,visibilityReason.empty(),1);
        if(visibilityReason.empty()&&(addon||underwearVersionWarning.empty()))tip("Hides the game's base underwear and compatible Wardrobe bra/panties. Built-in outfit pieces need provider support.");
        if(!addon&&compatibility.state==hex::CompatibilityState::Blocked){ImGui::SameLine();ImGui::TextColored(errorColour,"(!)");tip(compatibility.detail.c_str());}
    }
    if(status.mixed){ImGui::SameLine();ImGui::TextColored(gold,"Mixed");}
    if(!ownership.meshMissing&&!ownership.fullyExternal&&!status.ready&&status.hidden)
        ImGui::TextDisabled("Saved. Waiting for base visibility.");
    if(!ownership.meshMissing&&status.ready&&status.pending&&status.hidden&&target>=0){ImGui::SameLine();ImGui::TextDisabled("Waiting for base part");tip(status.detail.c_str());}
}
static void outfitHiddenControl(hex::Runtime& r){
    const int target=wardrobeTarget(r);
    const bool addon=target>=hex::firstAddonCharacter;
    const auto addonStatus=addon?hex::addonWardrobeStatus(target):hex::AddonWardrobeStatus{};
    auto status=addon?hex::UnderwearStatus{addonStatus.canSwitch,addonStatus.hidden,false,!addonStatus.actorSeen,addonStatus.detail}:
        hex::outfitHiddenStatus(target);
    const auto addons=target<0?hex::enabledAddonSettingsTargets():std::vector<int>{};
    for(const int other:addons){
        const auto otherStatus=hex::addonWardrobeStatus(other);
        status.mixed|=status.hidden!=otherStatus.hidden;
        status.hidden&=otherStatus.hidden;
    }
    bool hidden=status.hidden;
    if(ImGui::Checkbox("Hide outfit",&hidden)){
        std::string error;
        if(addon){if(!hex::setAddonOutfitHidden(target,hidden,error))r.model.message=error;}
        else if(!hex::setOutfitHidden(target,hidden,error))r.model.message=error;
        else for(const int other:addons)
            if(!hex::setAddonOutfitHidden(other,hidden,error))r.model.message=error;
    }
    record("outfitHidden");pinContext(hex::PinHideOutfit);versionDecoration(underwearVersionWarning,true,1);
    if(underwearVersionWarning.empty())tip("Hides equipped outfit parts. The slots and base underwear stay as they are.");
    if(status.mixed){ImGui::SameLine();ImGui::TextColored(gold,"Mixed");}
    if(status.hidden&&(!status.ready||status.pending)){ImGui::SameLine();ImGui::TextDisabled("Waiting for outfit visibility");tip(status.detail.c_str());}
}
static void outfitControl(){
    auto& runtime=hex::Runtime::get();
    const int target=wardrobeTarget(runtime);
    const auto status=hex::wardrobeEditorStatus(target);
    const auto selected=hex::wardrobeOutfitId(status.selected);
    const auto providers=target>=hex::firstAddonCharacter?std::vector<hex::WardrobeProviderOutfit>{}:hex::wardrobeProviderOutfits();
    const auto activeProvider=std::find_if(providers.begin(),providers.end(),[&](const auto& provider){
        return provider.id==selected&&provider.available&&!provider.removed;
    });
    const bool native=activeProvider==providers.end();
    const bool single=target>=0;
    std::string label=native?defaultOutfitLabel:activeProvider->label;
    if(native){
        switch(status.slotSelection){
            case hex::SlotOutfit::Game:label="Follow game";break;
            case hex::SlotOutfit::Barber:label=barberOutfitLabel;break;
            case hex::SlotOutfit::Saved:label="[HA] Wardrobe";break;
            default:break;
        }
    }
    if(status.spouseCombatActive)label=combatOutfitLabel;
    const auto addons=single?std::vector<int>{}:hex::enabledAddonSettingsTargets();
    const bool mixed=status.mixedOutfit||(native&&std::any_of(addons.begin(),addons.end(),[&](int other){
        return hex::addonWardrobeStatus(other).selected!=status.slotSelection;
    }));
    const auto dependencies=runtime.dependencyStatus();
    ImGui::BeginDisabled(status.spouseCombatActive);ImGui::SetNextItemWidth(-1);
    const bool opened=ImGui::BeginCombo("##outfit",mixed?"Mixed":label.c_str());record("outfit");
    if(opened){
        const auto choose=[&](const char* text,hex::SlotOutfit option){
            const bool picked=ImGui::Selectable(text,!mixed&&native&&status.slotSelection==option);
            record(std::string("outfitProfile")+std::to_string(int(option)));
            if(!picked)return;
            wardrobeSelectionNotice.clear();
            std::string error;
            if(!hex::requestSlotOutfit(target,option,error))wardrobeSelectionNotice=error;
            else for(int other:addons)
                if(!hex::requestSlotOutfit(other,option,error))wardrobeSelectionNotice=error;
        };
        choose("Follow game",hex::SlotOutfit::Game);
        tip("Let the game or Spouse Overhaul apply their current outfit.");
        choose(defaultOutfitLabel,hex::SlotOutfit::Default);
        if(single){
            if(status.barberOutfitAvailable)choose(barberOutfitLabel,hex::SlotOutfit::Barber);
            if(status.savedOutfitAvailable)choose("[HA] Wardrobe",hex::SlotOutfit::Saved);
        }
        for(const auto& provider:providers){if(!provider.itemSlot.empty()||(provider.removed&&provider.id!=selected))continue;ImGui::PushID(provider.id.c_str());ImGui::BeginDisabled(!provider.available||provider.removed);
            if(ImGui::Selectable(provider.label.c_str(),selected==provider.id)){wardrobeSelectionNotice.clear();std::string error;if(!hex::requestWardrobeOutfit(target,provider.id,error))wardrobeSelectionNotice=error;}
            ImGui::EndDisabled();tip((provider.available?provider.requirements:provider.reason).c_str());ImGui::PopID();}
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if(!opened){
        pinContext(hex::PinOutfit);
        auto warning=dependencies->game.valid&&!dependencies->game.verified?dependencies->game.detail:std::string{};
        const auto hooks=hex::wardrobeProbeStatus().versionWarning;
        if(!hooks.empty()){if(!warning.empty())warning+='\n';warning+=hooks;}
        versionDecoration(warning,false,3);
        auto help=status.spouseCombatActive?std::string("Spouse Overhaul owns the active Combat Gear profile."):status.detail;
        if(!warning.empty())help+=(help.empty()?"":"\n\n")+warning;
        tip(help.c_str());
    }
    if(mixed)ImGui::TextColored(gold,"Mixed outfits");
    if(status.spouseCombatActive)ImGui::TextDisabled("Spouse Overhaul controls the active Combat Gear profile.");
    else ImGui::TextWrapped("%s",status.caption.c_str());
    tip(status.spouseCombatActive?"Spouse Overhaul owns the active Combat Gear profile.":status.detail.c_str());
    if(!wardrobeSelectionNotice.empty())ImGui::TextColored(errorColour,"%s",wardrobeSelectionNotice.c_str());
}
static void wardrobeSlotsControl(hex::Runtime& runtime){
    const int witch=wardrobeTarget(runtime);
    if(witch<0)return;
    auto state=hex::wardrobeEditorStatus(witch);
    const bool expanded=ImGui::CollapsingHeader("Outfit slots");record("outfitSlots");
    if(!expanded)return;
    for(const auto& [slot,parts]:state.slots){
        bool hidden=state.hiddenSlots.count(slot)!=0;
        ImGui::PushID(slot.c_str());
        if(ImGui::Checkbox(("Hide "+slot).c_str(),&hidden)){
            std::string error;
            if(!hex::setWardrobeSlotHidden(witch,slot,hidden,error))runtime.model.message=error;
        }
        std::string names;
        for(const auto& part:parts){if(!names.empty())names+='\n';names+=part;}
        tip(names.c_str());ImGui::PopID();
    }

}
static void providerWardrobeControls(hex::Runtime& runtime,bool pinnedOnly=false){
    if(wardrobeTarget(runtime)>=hex::firstAddonCharacter)return;
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
static void equippedItemControls(hex::Runtime& runtime,bool preview,int target=-1){
    const int witch=target<0?wardrobeTarget(runtime):target;
    const auto options=hex::equippedWardrobeItemControls(witch,preview);
    if(options.empty()||!ImGui::CollapsingHeader("Equipped item options",ImGuiTreeNodeFlags_DefaultOpen))return;
    std::string previous;
    for(const auto& entry:options){
        if(entry.outfit!=previous){
            ImGui::TextDisabled("%s | %s",entry.slot.c_str(),entry.label.c_str());
            previous=entry.outfit;
        }
        const auto& control=entry.control;
        ImGui::PushID(entry.outfit.c_str());
        ImGui::PushID(control.id.c_str());
        ImGui::BeginDisabled(!entry.available);
        uint32_t value=entry.value;
        if(control.type==HA_TOGGLE){
            bool enabled=value!=0;
            if(ImGui::Checkbox(control.label.c_str(),&enabled))value=enabled;
        }else if(ImGui::BeginCombo(control.label.c_str(),control.options[value].label.c_str())){
            for(uint32_t i=0;i<control.options.size();++i)
                if(ImGui::Selectable(control.options[i].label.c_str(),value==i))value=i;
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        const auto help=entry.available?control.help:entry.reason;
        tip(help.c_str());
        if(value!=entry.value){
            std::string error;
            if(!hex::setEquippedWardrobeItemControl(witch,preview,entry.outfit,control.id,value,error)){
                auto& notice=preview?wardrobeEditorNotice:runtime.model.message;
                notice=std::move(error);
            }
        }
        ImGui::PopID();
        ImGui::PopID();
    }
}
static bool proportions(hex::Profile& profile,bool available,bool height,bool weight){
    bool changed=false;
    const auto slider=[&](const char* label,const char* id,float& stored,float factor,float limit,const char* format,bool enabled,uint32_t pin,const char* help){
        ImGui::PushID(id);ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);ImGui::SameLine(68*scale);
        ImGui::BeginDisabled(!enabled);float value=stored*factor;
        ImGui::SetNextItemWidth(std::max(100.f,ImGui::GetContentRegionAvail().x-24*scale));
        if(ImGui::SliderFloat("##value",&value,-limit,limit,format,ImGuiSliderFlags_AlwaysClamp)){stored=value/factor;changed=true;}
        record(id);ImGui::EndDisabled();pinContext(pin);versionDecoration(baseVersionWarning,enabled,1);
        if(!enabled)tip((available?weightBlockReason:shapeBlockReason).c_str());
        else if(baseVersionWarning.empty())tip(help);
        ImGui::PopID();
    };
    if(height)slider("Height","height",profile.height,10,10,"%+.1f%%",available,hex::PinHeight,"Scales the complete rig and fitted clothing. Independent of Weight; Ctrl-click for an exact value.");
    if(weight)slider("Weight","weight",profile.weight,100,100,"%+.0f%%",available&&weightReady,hex::PinWeight,weightReady?"Limbs and waist, independent of Height and breast/butt presets. No outfit dependency required.":"Native shaping must be ready to use Weight.");
    return changed;
}
static bool profileActions(hex::Runtime& r,hex::Profile& profile,uint32_t mask,bool canEdit){
    bool changed=false;const auto item=[&](const char* label,uint32_t bit){if(!(mask&bit))return;
        ImGui::BeginDisabled(bit!=hex::PinCopy&&(!canEdit||(bit==hex::PinPaste&&!r.hasCopied)));
        if(ImGui::SmallButton(label)){if(bit==hex::PinCopy){r.copied=profile;r.hasCopied=true;}else if(bit==hex::PinPaste){profile=r.copied;changed=true;}else{profile=hex::Profile{};changed=true;}}
        record(bit==hex::PinCopy?"copy":bit==hex::PinPaste?"paste":"reset");ImGui::EndDisabled();pinContext(bit);
        if(bit!=hex::PinCopy&&!canEdit)tip(shapeBlockReason.c_str());
        else if(bit==hex::PinPaste&&!r.hasCopied)tip("Copy a shape first.");
    };
    item("Copy",hex::PinCopy);if(mask&hex::PinCopy&&mask&(hex::PinPaste|hex::PinReset))ImGui::SameLine();item("Paste",hex::PinPaste);if(mask&hex::PinPaste&&mask&hex::PinReset)ImGui::SameLine();item("Reset",hex::PinReset);return changed;
}
static void logTab(hex::Runtime& r,const hex::GraphicsReportSource& graphics){
    static std::string report,recent,message;
    static uint64_t updated=0;
    const auto now=GetTickCount64();
    const bool refresh=ImGui::Button("Refresh");
    if(refresh||report.empty()||now-updated>=1000){
        report=r.diagnosticReport();recent=r.recentLog();updated=now;
    }
    ImGui::SameLine();
    if(ImGui::Button("Save report")){
        std::filesystem::path path;
        if(r.saveDiagnosticReport(path,message,graphics))message="Saved to "+path.u8string();
    }
    record("saveLogReport");
    ImGui::SameLine();
    if(ImGui::Button("Copy report")){
        const auto contents=report+"\nRecent runtime log\n"+recent;
        ImGui::SetClipboardText(contents.c_str());message="Copied report.";
    }
    ImGui::TextDisabled("Save report also collects graphics details. Reports include local paths.");
    if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
    if(ImGui::CollapsingHeader("Checks and mod sources",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PushTextWrapPos();ImGui::TextUnformatted(report.c_str());ImGui::PopTextWrapPos();
    }
    if(ImGui::CollapsingHeader("Recent runtime log",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PushTextWrapPos();ImGui::TextUnformatted(recent.empty()?"No runtime entries yet.":recent.c_str());ImGui::PopTextWrapPos();
    }
}
static void wardrobeItemIcon(const hex::WardrobeItem* item){
    const float size=36*scale;
    const auto position=ImGui::GetCursorScreenPos();
    ImGui::Dummy({size,size});
    if(!ImGui::IsItemVisible())return;
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(position,{position.x+size,position.y+size},IM_COL32(52,23,61,255),4*scale);
    if(!item||item->icon.empty())return;
    constexpr auto pixels=hex::wardrobeIconSize;
    const auto step=size/pixels;
    for(int y=0;y<pixels;++y){
        int x=0;
        while(x<pixels){
            const auto colour=item->icon[y*pixels+x];
            int end=x+1;
            while(end<pixels&&item->icon[y*pixels+end]==colour)++end;
            if(colour>>24)draw->AddRectFilled({position.x+x*step,position.y+y*step},{position.x+end*step,position.y+(y+1)*step},colour);
            x=end;
        }
    }
}
enum class GearState { Idle, Loading, Ready };
struct GearRequest {
    std::filesystem::path bin,root,manifest;
    std::filesystem::file_time_type mount,map,local,rules,registry;
    std::string body;
    bool operator==(const GearRequest& other)const{
        return bin==other.bin&&root==other.root&&manifest==other.manifest&&mount==other.mount&&
            map==other.map&&local==other.local&&rules==other.rules&&registry==other.registry&&body==other.body;
    }
};
static std::atomic<GearState> gearState{GearState::Idle};
using GearCatalog=std::shared_ptr<const hex::WardrobeItemCatalog>;
struct GearResult {
    GearCatalog stock,local,combined;
    bool partial=false;
};
struct GearTask {
    GearRequest input;
    GearResult previous;
};
static GearResult loadedGear;
static DWORD WINAPI loadWardrobeGear(void* parameter){
    const std::unique_ptr<GearTask> task(static_cast<GearTask*>(parameter));
    const auto& request=task->input;
    auto next=task->previous;
    next.partial=false;
    try{
        auto stock=std::make_shared<hex::WardrobeItemCatalog>();
        if(stock->loadStock(request.bin.parent_path(),request.manifest))next.stock=std::move(stock);
        else next.partial=true;
    }catch(const std::exception&){next.partial=true;}
    try{
        std::error_code error;
        const bool hasIndex=std::filesystem::exists(request.root/L"items.tsv",error);
        if(!hasIndex&&!error)next.local.reset();
        else{
            auto local=std::make_shared<hex::WardrobeItemCatalog>();
            if(local->load(request.root))next.local=std::move(local);
            else next.partial=true;
        }
    }catch(const std::exception&){next.partial=true;}
    try{
        auto combined=std::make_shared<hex::WardrobeItemCatalog>();
        if(next.stock)*combined=*next.stock;
        if(next.local)combined->supplement(*next.local);
        bool mounted=false;
        std::error_code error;
        if(std::filesystem::exists(request.manifest,error))
            combined->addMounted(request.bin,request.body,request.manifest,&mounted);
        else mounted=!error;
        if(!mounted){
            next.partial=true;
            if(next.combined)combined->retainMounted(*next.combined);
        }
        if(!combined->empty()||!next.partial)next.combined=std::move(combined);
    }catch(const std::exception&){next.partial=true;}
    loadedGear=std::move(next);
    gearState.store(GearState::Ready);
    return 0;
}
static void wardrobeGear(hex::Runtime& runtime,int witch,const hex::WardrobeEditorStatus& state){
    if(witch<0)return;
    const auto* addon=hex::characterAddon(witch);
    static const hex::WardrobeItemCatalog emptyCatalog;
    static GearResult equipment;
    static GearRequest requested,displayed,input;
    static std::filesystem::path mountLog;
    static uint64_t nextFileCheck=0;
    static bool requestMade=false,reloadQueued=false;
    static uint64_t retryAt=0;
    static unsigned failures=0;
    static std::string slot;
    static char filter[120]{};
    static int lastWitch=-1;
    if(lastWitch!=witch){slot.clear();filter[0]=0;lastWitch=witch;}
    const bool addonVenus=addon&&hex::addonVenusBodyActive(witch);
    const bool stockBody=vanillaBody||(addon&&!addonVenus);
    const std::string_view body=liveBody||(addon&&!addonVenus&&!vanillaBody)?std::string_view{}:stockBody?"vanilla":"venus";
    const auto root=runtime.profiles.parent_path()/L"WardrobeItems"/(stockBody?L"vanilla":L"venus");
    const auto dependencies=runtime.dependencyStatus();
    auto manifest=dependencies->manifest.path;
    if(manifest.empty())manifest=runtime.folder/L"dmm/manifest.tsv";
    ImGui::Spacing();
    ImGui::TextColored(gold,"EQUIPMENT");
    ImGui::SameLine();
    const bool reloadClicked=ImGui::SmallButton("Reload items");
    if(reloadClicked)reloadQueued=true;
    const auto now=GetTickCount64();
    if(now>=nextFileCheck||reloadClicked||input.bin!=runtime.folder||input.root!=root||
       input.manifest!=manifest||input.body!=body||mountLog!=dependencies->managerMountLog){
        const auto modifiedTime=[](const std::filesystem::path& path){
            std::error_code ec;
            return std::filesystem::last_write_time(path,ec);
        };
        mountLog=dependencies->managerMountLog;
        input={runtime.folder,root,manifest,modifiedTime(manifest),
            modifiedTime(manifest.parent_path()/L"mod_items.json"),modifiedTime(root/L"items.tsv"),
            modifiedTime(mountLog),modifiedTime(manifest.parent_path()/L"files/character/bin__/partprefabtable.pappt"),
            std::string(body)};
        nextFileCheck=now+1000;
    }
    const auto sameSource=[&](const GearRequest& previous){
        return previous.bin==input.bin&&previous.root==input.root&&previous.manifest==input.manifest&&previous.body==input.body;
    };
    const auto& draft=state.draftSlots;
    const auto scheduleRetry=[&]{
        ++failures;
        retryAt=now+std::min<uint64_t>(30000,2000ull<<std::min(failures,4u));
    };
    if(gearState.load()==GearState::Ready){
        if(sameSource(requested)){
            displayed=requested;
            equipment=std::move(loadedGear);
            if(equipment.partial)scheduleRetry();
            else{retryAt=0;failures=0;}
        }
        loadedGear={};
        gearState.store(GearState::Idle);
    }
    if(gearState.load()==GearState::Idle&&(!requestMade||!(requested==input)||reloadQueued||(retryAt&&now>=retryAt))){
        if(!sameSource(requested)){failures=0;retryAt=0;}
        requested=input;
        requestMade=true;
        reloadQueued=false;
        if(runtime.preview){
            auto catalog=std::make_shared<hex::WardrobeItemCatalog>();
            catalog->load(root);
            equipment.combined=std::move(catalog);
            displayed=input;
        }else{
            auto* task=new GearTask{input,sameSource(displayed)?equipment:GearResult{}};
            gearState.store(GearState::Loading);
            if(const auto thread=CreateThread(nullptr,0,loadWardrobeGear,task,0,nullptr))CloseHandle(thread);
            else{
                delete task;
                gearState.store(GearState::Idle);
                LOG_ERR("Wardrobe equipment scan could not start. Retrying.");
                scheduleRetry();
            }
        }
    }
    const bool current=sameSource(displayed)&&equipment.combined!=nullptr;
    const bool loading=gearState.load()!=GearState::Idle;
    const auto& catalog=current?*equipment.combined:emptyCatalog;
    const bool loaded=!catalog.empty();
    if(!ImGui::BeginTable("equip",2,ImGuiTableFlags_SizingStretchProp))return;
    ImGui::TableSetupColumn("Slots",ImGuiTableColumnFlags_WidthStretch,1.f);
    ImGui::TableSetupColumn("Items",ImGuiTableColumnFlags_WidthStretch,1.6f);
    ImGui::TableNextColumn();
    ImGui::BeginChild("equipment-slots",{0,250*scale},ImGuiChildFlags_Borders);
    constexpr const char* names[]={"Headwear","Upper body","Lower body","Hands","Feet","Cloak","Accessories","Shoulders","Waist","Armour"};
    if(ImGui::Selectable("All slots",slot.empty()))slot.clear();
    for(const auto* label:names){
        ImGui::PushID(label);
        const bool equipped=draft.count(label)!=0;
        const auto size=equipped?ImVec2{ImGui::GetContentRegionAvail().x-70*scale,0}:ImVec2{};
        if(ImGui::Selectable(label,slot==label,0,size))slot=label;
        if(equipped){
            ImGui::SameLine();
            if(ImGui::SmallButton("Unequip")){
                std::string error;
                if(!hex::unequipWardrobeSlot(witch,label,error))wardrobeGearNotice=std::move(error);
                else wardrobeGearNotice.clear();
            }
            record(std::string("unequip.")+label);
        }
        for(const auto& [origin,parts]:draft)for(const auto& part:parts){
            const auto* item=current?catalog.forPart(part):nullptr;
            const auto destination=item?item->slot:hex::outfitSlotForPart(part);
            if((destination.empty()?origin:destination)!=label)continue;
            wardrobeItemIcon(item);ImGui::SameLine();
            if(item)ImGui::TextWrapped("%s",item->name.c_str());
            else if(hex::defaultOutfitPart(witch,part.c_str()))ImGui::TextDisabled("Default %s",label);
            else {
                const auto named=addon?std::find_if(addon->items.begin(),addon->items.end(),[&](const auto& row){return row.prefab==part;}):
                    std::vector<hex::CharacterAddonItem>::const_iterator{};
                if(addon&&named!=addon->items.end())ImGui::TextWrapped("%s",named->label.c_str());
                else {ImGui::TextDisabled("Equipped part");tip(part.c_str());}
            }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::TableNextColumn();
    ImGui::BeginChild("equipment-items",{0,250*scale},ImGuiChildFlags_Borders);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##lookup","Item, mod, ID or tag",filter,sizeof filter);record("equipmentLookup");
    if(loading)ImGui::TextDisabled(loaded?"Refreshing equipment…":"Reading game equipment…");
    else if(retryAt)ImGui::TextWrapped(loaded?"Some equipment data could not be refreshed. Retrying.":"Equipment data could not be read. Retrying.");
    else if(!loaded)ImGui::TextWrapped("No equipment records were found.");
    static GearCatalog searchedCatalog;
    static std::string searchedFilter,searchedSlot;
    static std::vector<const hex::WardrobeItem*> matches;
    const auto activeCatalog=current?equipment.combined:GearCatalog{};
    if(searchedCatalog!=activeCatalog||searchedFilter!=filter||searchedSlot!=slot){
        matches=catalog.find(filter,slot,49);
        searchedCatalog=activeCatalog;
        searchedFilter=filter;
        searchedSlot=slot;
    }
    bool patchMatch=false;
    if(addon){
        const auto lower=[](std::string value){
            for(auto& letter:value)if(letter>='A'&&letter<='Z')letter=char(letter-'A'+'a');
            return value;
        };
        const auto query=lower(filter);
        for(const auto& item:addon->items){
            if((!slot.empty()&&slot!=item.slot)||(current&&catalog.forPart(item.prefab))||
               (lower(item.label).find(query)==std::string::npos&&lower(item.prefab).find(query)==std::string::npos&&
                lower(item.slot).find(query)==std::string::npos))continue;
            patchMatch=true;
            ImGui::PushID(item.prefab.c_str());
            wardrobeItemIcon(nullptr);ImGui::SameLine();
            ImGui::BeginDisabled(!state.editingSaved);
            if(ImGui::Selectable(item.label.c_str(),false,0,{0,36*scale})){
                std::string error;
                wardrobeGearNotice=hex::equipWardrobeItem(witch,item.slot,{item.prefab},error)?
                    "Queued "+item.slot+" for preview.":std::move(error);
            }
            ImGui::EndDisabled();
            tip((state.editingSaved?item.slot:wardrobeEditorNotice.empty()?state.detail:wardrobeEditorNotice).c_str());
            ImGui::PopID();
        }
    }
    if(loaded&&matches.empty()&&!patchMatch)ImGui::TextDisabled("No matching equipment");
    size_t shown=0;
    for(const auto* item:matches){
        if(shown++==48){
            ImGui::TextDisabled("Refine your search to see more items.");
            break;
        }
        ImGui::PushID(int(item->id));
        ImGui::PushID(item->slot.c_str());
        ImGui::PushID(item->parts.begin()->c_str());
        wardrobeItemIcon(item);ImGui::SameLine();
        ImGui::BeginDisabled(!state.editingSaved);
        const auto label=item->id||item->source.empty()?item->name:item->source+" — "+item->name;
        if(ImGui::Selectable(label.c_str(),false,0,{0,36*scale})){
            std::string error;
            wardrobeGearNotice=hex::equipWardrobeItem(witch,item->slot,item->parts,error)?"Queued "+item->slot+" for preview.":std::move(error);
        }
        ImGui::EndDisabled();
        if(!state.editingSaved)tip((wardrobeEditorNotice.empty()?state.detail:wardrobeEditorNotice).c_str());
        else if(ImGui::BeginItemTooltip()){
            ImGui::TextUnformatted(item->slot.c_str());
            if(item->id){ImGui::SameLine();ImGui::Text("#%u",item->id);}
            if(!item->source.empty())ImGui::TextUnformatted(item->source.c_str());
            if(item->fitUnverified)ImGui::TextDisabled("Fit unverified for this body");
            ImGui::EndTooltip();
        }
        ImGui::PopID();
        ImGui::PopID();
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::EndTable();
}
static void wardrobeControls(hex::Runtime& runtime){
    ImGui::TextColored(gold,"OUTFIT");outfitControl();
    const auto witch=wardrobeTarget(runtime);
    const auto status=hex::wardrobeEditorStatus(witch);
    const auto ready=witch>=0&&!status.spouseCombatActive;
    ImGui::BeginDisabled(!ready);
    if(ImGui::SmallButton("Edit [HA] Wardrobe"))enterWardrobeEditor(runtime,witch,true);
    record("editWardrobe");
    ImGui::EndDisabled();
    if(witch<0)tip("Select one character.");
    else if(status.spouseCombatActive)tip("Spouse Overhaul owns the active Combat Gear profile.");
    wardrobeSlotsControl(runtime);providerWardrobeControls(runtime);equippedItemControls(runtime,false);
    ImGui::Spacing();ImGui::Separator();ImGui::Spacing();ImGui::TextColored(gold,"OUTFIT VISIBILITY");outfitHiddenControl(runtime);
    ImGui::Spacing();ImGui::Separator();ImGui::Spacing();ImGui::TextColored(gold,"BASE UNDERWEAR");
    underwearControl(runtime);ImGui::TextDisabled("Hides base underwear and linked outfit parts.");
}
static void updatePanelBounds(ml::State& state){
    const auto position=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
    state.panelLeft=position.x;state.panelTop=position.y;state.panelRight=position.x+size.x;state.panelBottom=position.y+size.y;
}
static void selectCharacter(hex::Runtime& runtime,int target){
    savePendingAddonShape(runtime);
    wardrobeSelectionNotice.clear();
    if(addonCharacterTarget>=0&&target!=addonCharacterTarget)
        hex::cancelAddonWardrobe(addonCharacterTarget);
    if(target<hex::firstAddonCharacter){
        addonCharacterTarget=-1;
        runtime.model.selectTarget(target);
    }else{
        addonCharacterTarget=target;
        addonDraft=hex::addonBodyProfile(target).profile;
    }
}
static void characterPatchControls(hex::Runtime& runtime){
    const bool expanded=ImGui::CollapsingHeader("Character patches");record("characterPatches");
    if(!expanded)return;
    const auto& packs=hex::characterAddonPacks();
    if(packs.empty()){ImGui::TextDisabled("No character patches detected.");return;}
    std::string notice;
    for(size_t i=0;i<packs.size();++i){
        const auto& pack=packs[i];
        if(i)ImGui::Separator();
        ImGui::PushID(pack.id.c_str());
        bool enabled=hex::characterAddonPackEnabled(i);
        ImGui::BeginDisabled(!pack.accepted);
        if(ImGui::Checkbox(pack.label.c_str(),&enabled)){
            if(!hex::setCharacterAddonPackEnabled(i,enabled,notice))runtime.model.message=notice;
            else if(addonCharacterTarget>=0&&!hex::characterAddonEnabled(addonCharacterTarget))
                selectCharacter(runtime,-1);
        }
        record("patch."+pack.id);
        ImGui::EndDisabled();
        if(!pack.accepted)tip(pack.detail.c_str());
        ImGui::Indent();
        ImGui::TextColored(enabled?green:muted,"%s   |   %s   |   %zu/%zu characters",
            pack.version.empty()?"Version undeclared":("v"+pack.version).c_str(),
            enabled?"Enabled":pack.accepted?"Disabled":"Unavailable",pack.accepted,pack.declared);
        ImGui::TextWrapped("%s",pack.detail.c_str());
        if(enabled)for(const int target:hex::enabledAddonTargets()){
            const auto* addon=hex::characterAddon(target);
            if(addon->pack!=i)continue;
            const int owner=hex::addonGroupOwner(target);
            // Records that share settings report through their main entry.
            if(owner!=target&&hex::addonGroupShared(owner))continue;
            const auto state=hex::addonWardrobeStatus(target);
            ImGui::TextDisabled("%s",addon->label.c_str());
            ImGui::SameLine();
            ImGui::TextWrapped("%s",state.detail.c_str());
        }
        ImGui::Unindent();
        ImGui::PopID();
    }
    if(!notice.empty())ImGui::TextColored(errorColour,"%s",notice.c_str());
}
static void renderWardrobeEditor(hex::Runtime& runtime,float maxWidth,float maxHeight){
    const auto target=addonEditorTarget>=0?addonEditorTarget:runtime.model.target;
    const auto* addon=hex::characterAddon(target);
    auto& state=State::Get();
    auto& io=ImGui::GetIO();
    static float previousScale=0.f;
    if(previousScale!=scale){
        ImGui::SetNextWindowSize({std::min(560.f*scale,maxWidth),std::min(560.f*scale,maxHeight)},ImGuiCond_Always);
        previousScale=scale;
    }
    ImGui::SetNextWindowSizeConstraints({std::min(340.f*scale,maxWidth),std::min(280.f*scale,maxHeight)},{maxWidth,maxHeight});
    ImGui::SetNextWindowPos({24*scale,24*scale},ImGuiCond_FirstUseEver);
    bool open=true;
    const bool draw=ImGui::Begin("[HA] Wardrobe##spouse",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse);
    updatePanelBounds(state);
    if(draw){
        ImGui::PushFont(titleFont);ImGui::TextUnformatted("[HA] Wardrobe");ImGui::PopFont();
        ImGui::SameLine(ImGui::GetWindowWidth()-45*scale);
        if(ImGui::SmallButton("X"))open=false;
        record("closeSpouseWardrobe");
        ImGui::TextColored(accent,"%s",addon?addon->label.c_str():hex::witches[target].name);
        ImGui::Separator();
        auto status=hex::wardrobeEditorStatus(target);
        if(!status.editingSaved&&GetTickCount64()>=nextWardrobeEditRetryAt){
            tryStartWardrobeEdit(runtime,target);
            status=hex::wardrobeEditorStatus(target);
        }
        const auto& editorStatus=wardrobeEditorNotice.empty()?status.caption:wardrobeEditorNotice;
        if(!editorStatus.empty())ImGui::TextWrapped("%s",editorStatus.c_str());
        wardrobeGear(runtime,target,status);
        equippedItemControls(runtime,true,target);
        ImGui::Spacing();
        ImGui::BeginDisabled(!status.editingSaved);
        if(ImGui::Button("Apply and save")){
            std::string error;
            if(hex::applySavedWardrobe(target,error))open=false;
            else wardrobeEditorNotice=std::move(error);
        }
        record("applySpouseWardrobe");
        ImGui::EndDisabled();
        if(!status.editingSaved)tip(editorStatus.c_str());
        ImGui::SameLine();
        if(ImGui::Button("Cancel"))open=false;
        ImGui::SameLine();
        ImGui::BeginDisabled(!status.editingSaved||status.draftSlots.empty());
        if(ImGui::Button("Clear all slots"))ImGui::OpenPopup("Clear all slots?");
        ImGui::EndDisabled();
        if(!status.editingSaved)tip(editorStatus.c_str());
        else if(status.draftSlots.empty())tip("Every slot in this preview is already empty.");
        if(!wardrobeGearNotice.empty())ImGui::TextWrapped("%s",wardrobeGearNotice.c_str());
        const bool confirmingClear=ImGui::IsPopupOpen("Clear all slots?");
        const bool escape=ImGui::IsKeyPressed(ImGuiKey_Escape,false);
        if(ImGui::BeginPopupModal("Clear all slots?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextUnformatted("Clear every slot in this wardrobe preview?");
            if(ImGui::Button("Clear slots")){
                std::string error;
                if(!hex::clearSavedWardrobeDraft(target,error))wardrobeEditorNotice=std::move(error);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Keep outfit")||escape)ImGui::CloseCurrentPopup();
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
        if(escape&&!confirmingClear&&!io.WantTextInput)open=false;
    }
    state.mouseActive=ImGui::IsAnyItemActive()||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId);
    state.textEditing=io.WantTextInput;
    ImGui::End();
    if(!open){
        if(wardrobeEditorFromTab)finishWardrobeEditor(runtime);
        else setOpen(false);
    }
}
void Render(const hex::GraphicsReportSource& graphics){
    auto& r=hex::Runtime::get();r.tickAuto();auto& model=r.model;auto& state=State::Get();state.lastDrawAt=GetTickCount();state.blockGameInput=r.blockGameInput;
    const auto dependency=r.dependencyStatus();
    const auto& dependencies=*dependency;
    vanillaBody=dependencies.bodyMode==hex::BodyMode::Vanilla;liveBody=dependencies.bodyMode==hex::BodyMode::Live;customPreset=dependencies.customPreset;baseVersionWarning.clear();hexVersionWarning.clear();underwearVersionWarning.clear();
    const auto appendWarning=[](std::string& out,const hex::DependencyFile& file){if(file.valid&&!file.verified){if(!out.empty())out+="\n\n";out+=file.detail;}};
    for(const auto* file:{&dependencies.game,&dependencies.instant,&dependencies.venus,&dependencies.physics})appendWarning(baseVersionWarning,*file);
    if(!dependencies.bodyWarning.empty()){if(!baseVersionWarning.empty())baseVersionWarning+="\n\n";baseVersionWarning+=dependencies.bodyWarning;}
    // A witch's own body or appearance replacement marks only her controls (and ALL CHARACTERS,
    // which edits her too). Add-on characters share only the checks that apply to everyone.
    if(addonCharacterTarget<0){
        const auto addText=[](std::string& out,const std::string& text){if(text.empty())return;if(!out.empty())out+="\n\n";out+=text;};
        const auto& own=dependencies.witchBodyWarnings;
        if(model.target>=0&&model.target<int(own.size()))addText(baseVersionWarning,own[model.target]);
        else if(model.target<0)for(size_t i=0;i<own.size();++i)
            if(!own[i].empty()&&!(dependencies.unavailableBodyWitches&(1u<<i)))addText(baseVersionWarning,std::string(hex::witches[i].name)+": "+own[i]);
    }
    if(!customPreset)appendWarning(hexVersionWarning,dependencies.hex);
    appendWarning(underwearVersionWarning,dependencies.creator);
    const bool shapingFiles=dependencies.scanned?dependencies.game.valid&&dependencies.instant.valid&&dependencies.canShapeBody():r.shapingAvailable.load();
    const auto shaping=r.featureCompatibility(hex::Feature::Shaping);
    const auto hexPreset=r.featureCompatibility(hex::Feature::Hex);
    const auto weight=r.featureCompatibility(hex::Feature::Weight);
    const bool nativeShapingReady=shaping.allows(shapingFiles);
    const bool hexReady=hexPreset.allows(dependencies.scanned?dependencies.hex.valid:r.hexAvailable.load());
    const auto selectedShapeMask=[&]{return (model.target<0?63u:1u<<model.target)&~dependencies.unavailableBodyWitches;};
    const auto dependencyReason=[](const hex::DependencyFile& file){
        return file.detail+(file.path.empty()?std::string{}:"\nFile: "+file.path.u8string());
    };
    shapeBlockReason=shaping.detail;
    if(dependencies.scanned){
        if(!dependencies.game.valid)shapeBlockReason=dependencyReason(dependencies.game);
        else if(!dependencies.instant.valid)shapeBlockReason=dependencyReason(dependencies.instant);
        else if(!dependencies.canShapeBody()&&!dependencies.bodyDetail.empty())shapeBlockReason=dependencies.bodyDetail;
    }
    if(addonCharacterTarget<0&&!selectedShapeMask()){
        if(model.target>=0&&!dependencies.unavailableBodyDetails[model.target].empty())
            shapeBlockReason=dependencies.unavailableBodyDetails[model.target];
        else if(model.target<0){
            for(size_t i=0;i<dependencies.unavailableBodyDetails.size();++i)if(!dependencies.unavailableBodyDetails[i].empty()){
                shapeBlockReason+="\n\n"+std::string(hex::witches[i].name)+": "+dependencies.unavailableBodyDetails[i];
                break;
            }
        }
    }
    if(shapeBlockReason.empty())shapeBlockReason="The native body pose route has not been confirmed.";
    hexBlockReason=dependencies.scanned&&!dependencies.hex.valid?dependencyReason(dependencies.hex):hexPreset.detail;
    if(hexBlockReason.empty())hexBlockReason="The selected HEX preset has not been loaded.";
    weightBlockReason=weight.detail.empty()?"The native Weight controls have not been confirmed.":weight.detail;
    const auto updateShapeControls=[&]{
        const bool ready=nativeShapingReady&&selectedShapeMask()!=0;
        weightReady=r.featureCompatibility(hex::Feature::Weight).allows(ready);
        return ready;
    };
    auto& io=ImGui::GetIO();io.MouseDrawCursor=true;const float maxW=std::max(300.f,io.DisplaySize.x-24),maxH=std::max(200.f,io.DisplaySize.y-24);
    if(spouseWardrobeOpen){
        renderWardrobeEditor(r,maxW,maxH);return;
    }
    static bool previous=false;static float previousScale=0.f;static uint32_t previousPins=0;static size_t previousProviderPins=0;
    float quickHeight=190;for(uint32_t bit=1;bit<=hex::PinReset;bit<<=1)if(r.quickPins&bit)quickHeight+=bit==hex::PinPad?260:bit==hex::PinTop||bit==hex::PinBottom?40:bit==hex::PinOutfit?65:35;
    if(r.quickPins&hex::PinUnderwear)quickHeight+=42;
    size_t providerPins=0;const auto outfitId=hex::wardrobeOutfitId(hex::wardrobeEditorStatus(wardrobeTarget(r)).selected);for(const auto& provider:hex::wardrobeProviderOutfits())if(provider.id==outfitId)for(const auto& control:provider.controls)if(hex::wardrobeControlPinned(provider.id,control.id))++providerPins;quickHeight+=providerPins*65;
    if(previousScale!=scale||previous!=r.compact||(r.compact&&(previousPins!=r.quickPins||previousProviderPins!=providerPins))){
        ImGui::SetNextWindowSize({std::min((r.compact?520.f:810.f)*scale,maxW),std::min((r.compact?quickHeight:780.f)*scale,maxH)},ImGuiCond_Always);
        previousScale=scale;previous=r.compact;previousPins=r.quickPins;previousProviderPins=providerPins;
    }
    ImGui::SetNextWindowSizeConstraints({std::min(400*scale,maxW),std::min((r.compact?220.f:330.f)*scale,maxH)},{maxW,maxH});ImGui::SetNextWindowPos({24*scale,24*scale},ImGuiCond_FirstUseEver);
    bool open=true;ImGui::Begin("Hex Appeal",&open,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar);
    updatePanelBounds(state);const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
    auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled({pos.x+16*scale,pos.y+10*scale},{pos.x+size.x-16*scale,pos.y+13*scale},ImGui::ColorConvertFloat4ToU32(accent),2*scale);
    ImGui::PushFont(titleFont);ImGui::TextUnformatted("HEX APPEAL");ImGui::PopFont();
    const float titleWidth=ImGui::GetItemRectSize().x,toolbarWidth=ImGui::CalcTextSize("Block game input").x+ImGui::GetFrameHeight()+ImGui::CalcTextSize("Minimise").x+70*scale;
    if(size.x>titleWidth+toolbarWidth+55*scale)ImGui::SameLine(size.x-toolbarWidth-18*scale);
    if(ImGui::Checkbox("Block game input",&r.blockGameInput)){state.blockGameInput=r.blockGameInput;input::InputModeChanged();r.saveInterface();}record("blockInput");tip("On: block gameplay input while this panel is visible. Off: allow gameplay outside panel gestures and text entry. Saved automatically.");
    ImGui::SameLine();if(ImGui::SmallButton(r.compact?"Restore":"Minimise")){r.compact=!r.compact;r.saveInterface();}record("compact");ImGui::SameLine();if(ImGui::SmallButton("X"))open=false;record("close");
    ImGui::Separator();bool changed=false,addonChanged=false;
    // A previous log without game graphics is one clue for startup troubleshooting.
    static bool startupNoteDismissed=false;
    if(!startupNoteDismissed&&ml::Log::PreviousStartupIncomplete()&&GetModuleHandleW(L"GameOverlayRenderer64.dll")){
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(gold,"Hex Appeal did not detect game graphics in the previous log. If Crimson Desert keeps closing at startup, try turning off \"Enable the Steam Overlay while in-game\" for this game in Steam.");
        ImGui::PopTextWrapPos();
        if(ImGui::SmallButton("Dismiss"))startupNoteDismissed=true;record("startupNote");
        ImGui::Separator();
    }
    if(addonCharacterTarget>=0&&!hex::characterAddonEnabled(addonCharacterTarget))selectCharacter(r,-1);
    const auto addons=hex::enabledAddonSettingsTargets();
    const auto roster=hex::rosterAddonTargets();
    const auto addonShaping=[&](bool quick){
        const auto pins=quick?r.quickPins:~uint32_t{};
        const bool ready=nativeShapingReady;
        weightReady=r.featureCompatibility(hex::Feature::Weight).allows(ready);
        if(!quick)ImGui::Spacing();
        if(pins&hex::PinTop)addonChanged|=region("TOP",addonDraft.top,addonDraft.topBlend,ready,hexReady);
        if(pins&hex::PinBottom)addonChanged|=region("BOTTOM",addonDraft.bottom,addonDraft.bottomBlend,ready,hexReady);
        if(pins&hex::PinPad)addonChanged|=shapePad(addonDraft,quick?250*scale:ImGui::GetContentRegionAvail().y-140*scale,ready,hexReady);
        addonChanged|=proportions(addonDraft,ready,(pins&hex::PinHeight)!=0,(pins&hex::PinWeight)!=0);
        addonChanged|=profileActions(r,addonDraft,pins,ready);
        const auto detail=hex::addonShapeDetail(addonCharacterTarget);
        if(!quick)ImGui::TextWrapped("%s",!ready?"Waiting for the native shaping dependencies.":detail.empty()?"Using the character's original shape.":detail.c_str());
    };
    const auto pickCharacters=[&](bool compact){
        const int selected=addonCharacterTarget>=0?hex::addonGroupOwner(addonCharacterTarget):model.target;
        const auto pick=[&](const char* name,int target,int rowId){
            ImGui::PushID(rowId);
            if(ImGui::Selectable(name,selected==target,0,{0,compact?0.f:33*scale}))selectCharacter(r,target);
            record("target"+std::to_string(rowId));
            if(target>=hex::firstAddonCharacter)tip(name);
            ImGui::PopID();
        };
        pick("ALL CHARACTERS",-1,-1);
        ImGui::Separator();
        for(size_t i=0;i<hex::witches.size();++i)pick(hex::witches[i].name,int(i),int(i));
        if(!roster.empty())ImGui::Separator();
        const auto& packs=hex::characterAddonPacks();
        const auto* selectedAddon=hex::characterAddon(selected);
        std::array<bool,hex::characterCapacity> listedPacks{};
        for(const int target:roster){
            const auto& addon=*hex::characterAddon(target);
            const auto& pack=packs[addon.pack];
            if(compact||!pack.multiCharacter){
                pick(addon.label.c_str(),target,target);
                continue;
            }
            if(listedPacks[addon.pack])continue;
            listedPacks[addon.pack]=true;
            std::string_view label=pack.label;
            constexpr std::string_view prefix="Hex Appeal - ";
            if(label.size()>prefix.size()&&label.substr(0,prefix.size())==prefix)label.remove_prefix(prefix.size());
            const bool active=selectedAddon&&selectedAddon->pack==addon.pack;
            pick(label.data(),active?selected:target,target);
        }
    };
    const auto characterHeader=[&]{
        const int owner=hex::addonGroupOwner(addonCharacterTarget);
        const auto members=owner>=0?hex::addonGroupTargets(owner):std::vector<int>{};
        const bool hasVariants=members.size()>1;
        bool shared=hasVariants&&hex::addonGroupShared(owner);
        if(shared&&addonCharacterTarget!=owner)selectCharacter(r,owner);
        const auto* addon=hex::characterAddon(addonCharacterTarget);
        const auto samePack=[&](int target){return addon&&hex::characterAddon(target)->pack==addon->pack;};
        const bool packPicker=!r.compact&&addon&&hex::characterAddonPacks()[addon->pack].multiCharacter&&
            std::any_of(roster.begin(),roster.end(),[&](int target){return target!=owner&&samePack(target);});
        const char* name=addon?addon->label.c_str():model.target<0?"ALL CHARACTERS":hex::witches[model.target].name;
        constexpr const char* combineLabel="Combine instances";
        const auto& style=ImGui::GetStyle();
        const float combineWidth=ImGui::GetFrameHeight()+style.ItemInnerSpacing.x+ImGui::CalcTextSize(combineLabel).x;
        const float available=ImGui::GetContentRegionAvail().x;
        const float pickerWidth=hasVariants?std::max(1.f,available-combineWidth-style.ItemSpacing.x):available;
        const float combineX=ImGui::GetCursorPosX()+pickerWidth+style.ItemSpacing.x;
        if(r.compact||packPicker||(hasVariants&&!shared)){
            ImGui::SetNextItemWidth(pickerWidth);
            const bool picking=ImGui::BeginCombo("##character",name);
            record(r.compact?"quickWitch":"addonRecord");
            if(picking){
                if(r.compact)pickCharacters(true);
                if(packPicker){
                    ImGui::PushID("packCharacters");
                    for(const int target:roster){
                        if(!samePack(target))continue;
                        ImGui::PushID(target);
                        if(ImGui::Selectable(hex::characterAddon(target)->label.c_str(),target==owner)&&target!=owner)
                            selectCharacter(r,target);
                        record("packNpc"+std::to_string(target));
                        ImGui::PopID();
                    }
                    ImGui::PopID();
                }
                if(hasVariants&&!shared){
                    if(r.compact)ImGui::SeparatorText("Instances");
                    for(const int member:members){
                        ImGui::PushID(member);
                        if(ImGui::Selectable(hex::characterAddon(member)->label.c_str(),member==addonCharacterTarget)&&member!=addonCharacterTarget)
                            selectCharacter(r,member);
                        record("instance"+std::to_string(member));
                        ImGui::PopID();
                    }
                }
                ImGui::EndCombo();
            }
        }else{
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(name);
            record("characterName");
        }
#ifdef HEX_MENU_TESTS
        record("selectedAddon"+std::to_string(addonCharacterTarget));
#endif
        if(hasVariants){
            ImGui::SameLine(combineX);
            if(ImGui::Checkbox(combineLabel,&shared)){
                std::string issue;
                if(hex::setAddonSettingsShared(owner,shared,issue))selectCharacter(r,owner);
                else model.message=issue;
            }
            record("shareVariants."+hex::characterAddon(owner)->id);
            tip("Share Shape and Wardrobe settings across this character's instances. Untick to choose and edit each instance separately.");
        }
    };
    ImGui::BeginChild("workspace",{0,-37*scale},ImGuiChildFlags_None);
    if(!r.compact){
        ImGui::BeginChild("witches",{125*scale,0},ImGuiChildFlags_None);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,{.5f,.5f});
        pickCharacters(false);ImGui::PopStyleVar();ImGui::EndChild();ImGui::SameLine();
    }
    ImGui::BeginChild("editor",{0,0},ImGuiChildFlags_None);
    characterHeader();
    if(r.compact){
        if(addonCharacterTarget>=0){addonShaping(true);}
        else {
        const bool shapingReady=updateShapeControls();
        bool mixed=false;if(model.target<0)for(const auto& profile:model.saved)if(!(profile==model.saved[0]))mixed=true;
        if(model.target<0)for(int target:addons)if(!(hex::addonBodyProfile(target).profile==model.saved[0]))mixed=true;
        if(mixed)ImGui::TextColored(gold,"Mixed profiles");else ImGui::TextColored(gold,"%s %.0f%%  /  %s %.0f%%",name(model.draft.top),100*model.draft.topBlend,name(model.draft.bottom),100*model.draft.bottomBlend);
        if(r.quickPins&hex::PinTop)changed|=region("TOP",model.draft.top,model.draft.topBlend,shapingReady,hexReady);
        if(r.quickPins&hex::PinBottom)changed|=region("BOTTOM",model.draft.bottom,model.draft.bottomBlend,shapingReady,hexReady);
        if(r.quickPins&hex::PinPad)changed|=shapePad(model.draft,250*scale,shapingReady,hexReady);
        changed|=proportions(model.draft,shapingReady,(r.quickPins&hex::PinHeight)!=0,(r.quickPins&hex::PinWeight)!=0);
        changed|=profileActions(r,model.draft,r.quickPins,selectedShapeMask()!=0);
        }
        if(r.quickPins&hex::PinUnderwear)underwearControl(r);
        if(r.quickPins&hex::PinHideOutfit)outfitHiddenControl(r);
        if(r.quickPins&hex::PinOutfit)outfitControl();
        providerWardrobeControls(r,true);
    }
    else if(ImGui::BeginTabBar("pages")){
        const bool shape=ImGui::BeginTabItem("Shape");record("tabShape");
        if(shape){
            if(addonCharacterTarget>=0)addonShaping(false);
            else {
            const bool shapingReady=updateShapeControls();
            ImGui::Spacing();changed|=region("TOP",model.draft.top,model.draft.topBlend,shapingReady,hexReady);changed|=region("BOTTOM",model.draft.bottom,model.draft.bottomBlend,shapingReady,hexReady);ImGui::Spacing();
            changed|=shapePad(model.draft,ImGui::GetContentRegionAvail().y-108*scale,shapingReady,hexReady);ImGui::Spacing();
            changed|=proportions(model.draft,shapingReady,true,true);
            changed|=profileActions(r,model.draft,hex::PinCopy|hex::PinPaste|hex::PinReset,selectedShapeMask()!=0);
            }
            ImGui::EndTabItem();
        }
        const bool wardrobe=ImGui::BeginTabItem("Wardrobe");record("tabWardrobe");
        if(wardrobe){
            ImGui::Spacing();ImGui::PushFont(titleFont);ImGui::TextUnformatted("WARDROBE");ImGui::PopFont();ImGui::Spacing();
            wardrobeControls(r);
            ImGui::EndTabItem();
        }
        const bool settings=ImGui::BeginTabItem("Settings");record("tabInterface");
        if(settings){
            ImGui::Spacing();const int keys[]={VK_F9,VK_F8,VK_F10,VK_F11,VK_INSERT};const char* labels[]={"F9","F8","F10","F11","Insert"};int selected=0;for(int i=0;i<5;++i)if(r.key==keys[i])selected=i;
            ImGui::SetNextItemWidth(150*scale);if(ImGui::Combo("Shortcut",&selected,labels,5)){r.key=keys[selected];r.control=r.alt=r.shift=false;r.saveInterface();}record("hotkey");tip("Choose another key if a different mod uses this shortcut.");
            static int scaleDraft=100;
            static bool editingScale=false;
            if(!editingScale)scaleDraft=r.uiScalePercent;
            ImGui::SetNextItemWidth(200*scale);
            ImGui::SliderInt("Scale",&scaleDraft,hex::minimumUiScalePercent,hex::maximumUiScalePercent,"%d%%",ImGuiSliderFlags_AlwaysClamp);
            editingScale=ImGui::IsItemActive();
            if(ImGui::IsItemDeactivatedAfterEdit()&&r.uiScalePercent!=scaleDraft){
                r.uiScalePercent=scaleDraft;
                r.saveInterface();
            }
            record("scale");
            tip("Adjust text and panel size. The new size applies when you release the slider.");
            const bool rangeAvailable=r.preview||hex::outfitRangeAvailable();
            int range=r.outfitRangeMeters.load();
            ImGui::BeginDisabled(!rangeAvailable);
            ImGui::SetNextItemWidth(200*scale);
            if(ImGui::SliderInt("Outfit update range",&range,0,hex::maximumOutfitRange,range?"%d m":"Game default",
                ImGuiSliderFlags_AlwaysClamp))r.outfitRangeMeters=hex::clampOutfitRange(range);
            if(ImGui::IsItemDeactivatedAfterEdit())r.saveInterface();
            record("outfitRange");
            ImGui::EndDisabled();
            tip(rangeAvailable?"Applies to outfit assembly and visibility, including base underwear. Higher values can update more loaded characters. 0 uses the original game-driven updates.":
                "The loaded Crimson Desert build has no checked loaded-actor scan. Outfit updates still follow the game's normal actor updates.");
            characterPatchControls(r);
            pinSettings(r);
            skinSettings(r);
            dependencySettings(r);
            if(ImGui::CollapsingHeader("Diagnostics")){ImGui::TextWrapped("%s",r.native.shapingDetail.c_str());ImGui::TextWrapped("%s",model.message.c_str());const auto probe=hex::wardrobeProbeStatus();if(probe.ready){ImGui::BeginDisabled(probe.pending);if(ImGui::Button("Refresh selected outfit")){std::string error;if(!hex::requestWardrobeRefresh(model.target,error))model.message=error;}ImGui::EndDisabled();ImGui::TextWrapped("%s",probe.detail.c_str());}}
            ImGui::EndTabItem();
        }
        const bool log=ImGui::BeginTabItem("Log");record("tabLog");
        if(log){logTab(r,graphics);ImGui::EndTabItem();}
        ImGui::EndTabBar();
    }
    ImGui::EndChild();ImGui::EndChild();
    if(addonChanged||(changed&&model.target<0&&!addons.empty())){
        pendingAddonTarget=addonChanged?addonCharacterTarget:-1;
        pendingAddonProfile=addonChanged?addonDraft:model.draft;
    }
    if(!io.MouseDown[0])savePendingAddonShape(r);
    model.draft.autoFit=true;if(changed)r.selectionChanged();r.tickAuto(GetTickCount64(),!io.MouseDown[0]);
    ImGui::Separator();model.applied=model.draftIsApplied(~dependencies.unavailableBodyWitches);
    auto dependencyIssue=hex::profileDependencyIssue(model.draft,r.shapingAvailable,r.hexAvailable,r.weightAvailable);bool blocked=false;
    for(const auto feature:{hex::Feature::Shaping,hex::Feature::Hex,hex::Feature::Weight}){if(feature==hex::Feature::Hex&&!(model.draft.top==hex::Preset::Hex&&model.draft.topBlend>0)&&!(model.draft.bottom==hex::Preset::Hex&&model.draft.bottomBlend>0))continue;if(feature==hex::Feature::Weight&&model.draft.weight==0)continue;const auto compatibility=r.featureCompatibility(feature);if(compatibility.state==hex::CompatibilityState::Blocked){dependencyIssue=compatibility.detail;blocked=true;break;}}
    const bool limited=model.target<0&&dependencies.unavailableBodyWitches;
    if(!selectedShapeMask()){blocked=true;dependencyIssue=shapeBlockReason;}
    else if(blocked&&shaping.state==hex::CompatibilityState::Blocked)dependencyIssue=shapeBlockReason;
    if(limited&&dependencyIssue.empty())dependencyIssue=dependencies.bodySkipped.empty()?dependencies.bodyWarning:dependencies.bodySkipped;
    const bool waiting=(r.queuedMask&selectedShapeMask())||model.pendingTransaction;
    const auto* shapeStatus=r.savePending?"Shape / saving...":blocked?"Shape / blocked  (!)":waiting?"Shape / waiting":
        limited?"Shape / available witches only  (!)":!dependencyIssue.empty()?"Shape / dependency needed":model.applied?"Shape / active":"Shape / saved";
    const bool addonPage=addonCharacterTarget>=0;
    ImGui::TextColored(blocked&&!addonPage?errorColour:r.autoError||limited?gold:waiting||!dependencyIssue.empty()?muted:green,
        "%s",addonPage?hex::characterAddon(addonCharacterTarget)->label.c_str():shapeStatus);
    char shortcut[32]{};GetKeyNameTextA(LONG(MapVirtualKeyA(UINT(r.key),MAPVK_VK_TO_VSC)<<16),shortcut,32);
    if(!addonPage){tip(dependencyIssue.empty()?model.message.c_str():dependencyIssue.c_str());ImGui::SameLine();ImGui::TextDisabled("AUTO");}
    ImGui::SameLine(ImGui::GetWindowWidth()-82*scale);ImGui::TextDisabled("%s  |  ?",shortcut[0]?shortcut:"Menu");
    tip("Changes apply and save automatically. Right-click a control to pin it. Use Block game input to choose whether gameplay receives input while open.");
    state.mouseActive=ImGui::IsAnyItemActive()||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId);state.textEditing=io.WantTextInput;
    ImGui::End();if(!open)setOpen(false);
}
}
