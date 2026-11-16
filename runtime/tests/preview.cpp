#include "runtime.h"
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
static void frame(){hex::test::controls.clear();ImGui::NewFrame();ml::gui::Render();ImGui::Render();}
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
int main(int argc,char** argv){try{
    if(argc!=3&&argc!=6)throw std::runtime_error("preview output, profile output, optional width height page required");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize=argc==6?ImVec2(float(atoi(argv[3])),float(atoi(argv[4]))):ImVec2(1200,1000);io.DeltaTime=1.f/60.f;io.IniFilename=nullptr;
    auto& r=hex::Runtime::get();r.preview=true;r.shapingAvailable=true;r.hexAvailable=true;r.profiles=argv[2];r.native.detail="Offline preview: no game or editor is loaded.";
    ml::State::Get().menuOpen=true;ml::gui::InitStyle(1.f);unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);io.Fonts->SetTexID((ImTextureID)1);
    for(int i=0;i<4;++i)frame();
    if(argc==6){const std::string page=argv[5];if(page=="wardrobe")click("tabWardrobe");else if(page=="interface")click("tabInterface");else if(page=="compact")click("compact");io.AddMousePosEvent(io.DisplaySize.x-10,10);frame();frame();output(argv[1]);ImGui::DestroyContext();return 0;}
    const float points[]={.5f,.001f,.999f};
    for(int t=0;t<3;++t)for(int b=0;b<3;++b){pad(points[b],points[t]);if(int(r.model.draft.top)!=t||int(r.model.draft.bottom)!=b)throw std::runtime_error("pad cannot reach combination "+std::to_string(t)+"/"+std::to_string(b)+"; got "+std::to_string(int(r.model.draft.top))+"/"+std::to_string(int(r.model.draft.bottom)));}
    pad(.72f,.29f);const auto priorTop=r.model.draft.topBlend;click("BOTTOM.preset1");
    if(std::fabs(r.model.draft.topBlend-priorTop)>.00001f||r.model.draft.bottom!=hex::Preset::Hex||r.model.draft.bottomBlend!=1)throw std::runtime_error("bottom selector changed the prior top blend");
    const auto beforeHeight=r.model.draft;click("height",.7f);if(r.model.draft.height<=0||r.model.draft.topBlend!=beforeHeight.topBlend||r.model.draft.bottom!=beforeHeight.bottom)throw std::runtime_error("height slider changed a regional selection");
    const auto beforeWeight=r.model.draft;click("weight",.9f);if(!(r.model.draft==beforeWeight))throw std::runtime_error("weight control changed without its optional reference");
    if(r.model.applied)throw std::runtime_error("unavailable auto application falsely acknowledged");
    hex::Model read;std::string error;if(!read.load(r.profiles,error)||!(read.saved[0]==r.model.draft))throw std::runtime_error("automatic menu save failed");
    click("copy");const auto copied=r.model.draft;click("target0");click("paste");if(!(r.model.draft==copied))throw std::runtime_error("cross-witch paste failed");
    pad(.54f,.305f);const auto prior=r.model.draft;click("target3");click("target0");if(!(r.model.draft==prior))throw std::runtime_error("witch navigation discarded the latest pad edit");
    click("reset");if(int(r.model.draft.top)||int(r.model.draft.bottom)||r.model.draft.topBlend!=1||r.model.draft.bottomBlend!=1)throw std::runtime_error("baseline reset failed");
    hex::DependencyStatus unverified;unverified.scanned=true;unverified.bodyVerified=true;for(auto* file:{&unverified.game,&unverified.instant,&unverified.venus,&unverified.hex,&unverified.weight}){file->valid=true;file->detail=hex::unverifiedDependency("test dependency");}
    r.publishDependencies(unverified);frame();click("TOP.preset1");if(r.model.draft.top!=hex::Preset::Hex)throw std::runtime_error("unverified preset was hard-disabled");const auto softBefore=r.model.draft.height;click("height",.8f);if(r.model.draft.height==softBefore)throw std::runtime_error("unverified base blocked proportion editing");const auto softWeight=r.model.draft.weight;click("weight",.8f);if(r.model.draft.weight==softWeight)throw std::runtime_error("unverified optional weight was hard-disabled");io.AddMousePosEvent(io.DisplaySize.x-5,5);frame();frame();output((std::string(argv[1])+".unverified").c_str());
    unverified.weight.valid=false;r.publishDependencies(unverified);frame();const auto missingWeight=r.model.draft.weight;click("weight",.2f);if(r.model.draft.weight!=missingWeight)throw std::runtime_error("missing weight file did not hard-disable its slider");
    r.publishDependencies({});click("reset");
    r.hexAvailable=false;frame();const auto noHex=r.model.draft;click("TOP.preset1");pad(.1f,.8f);
    if(!(r.model.draft==noHex))throw std::runtime_error("missing Hex allowed a HEX button or pad change");
    pad(.85f,.7f);if(r.model.draft.top!=hex::Preset::Flat||r.model.draft.bottom!=hex::Preset::Flat)throw std::runtime_error("missing Hex blocked independent VENUS/FLAT controls");
    r.shapingAvailable=false;frame();const auto noBase=r.model.draft;click("height",.95f);click("TOP.preset0");pad(.95f,.95f);
    if(!(r.model.draft==noBase))throw std::runtime_error("missing base dependencies allowed shaping edits");
    click("tabInterface");click("dependencyHeader");if(!hex::test::controls.count("pathManager"))throw std::runtime_error("dependency settings unavailable when shaping is disabled");
    r.shapingAvailable=true;r.hexAvailable=true;
    click("tabShape");
    click("tabWardrobe");const auto mask=r.underwearHiddenMask.load();click("underwear");if(r.underwearHiddenMask!=mask)throw std::runtime_error("unavailable underwear control changed a character preference");
    click("tabInterface");click("pinHeader");click("pin8");if(!(r.quickPins&hex::PinHeight))throw std::runtime_error("Height pin checkbox did not persist");
    click("compact");frame();if(!r.compact||!hex::test::controls.count("quickWitch")||!hex::test::controls.count("height")||hex::test::controls.count("pad"))throw std::runtime_error("quick panel did not restrict itself to pinned controls");
    rightClick("height");click("pinAction8");frame();if(r.quickPins&hex::PinHeight||hex::test::controls.count("height"))throw std::runtime_error("per-item context unpin failed");
    rightClick("underwear");click("pinAction32");frame();if(r.quickPins&hex::PinUnderwear||hex::test::controls.count("underwear"))throw std::runtime_error("unavailable controls cannot be unpinned individually");
    if(!r.blockGameInput)throw std::runtime_error("input blocking is not on by default");click("blockInput");if(r.blockGameInput||ml::State::Get().blockGameInput)throw std::runtime_error("header toggle did not enable pass-through");
    if(GetPrivateProfileIntW(L"Interface",L"BlockGameInput",1,(r.profiles.parent_path()/L"interface.ini").c_str()))throw std::runtime_error("input preference did not save");
    click("compact");frame();click("tabShape");r.model.draft=copied;io.AddMousePosEvent(1100,10);frame();frame();output(argv[1]);
    ImGui::DestroyContext();std::cout<<"PASS: all nine pad points, continuous blends, independent controls, dependency gating, auto save, per-item pins/context unpin, quick-panel visibility and saved input toggle.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
