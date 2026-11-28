#include "spouse_interaction.h"
#include "appearance_hooks.h"
#include "core/log.h"
#include "core/state.h"
#include "game_layout.h"
#include "game_signatures.h"
#include "gui/menu.h"
#include "hooks/frame_dispatch.h"
#include "native_interface.h"
#include "pose_access.h"
#include "character_addons.h"
#include "wardrobe_snapshot.h"
#include "wardrobe_native.h"
#include "runtime.h"
#include <MinHook.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <mutex>

extern "C" {
void* HexWardrobeHudOriginal=nullptr;
alignas(4) volatile LONG HexWardrobeHideHud=0;
void HexWardrobeHudEntry();
}

namespace hex {
namespace {
struct Action {
    uint32_t key;
    uint8_t type;
    uint8_t mode;
};
constexpr Action focusAction{1969427,3,2};
constexpr Action relationshipAction{1969428,19,0};
constexpr uint32_t interactRecord=1000350;
constexpr uint32_t alternateInteractRecord=1000351;
constexpr uint16_t noRecord=UINT16_MAX;

struct Entry {
    unsigned char identity[16];
    uint16_t index;
    uint16_t padding;
    int32_t condition;
    uintptr_t label;
    unsigned char options[14];
    uint16_t parent;
    unsigned char tail[8];
};
static_assert(sizeof(Entry)==0x38&&offsetof(Entry,index)==0x10&&offsetof(Entry,label)==0x18&&offsetof(Entry,parent)==0x2e);

struct Entries {
    Entry* data;
    uint32_t size;
    uint32_t capacity;
};

struct Layout {
    uint32_t gameStamp;
    uintptr_t build, evaluate, select, records;
    uintptr_t reserve, copy, makeLabel, copyLabel, emptyLabel;
    uintptr_t focusLists[3];
};
struct PreviewLayout {
    size_t groups, rowSize, group, selected, rootState, handle, cursor;
    uintptr_t chooseCaller, closeEvent, cursorClass, commitDisplay, close;
};
constexpr PreviewLayout stockPreview{0x1e0,0x80,0x284,0x282,0x280,0x268,0x1b0,0x129c951,0x71a7ca8,0x5af0ee0,0x12a84f7,0x129cdc0};
// 2.06: the addresses come from the signature lookup when the preview route installs.
PreviewLayout expandedPreview{0x210,0x78,0x29e,0x29c,0x2bc,0x2b8,0x1e0,0,0,0,0,0};
const PreviewLayout& previewLayout(){return gameLayout().expandedRecords?expandedPreview:stockPreview;}
constexpr Layout layouts[]{
    {0x6af5d198,0x235b640,0xaf5030,0xc049f0,0x7246388,
      0xafd060,0x235e4a0,0x464550,0x4d6c20,0x6f29f48,
      {0xaf5d64,0xc05b6d,0xc0641e}},
    {0x6afdc0cf,0x235d2b0,0xaf58e0,0xc05440,0x724c528,
      0xafd910,0x2360110,0x464960,0x4d7030,0x6f2ff48,
      {0xaf6614,0xc065bd,0xc06e6e}}
};
const Layout* checkedInteractionLayout(){
    const auto* game=loadedGameLayout();
    if(!game)return nullptr;
    if(!ignoreGameLayoutTable().load(std::memory_order_acquire))
        for(const auto& entry:layouts)if(entry.gameStamp==game->timeStamp)return &entry;
    // A build found by code signature carries its interaction addresses too.
    if(const auto* found=signatureInteraction()){
        static const Layout resolved{game->timeStamp,found->build,found->evaluate,found->select,found->records,
            found->reserve,found->copy,found->makeLabel,found->copyLabel,found->emptyLabel,
            {found->focusLists[0],found->focusLists[1],found->focusLists[2]}};
        return &resolved;
    }
    return nullptr;
}

using Build=uintptr_t(*)(void*,bool,bool,Entries*);
using Evaluate=uintptr_t(*)(void*,void*,const Entry*,uint8_t,void*);
using Select=uintptr_t(*)(void*,uint32_t*,void*,const Entry*,uintptr_t,uintptr_t);
using MenuChoose=void(*)(void*,uint8_t);
using MenuCommit=void(*)(void*);
using ResolveMenuTarget=void*(*)(void*,void*,uint32_t);
using ReleaseMenuTarget=void(*)(void*);
using CloseMenu=void(*)(void*,const uint32_t*,bool);
using RefreshMenuRows=void(*)(void*,const void*);
using RefreshMenuRows206=void(*)(void*,const void*,bool);
using BuildMenuRows=void(*)(void*,void*,uint32_t,const void*);
using FindWidgetProperty=void*(*)(void*,void*,const uint8_t*);
using ResolveWidgetProperty=const unsigned char*(*)(void*);
using SetWidgetFloat=void(*)(void*,const uint8_t*,const float*,const uint8_t*);
using Reserve=void(*)(Entries*,uint32_t);
using Copy=void(*)(Entry*,const Entry*);
using MakeLabel=void(*)(uintptr_t*,const char*);
using CopyLabel=void(*)(uintptr_t*,const uintptr_t*);
Build originalBuild=nullptr;
Evaluate originalEvaluate=nullptr;
Select originalSelect=nullptr;
MenuChoose originalMenuChoose=nullptr;
MenuCommit originalMenuCommit=nullptr;
ResolveMenuTarget resolveMenuTarget=nullptr;
ReleaseMenuTarget releaseMenuTarget=nullptr;
CloseMenu closeMenu=nullptr;
RefreshMenuRows originalRefreshMenuRows=nullptr;
RefreshMenuRows206 originalRefreshMenuRows206=nullptr;
BuildMenuRows originalBuildMenuRows=nullptr;
FindWidgetProperty findWidgetProperty=nullptr;
ResolveWidgetProperty resolveWidgetProperty=nullptr;
SetWidgetFloat setWidgetFloat=nullptr;
Reserve reserveEntries=nullptr;
Copy copyEntry=nullptr;
MakeLabel makeLabel=nullptr;
CopyLabel copyLabel=nullptr;
const Layout* layout=nullptr;
uintptr_t base=0, label=0;
std::mutex labelMutex;
std::atomic<bool> labelReady{false};
std::mutex hudMutex;
void* hudHookSite=nullptr;
bool hudHookEnabled=false;
std::atomic<uint16_t> focusIndex{noRecord}, relationshipIndex{noRecord};
std::atomic<bool> reportedFault{false}, reportedMissingRecord{false};
bool nativePreviewHold=false;
struct FocusAnchor {
    void* actor;
    uint32_t character;
    uint16_t index;
};
thread_local FocusAnchor focusAnchors[firstAddonCharacter+maximumAddonCharacters]{};
struct PreviewSelection {
    void* menu=nullptr;
    uintptr_t actor=0;
    int witch=-1;
    uint16_t stock=noRecord;
    uint64_t selectedAt=0;
};
thread_local PreviewSelection pendingPreview;
struct PreviewSession {
    void* menu=nullptr;
    uintptr_t actor=0;
    int witch=-1;
    uint32_t handle=0;
    uint8_t group=0;
    uint64_t startedAt=0;
    struct CursorPart {
        void* style=nullptr;
        float originalOpacity=0.f;
    } cursor[2];
};
thread_local PreviewSession previewSession;
thread_local void* previewCommitting=nullptr;
thread_local void* returningPreviewMenu=nullptr;
thread_local uint64_t returningPreviewUntil=0;
std::atomic<bool> previewCloseRequested{false};
std::atomic<bool> previewGroupTracked{false};

uintptr_t recordTable(){
    return *reinterpret_cast<const uintptr_t*>(base+layout->records);
}
uint32_t recordCount(uintptr_t table){
    return databaseRecordCount(table);
}
uintptr_t recordAt(uintptr_t table,uint16_t index){
    if(index==noRecord||index>=recordCount(table))return 0;
    return databaseRecordAt(databaseRecordStorage(table),index);
}
size_t interactionTypeOffset(){return gameLayout().expandedRecords?0x1a:0x1e;}
uint32_t recordKey(uintptr_t row){
    return row?*reinterpret_cast<const uint32_t*>(row+(gameLayout().expandedRecords?0:8)):0;
}
uint8_t recordType(uintptr_t row){
    return *reinterpret_cast<const uint8_t*>(row+interactionTypeOffset());
}
uint8_t recordMode(uintptr_t row){
    return *reinterpret_cast<const uint8_t*>(row+interactionTypeOffset()+1);
}
bool matchesAction(uintptr_t row,const Action& action){
    return row&&recordKey(row)==action.key&&recordType(row)==action.type&&recordMode(row)==action.mode;
}
bool wardrobeAction(uintptr_t row){
    return matchesAction(row,focusAction)||matchesAction(row,relationshipAction);
}
uint16_t wardrobeIndex(uintptr_t table,uint8_t mode){
    if(mode!=focusAction.mode&&mode!=relationshipAction.mode)return noRecord;
    const auto& action=mode==focusAction.mode?focusAction:relationshipAction;
    auto& actionIndex=mode==focusAction.mode?focusIndex:relationshipIndex;
    const auto cached=actionIndex.load(std::memory_order_relaxed);
    if(matchesAction(recordAt(table,cached),action))return cached;
    const auto count=recordCount(table);
    if(count<=noRecord){
        for(uint32_t i=0;i<count;++i){
            const auto index=static_cast<uint16_t>(i);
            if(!matchesAction(recordAt(table,index),action))continue;
            actionIndex.store(index,std::memory_order_relaxed);
            return index;
        }
    }
    if(count&&!reportedMissingRecord.exchange(true))
        LOG("[HA] Wardrobe action unavailable. Check the mounted Hex Appeal body option. The F9 editor is available.");
    return noRecord;
}
bool validEntries(const Entries* entries){
    return entries&&entries->data&&entries->size&&entries->size<=entries->capacity&&entries->size<=64;
}
const Entry* outerInteract(uintptr_t table,const Entries* entries,uint8_t mode=UINT8_MAX){
    if(!validEntries(entries))return nullptr;
    const Entry* alternate=nullptr;
    for(uint32_t i=0;i<entries->size;++i){
        const auto& entry=entries->data[i];
        if(entry.parent!=noRecord)continue;
        const auto record=recordAt(table,entry.index);
        const auto key=recordKey(record);
        if(mode!=UINT8_MAX&&(!record||recordMode(record)!=mode))continue;
        if(key==interactRecord)return &entry;
        if(key==alternateInteractRecord)alternate=&entry;
    }
    return alternate;
}
const unsigned char* actorComponent(void* actor){
    if(!actor)return nullptr;
    const auto* metadata=*reinterpret_cast<const unsigned char* const*>(static_cast<unsigned char*>(actor)+0x68);
    if(!metadata)return nullptr;
    const auto* component=*reinterpret_cast<const unsigned char* const*>(metadata+0x98);
    return component&&*reinterpret_cast<void* const*>(component+8)==actor?component:nullptr;
}
size_t cachedInteractionOffset(){return gameLayout().expandedRecords?0x80:0x90;}
template<size_t N> bool follows(uintptr_t caller,const uintptr_t (&routes)[N]){
    for(const auto address:routes)if(caller==address)return true;
    return false;
}
void reportFault(){
    if(!reportedFault.exchange(true))LOG_ERR("[HA] Wardrobe could not read the native interaction layout. The F9 editor is available.");
}
void initializeLabel(){
    __try {
        const auto empty=base+layout->emptyLabel;
        label=empty;
        makeLabel(&label,"[HA] Wardrobe");
        if(label==empty)label=0;
    }__except(EXCEPTION_EXECUTE_HANDLER){label=0;reportFault();}
}
bool ensureLabel(){
    if(labelReady.load())return true;
    std::lock_guard<std::mutex> lock(labelMutex);
    if(labelReady.load())return true;
    static uint64_t next=0;
    const auto now=GetTickCount64();
    if(now<next)return false;
    next=now+1000;
    initializeLabel();
    if(!label){LOG_ERR("[HA] Wardrobe is waiting for the game's interaction label service.");return false;}
    labelReady.store(true);
    return true;
}
bool wardrobeIdentity(void* actor,ActorIdentity& identity){
    if(!readWardrobeIdentity(actor,identity))return false;
    const auto* addon=characterAddon(identity.witch);
    if(!addon)return true;
    if(!characterAddonEnabled(identity.witch)||!matchesWardrobeAppearance(identity.actor,base,addon->appearance.c_str()))return false;
    identity.recordKey=addon->recordKey;
    return true;
}

void appendWardrobe(void* context,Entries* output){
    __try {
        if(!context||!validEntries(output)||output->size==64)return;
        const auto actor=*reinterpret_cast<void**>(static_cast<unsigned char*>(context)+8);
        ActorIdentity identity{};
        if(!wardrobeIdentity(actor,identity)||actorComponent(actor)!=context)return;
        auto* npcEntries=reinterpret_cast<Entries*>(static_cast<unsigned char*>(context)+cachedInteractionOffset());
        if(output==npcEntries||output->data==npcEntries->data)return;
        const auto table=recordTable();
        const auto* interact=outerInteract(table,output);
        if(!interact)return;
        const auto nativeRecord=recordAt(table,interact->index);
        const auto mode=recordMode(nativeRecord);
        const auto index=wardrobeIndex(table,mode);
        if(index==noRecord)return;
        focusAnchors[identity.witch]={actor,identity.recordKey,interact->index};
        for(uint32_t i=0;i<output->size;++i){
            if(wardrobeAction(recordAt(table,output->data[i].index)))return;
        }
        if(!ensureLabel())return;
        const size_t sample=interact-output->data;
        const auto size=output->size;
        if(size==output->capacity)reserveEntries(output,size+1);
        if(!output->data||size>=output->capacity||output->size!=size)return;
        auto& entry=output->data[size];
        copyEntry(&entry,&output->data[sample]);
        entry.index=index;
        copyLabel(&entry.label,&label);
        ++output->size;
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();}
}
const Entry* eligibilitySource(void* target,const Entry* entry,Entry& temporary){
    __try {
        if(!entry||entry->parent!=noRecord)return nullptr;
        const auto table=recordTable();
        const auto wardrobe=recordAt(table,entry->index);
        if(!wardrobeAction(wardrobe))return nullptr;
        const auto mode=recordMode(wardrobe);
        ActorIdentity identity{};
        if(!wardrobeIdentity(target,identity))return nullptr;
        const auto* component=actorComponent(target);
        if(!component)return nullptr;
        const auto* cached=reinterpret_cast<const Entries*>(component+cachedInteractionOffset());
        if(const auto* source=outerInteract(table,cached,mode))return source;
        const auto& anchor=focusAnchors[identity.witch];
        if(anchor.actor!=target||anchor.character!=identity.recordKey)return nullptr;
        const auto record=recordAt(table,anchor.index);
        if(!record||recordMode(record)!=mode)return nullptr;
        const auto key=recordKey(record);
        if(key!=interactRecord&&key!=alternateInteractRecord)return nullptr;
        temporary=*entry;
        temporary.index=anchor.index;
        return &temporary;
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();return nullptr;}
}
bool openWardrobe(uint32_t* result,void* target,const Entry* entry){
    __try {
        if(!result||!entry||!wardrobeAction(recordAt(recordTable(),entry->index)))return false;
        ActorIdentity identity{};
        if(!wardrobeIdentity(target,identity)||!actorComponent(target))return false;
        if(!ml::State::Get().HasRecentPresentation(GetTickCount64())){
            *result=0;
            LOG_ERR("[HA] Wardrobe could not open: no recent game frame reached Hex Appeal's renderer.");
            return true;
        }
        observeNativeWardrobeInteraction(identity.actor,identity.witch);
        if(!ml::gui::RequestSpouseWardrobe(identity.witch))return false;
        *result=0;
        LOG("[HA] Wardrobe selected for %s.",identity.name);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();return false;}
}

bool menuIdentity(void* menu,ActorIdentity& identity){
    if(!resolveMenuTarget||!releaseMenuTarget)return false;
    __try{
        const auto handle=*reinterpret_cast<const uint32_t*>(static_cast<const char*>(menu)+previewLayout().handle);
        if(!handle||handle==UINT32_MAX)return false;
        alignas(8) unsigned char reference[0x20]{};
        resolveMenuTarget(nullptr,reference,handle);
        bool valid=false;
        __try{
            auto* actor=*reinterpret_cast<void**>(reference+8);
            valid=reference[0x10]&&wardrobeIdentity(actor,identity)&&actorComponent(actor);
        }__finally{releaseMenuTarget(reference);}
        return valid;
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();return false;}
}
struct MenuRows {
    const unsigned char* data=nullptr;
    uint32_t count=0;

    const Entry* entry(uint8_t index) const{
        return index<count?*reinterpret_cast<const Entry* const*>(data+size_t(index)*previewLayout().rowSize):nullptr;
    }
};
const unsigned char* menuGroup(void* menu,uint8_t group){
    const auto* bytes=static_cast<const unsigned char*>(menu);
    const auto groups=previewLayout().groups;
    const auto listCount=*reinterpret_cast<const uint32_t*>(bytes+groups+8);
    if(group>=listCount||listCount>64)return nullptr;
    const auto* lists=*reinterpret_cast<const unsigned char* const*>(bytes+groups);
    return lists?lists+size_t(group)*16:nullptr;
}
MenuRows menuRows(void* menu,uint8_t group){
    const auto* list=menuGroup(menu,group);
    if(!list)return {};
    const auto count=*reinterpret_cast<const uint32_t*>(list+8);
    const auto* rows=*reinterpret_cast<const unsigned char* const*>(list);
    return rows&&count<=64?MenuRows{rows,count}:MenuRows{};
}
uint8_t choosePreviewInteract(void* menu,uint8_t option){
    __try{
        if(static_cast<const unsigned char*>(menu)[previewLayout().group]!=0)return option;
        const auto rows=menuRows(menu,0);
        const auto* selected=rows.entry(option);
        if(!selected||selected->parent!=noRecord)return option;
        const auto table=recordTable();
        const auto action=recordAt(table,selected->index);
        if(!wardrobeAction(action))return option;
        if(!ml::State::Get().HasRecentPresentation(GetTickCount64()))return option;
        const auto mode=recordMode(action);
        for(uint8_t i=0;i<rows.count;++i){
            const auto* sibling=rows.entry(i);
            if(!sibling||sibling->parent!=noRecord)continue;
            const auto record=recordAt(table,sibling->index);
            const auto key=recordKey(record);
            if((key!=interactRecord&&key!=alternateInteractRecord)||
               recordType(record)!=16||recordMode(record)!=mode)continue;
            ActorIdentity identity{};
            if(!menuIdentity(menu,identity))return option;
            pendingPreview={menu,identity.actor,identity.witch,sibling->index,GetTickCount64()};
            LOG("[HA] Wardrobe selected native Interact for %s.",identity.name);
            return i;
        }
        return option;
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();return option;}
}
bool matchingPreviewCommit(void* menu,const PreviewSelection& pending){
    if(!pending.menu||menu!=pending.menu||GetTickCount64()-pending.selectedAt>2000)return false;
    __try{
        const auto option=static_cast<const unsigned char*>(menu)[previewLayout().selected];
        if(static_cast<const unsigned char*>(menu)[previewLayout().group]!=0)return false;
        const auto* entry=menuRows(menu,0).entry(option);
        ActorIdentity identity{};
        return entry&&entry->index==pending.stock&&menuIdentity(menu,identity)&&
               identity.actor==pending.actor&&identity.witch==pending.witch;
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();return false;}
}
constexpr uint8_t widgetOpacity=0x50;
constexpr uint8_t localWidgetValue=1;
void* previewCursorStyle(void* menu,size_t part){
    const auto& offsets=previewLayout();
    const auto* control=*reinterpret_cast<const unsigned char* const*>(static_cast<const unsigned char*>(menu)+offsets.cursor);
    if(!control||!offsets.cursorClass||*reinterpret_cast<const uintptr_t*>(control)!=base+offsets.cursorClass)return nullptr;
    const auto* widget=*reinterpret_cast<const unsigned char* const*>(control+0xe8+part*sizeof(void*));
    return widget?*reinterpret_cast<void* const*>(widget+0x20):nullptr;
}
bool readWidgetOpacity(void* style,float& value){
    alignas(8) unsigned char reference[32]{};
    const auto* property=resolveWidgetProperty(findWidgetProperty(style,reference,&widgetOpacity));
    if(!property)return false;
    const auto* data=property[0x0e]?*reinterpret_cast<const float* const*>(property):reinterpret_cast<const float*>(property);
    if(!data)return false;
    value=*data;
    return value>=0.f&&value<=1.f;
}
void setPreviewCursorVisibility(PreviewSession& session,bool restore){
    if(!setWidgetFloat)return;
    __try{
        for(size_t i=0;i<std::size(session.cursor);++i){
            auto& entry=session.cursor[i];
            auto* style=previewCursorStyle(session.menu,i);
            if(!style||(entry.style&&entry.style!=style))continue;
            if(restore){
                if(entry.style)setWidgetFloat(style,&widgetOpacity,&entry.originalOpacity,&localWidgetValue);
                continue;
            }
            float opacity=0.f;
            if(!readWidgetOpacity(style,opacity)||opacity==0.f)continue;
            if(!entry.style){
                entry={style,opacity};
                LOG("[HA] Wardrobe suppressed its native radius cursor %s.",i==0?"line":"tip");
            }
            constexpr float hidden=0.f;
            setWidgetFloat(style,&widgetOpacity,&hidden,&localWidgetValue);
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();}
}
bool startPreviewSession(void* menu,const PreviewSelection& pending){
    __try{
        const auto* bytes=static_cast<const unsigned char*>(menu);
        const auto group=bytes[previewLayout().group];
        const auto handle=*reinterpret_cast<const uint32_t*>(bytes+previewLayout().handle);
        if(!group||!handle||handle==UINT32_MAX||!menuGroup(menu,group))return false;
        previewSession={menu,pending.actor,pending.witch,handle,group,GetTickCount64()};
        previewCloseRequested.store(false);
        setPreviewCursorVisibility(previewSession,false);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();return false;}
}
bool previewIsCurrent(const PreviewSession& session){
    __try{
        const auto* bytes=static_cast<const unsigned char*>(session.menu);
        return bytes[previewLayout().group]==session.group&&
               *reinterpret_cast<const uint32_t*>(bytes+previewLayout().handle)==session.handle&&
               menuGroup(session.menu,session.group)!=nullptr;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool suppressPreviewChoices(void* menu,void* displayRows,uintptr_t caller){
    const auto commitDisplay=previewLayout().commitDisplay;
    if(previewCommitting==menu&&commitDisplay&&caller==commitDisplay){
        __try{
            const auto* bytes=static_cast<const unsigned char*>(menu);
            const auto handle=*reinterpret_cast<const uint32_t*>(bytes+previewLayout().handle);
            return handle&&handle!=UINT32_MAX;
        }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
    if(previewSession.menu!=menu||!previewIsCurrent(previewSession))return false;
    __try{return displayRows==menuGroup(menu,previewSession.group);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void buildInteractionMenuRows(void* menu,void* displayRows,uint32_t style,const void* options){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-base;
    if(suppressPreviewChoices(menu,displayRows,caller)){
        alignas(8) unsigned char empty[16]{};
        originalBuildMenuRows(menu,displayRows,style,empty);
    }else originalBuildMenuRows(menu,displayRows,style,options);
}
void clearPreviewSelection(void* menu){
    auto* bytes=static_cast<unsigned char*>(menu);
    const auto selected=previewLayout().selected;
    bytes[selected]=UINT8_MAX;
    bytes[selected+1]=UINT8_MAX;
}
void resetInvalidReturnedSelection(void* menu){
    if(menu!=returningPreviewMenu||GetTickCount64()>returningPreviewUntil)return;
    __try{
        const auto* bytes=static_cast<const unsigned char*>(menu);
        const auto& offsets=previewLayout();
        const auto selected=bytes[offsets.selected];
        if(bytes[offsets.group]!=0||bytes[offsets.rootState]!=0||selected==UINT8_MAX)return;
        const auto rows=menuRows(menu,0);
        if(!rows.count)return;
        // The root refresh assumes that a selected index has a live widget.
        if(selected<rows.count){
            const auto* widget=*reinterpret_cast<const unsigned char* const*>(rows.data+size_t(selected)*offsets.rowSize+8);
            void* control=nullptr;
            if(widget&&creation::Read(widget+8,&control,sizeof control)&&control)return;
        }
        clearPreviewSelection(menu);
        LOG("[HA] Wardrobe discarded a stale native selection during Interact return.");
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();}
}
void refreshInteractionMenuRows(void* menu,const void* options){
    resetInvalidReturnedSelection(menu);
    originalRefreshMenuRows(menu,options);
}
void refreshInteractionMenuRows206(void* menu,const void* options,bool refreshSelection){
    resetInvalidReturnedSelection(menu);
    originalRefreshMenuRows206(menu,options,refreshSelection);
}
void closePreviewSession(const PreviewSession& session){
    __try{
        ActorIdentity identity{};
        if(previewIsCurrent(session)&&menuIdentity(session.menu,identity)&&
           identity.actor==session.actor&&identity.witch==session.witch){
            if(gameLayout().expandedRecords){
                const auto vtable=*reinterpret_cast<const uintptr_t*>(session.menu);
                if(!vtable||!previewLayout().close||*reinterpret_cast<const uintptr_t*>(vtable+0x510)!=base+previewLayout().close){
                    LOG_ERR("[HA] Wardrobe could not verify the native Interact close handler.");
                    return;
                }
            }
            const auto event=*reinterpret_cast<const uint32_t*>(base+previewLayout().closeEvent);
            if(!event||event==UINT32_MAX){
                LOG_ERR("[HA] Wardrobe could not resolve the native Interact close event.");
                return;
            }
            returningPreviewMenu=session.menu;
            returningPreviewUntil=GetTickCount64()+2000;
            clearPreviewSelection(session.menu);
            closeMenu(session.menu,&event,false);
            LOG("[HA] Wardrobe closed its native Interact group.");
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){reportFault();}
}
void chooseInteractionMenu(void* menu,uint8_t option){
    const auto chooseCaller=previewLayout().chooseCaller;
    if(chooseCaller&&reinterpret_cast<uintptr_t>(_ReturnAddress())-base==chooseCaller&&frameThreadId()==GetCurrentThreadId()){
        pendingPreview={};
        option=choosePreviewInteract(menu,option);
    }
    originalMenuChoose(menu,option);
}
void commitInteractionMenu(void* menu){
    const auto pending=pendingPreview;
    pendingPreview={};
    const bool opensWardrobe=matchingPreviewCommit(menu,pending);
    if(opensWardrobe)previewCommitting=menu;
    __try{originalMenuCommit(menu);}
    __finally{if(opensWardrobe)previewCommitting=nullptr;}
    if(opensWardrobe){
        const bool active=startPreviewSession(menu,pending);
        previewGroupTracked.store(active);
        if(active)observeNativeWardrobeInteraction(pending.actor,pending.witch);
        else {
            previewSession={menu,pending.actor,pending.witch,0,0,GetTickCount64()};
            previewCloseRequested.store(false);
            LOG_ERR("[HA] Wardrobe could not track the borrowed Interact group. HUD suppression is unavailable for this preview.");
        }
        if(ml::gui::RequestSpouseWardrobe(pending.witch))
            LOG("[HA] Wardrobe opened after the native Interact menu committed (preview group=%d).",int(active));
        else if(previewSession.menu)previewCloseRequested.store(true);
    }
}

uintptr_t buildInteraction(void* context,bool cached,bool alternate,Entries* output){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-base;
    const auto value=originalBuild(context,cached,alternate,output);
    const bool gameThread=frameThreadId()==GetCurrentThreadId();
    const auto* resolved=gameLayout().expandedRecords?signatureInteraction():nullptr;
    if(cached&&(gameThread||follows(caller,layout->focusLists)||
        (resolved&&follows(caller,resolved->alternateFocusLists))))appendWardrobe(context,output);
    return value;
}
uintptr_t evaluateInteraction(void* context,void* target,const Entry* entry,uint8_t mode,void* result){
    Entry temporary{};
    if(const auto* source=eligibilitySource(target,entry,temporary))entry=source;
    return originalEvaluate(context,target,entry,mode,result);
}
uintptr_t selectInteraction(void* context,uint32_t* result,void* target,const Entry* entry,uintptr_t arg5,uintptr_t arg6){
    if(openWardrobe(result,target,entry))return reinterpret_cast<uintptr_t>(result);
    return originalSelect(context,result,target,entry,arg5,arg6);
}

template<class T> T gameFunction(HMODULE game,uintptr_t offset){
    const auto* address=reinterpret_cast<const void*>(base+offset);
    return executablePluginFunction(game,address)?reinterpret_cast<T>(base+offset):nullptr;
}
template<size_t M>
bool checkedHead(HMODULE game,void* address,const unsigned char (&head)[15],const unsigned char (&tail)[M]){
    return executablePluginFunction(game,address)&&
        appearance::checkedGameEntry(address,head,16,tail)!=appearance::GameEntry::Unavailable;
}
bool installPreviewInteraction(HMODULE game){
    if(!nativePreviewHold)return true;
    const bool expanded=gameLayout().expandedRecords;
    if(!expanded&&layout->gameStamp!=0x6afdc0cf)return true;
    // 2.06 addresses come from the signature lookup, so another link of the same
    // code (a different store's executable) gets the same route.
    const auto* found=expanded?signatureExpanded():nullptr;
    if(expanded){
        static bool reported=false;
        const uintptr_t required[]{found?found->menuChoose:0,found?found->menuCommit:0,found?found->menuDisplay:0,
            found?found->menuRefresh:0,found?found->menuResolve:0,found?found->menuRelease:0,found?found->menuClose:0,
            found?found->chooseCaller:0,found?found->commitDisplay:0,found?found->closeEvent:0};
        if(std::find(std::begin(required),std::end(required),uintptr_t(0))!=std::end(required)){
            if(!reported){reported=true;LOG_ERR("[HA] Wardrobe could not find every native Interact preview address on this build. The [HA] Wardrobe row still opens the editor.");}
            return false;
        }
        expandedPreview.chooseCaller=found->chooseCaller;expandedPreview.closeEvent=found->closeEvent;
        expandedPreview.cursorClass=found->cursorClass;expandedPreview.commitDisplay=found->commitDisplay;
        expandedPreview.close=found->menuClose;
    }
    const unsigned char chooseHead[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
    const unsigned char commitHead[]{0x48,0x89,0x4c,0x24,0x08,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56};
    const unsigned char resolveHead[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x54,0x24,0x10,0x57,0x48,0x83,0xec,0x50};
    const unsigned char releaseHead[]{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20};
    const unsigned char closeHead[]{0x40,0x53,0x55,0x56,0x57,0x41,0x56,0x48,0x83,0xec,0x50,0xc5,0xf8,0x29,0x74};
    const unsigned char refreshHead[]{0x48,0x89,0x54,0x24,0x10,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56};
    const unsigned char displayHead[]{0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x4c,0x89,0x48,0x20,0x44,0x89,0x40,0x18};
    const unsigned char chooseTail[]{0x48,0x81,0xec,0x90,0x01,0x00,0x00,0x48,0x8b,0xd9,0x48,0x8b,0x05,0x17,0x3c};
    const unsigned char commitTail[]{0x57,0x48,0x8d,0x6c,0x24,0x98,0x48,0x81,0xec,0x68,0x01,0x00,0x00,0x48,0x8b};
    const unsigned char displayTail[]{0x89,0x50,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48};
    const unsigned char refreshTail[]{0x57,0x48,0x8d,0x6c,0x24,0xe1,0x48,0x81,0xec,0xc8,0x00,0x00,0x00,0xc5,0xf8};
    const unsigned char findPropertyHead[]{0x40,0x55,0x53,0x56,0x41,0x54,0x48,0x8b,0xec,0x48,0x83,0xec,0x78,0x4c,0x8b};
    const unsigned char resolvePropertyHead[]{0x48,0x8b,0xd1,0x0f,0xb6,0x49,0x0c,0x80,0xf9,0xff,0x74,0x47,0x4c,0x8b,0x02};
    const unsigned char setPropertyHead[]{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x56,0x57,0x41,0x54,0x41};
    const unsigned char expandedChooseHead[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x81,0xec,0x90};
    const unsigned char expandedChooseTail[]{0x00,0x00,0x48,0x8b,0xf9,0x48,0x8b,0x05};
    const unsigned char expandedCommitTail[]{0x57,0x48,0x8d,0x6c,0x24,0xe8,0x48,0x81,0xec,0x18,0x01,0x00,0x00,0x48,0x8b};
    const unsigned char expandedRefreshHead[]{0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec};
    const unsigned char expandedRefreshTail[]{0x45,0x0f,0xb6,0xf0,0x48,0x8b,0xf2,0x48,0x8b,0xf9,0x48,0x8b,0x05};
    const unsigned char expandedDisplayHead[]{0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x44,0x89,0x40,0x18,0x55,0x56,0x57,0x41};
    const unsigned char expandedDisplayTail[]{0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0xa8,0x48,0x81,0xec,0x20,0x01};
    const unsigned char expandedFindPropertyHead[]{0x40,0x55,0x53,0x57,0x41,0x54,0x41,0x56,0x48,0x8b,0xec,0x48,0x83,0xec,0x50};
    const unsigned char expandedResolvePropertyHead[]{0x8b,0x41,0x10,0x48,0x8b,0xd1,0x85,0xc0,0x74,0x55,0x0f,0xb6,0x49,0x0c,0x80};
    const auto matchesHead=[&](uintptr_t offset,const unsigned char (&head)[15]){
        auto* address=reinterpret_cast<void*>(base+offset);
        unsigned char bytes[sizeof head]{};
        return executablePluginFunction(game,address)&&creation::Read(address,bytes,sizeof bytes)&&
               std::memcmp(bytes,head,sizeof bytes)==0;
    };
    const uintptr_t resolveAddress=expanded?found->menuResolve:0x748c50;
    const uintptr_t releaseAddress=expanded?found->menuRelease:0x165af00;
    const uintptr_t closeAddress=previewLayout().close;
    // On 2.06 the close handler is read from the Interact class's vtable, and its
    // entry may be a protector jump, so only its placement is checked here. The
    // live menu's vtable is checked again before every close.
    if(!matchesHead(resolveAddress,resolveHead)||!matchesHead(releaseAddress,releaseHead)||
       !(expanded?executablePluginFunction(game,reinterpret_cast<void*>(base+closeAddress)):matchesHead(closeAddress,closeHead)))return false;
    resolveMenuTarget=gameFunction<ResolveMenuTarget>(game,resolveAddress);
    releaseMenuTarget=gameFunction<ReleaseMenuTarget>(game,releaseAddress);
    closeMenu=gameFunction<CloseMenu>(game,closeAddress);
    if(!resolveMenuTarget||!releaseMenuTarget||!closeMenu)return false;
    const uintptr_t findPropertyAddress=expanded?found->widgetFind:0x3918f50;
    const uintptr_t resolvePropertyAddress=expanded?found->widgetResolve:0x700360;
    const uintptr_t setPropertyAddress=expanded?found->widgetSet:0x73eee0;
    if(findPropertyAddress&&resolvePropertyAddress&&setPropertyAddress&&
       matchesHead(findPropertyAddress,expanded?expandedFindPropertyHead:findPropertyHead)&&
       matchesHead(resolvePropertyAddress,expanded?expandedResolvePropertyHead:resolvePropertyHead)&&
       matchesHead(setPropertyAddress,setPropertyHead)){
        findWidgetProperty=gameFunction<FindWidgetProperty>(game,findPropertyAddress);
        resolveWidgetProperty=gameFunction<ResolveWidgetProperty>(game,resolvePropertyAddress);
        if(findWidgetProperty&&resolveWidgetProperty)
            setWidgetFloat=gameFunction<SetWidgetFloat>(game,setPropertyAddress);
    }
    struct PreviewHook{
        uintptr_t offset;
        bool checked;
        void* detour;
        void** original;
    };
    const auto at=[&](uintptr_t offset){return reinterpret_cast<void*>(base+offset);};
    const auto hooks=expanded?std::array<PreviewHook,4>{{
        {found->menuChoose,checkedHead(game,at(found->menuChoose),expandedChooseHead,expandedChooseTail),reinterpret_cast<void*>(chooseInteractionMenu),reinterpret_cast<void**>(&originalMenuChoose)},
        {found->menuCommit,checkedHead(game,at(found->menuCommit),commitHead,expandedCommitTail),reinterpret_cast<void*>(commitInteractionMenu),reinterpret_cast<void**>(&originalMenuCommit)},
        {found->menuDisplay,checkedHead(game,at(found->menuDisplay),expandedDisplayHead,expandedDisplayTail),reinterpret_cast<void*>(buildInteractionMenuRows),reinterpret_cast<void**>(&originalBuildMenuRows)},
        {found->menuRefresh,checkedHead(game,at(found->menuRefresh),expandedRefreshHead,expandedRefreshTail),reinterpret_cast<void*>(refreshInteractionMenuRows206),reinterpret_cast<void**>(&originalRefreshMenuRows206)}}}:
        std::array<PreviewHook,4>{{
        {0x12a7770,checkedHead(game,at(0x12a7770),chooseHead,chooseTail),reinterpret_cast<void*>(chooseInteractionMenu),reinterpret_cast<void**>(&originalMenuChoose)},
        {0x12a7d10,checkedHead(game,at(0x12a7d10),commitHead,commitTail),reinterpret_cast<void*>(commitInteractionMenu),reinterpret_cast<void**>(&originalMenuCommit)},
        {0x129fdd0,checkedHead(game,at(0x129fdd0),displayHead,displayTail),reinterpret_cast<void*>(buildInteractionMenuRows),reinterpret_cast<void**>(&originalBuildMenuRows)},
        {0x129f2e0,checkedHead(game,at(0x129f2e0),refreshHead,refreshTail),reinterpret_cast<void*>(refreshInteractionMenuRows),reinterpret_cast<void**>(&originalRefreshMenuRows)}}};
    for(const auto& hook:hooks)if(!hook.checked)return false;
    size_t installed=0;
    bool queued=true;
    for(const auto& hook:hooks){
        auto* address=reinterpret_cast<void*>(base+hook.offset);
        if(MH_CreateHook(address,hook.detour,hook.original)!=MH_OK)break;
        ++installed;
        if(MH_QueueEnableHook(address)!=MH_OK){queued=false;break;}
    }
    if(installed==std::size(hooks)&&queued&&MH_ApplyQueued()==MH_OK){
        LOG("[HA] Wardrobe native Interact group routing ready (radius cursor styling=%d).",int(setWidgetFloat!=nullptr));
        return true;
    }
    for(size_t i=0;i<installed;++i){
        auto* address=reinterpret_cast<void*>(base+hooks[i].offset);
        MH_RemoveHook(address);
    }
    return false;
}
bool installInteraction(){
    const auto config=Runtime::get().folder/L"HexAppeal.ini";
    nativePreviewHold=GetPrivateProfileIntW(L"Wardrobe",L"HoldCharacterDuringPreview",1,config.c_str())!=0;
    layout=checkedInteractionLayout();
    if(!layout)return false;
    const auto game=GetModuleHandleW(nullptr);
    base=reinterpret_cast<uintptr_t>(game);
    reserveEntries=gameFunction<Reserve>(game,layout->reserve);
    copyEntry=gameFunction<Copy>(game,layout->copy);
    makeLabel=gameFunction<MakeLabel>(game,layout->makeLabel);
    copyLabel=gameFunction<CopyLabel>(game,layout->copyLabel);
    if(!reserveEntries||!copyEntry||!makeLabel||!copyLabel)return false;
    const unsigned char buildHead[]{0x48,0x89,0x5c,0x24,0x08,0x44,0x88,0x44,0x24,0x18,0x55,0x56,0x57,0x41,0x54};
    const unsigned char expandedBuildHead[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x20};
    const unsigned char evaluateHead[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18};
    const unsigned char selectHead[]{0x48,0x89,0x5c,0x24,0x08,0x4c,0x89,0x4c,0x24,0x20,0x4c,0x89,0x44,0x24,0x18};
    const unsigned char buildTail[]{0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xd9,0x48,0x81,0xec,0xe0,0x00};
    const unsigned char expandedBuildTail[]{0x88,0x44,0x24,0x18,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d};
    const unsigned char evaluateTail[]{0x88,0x4c,0x24,0x20,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8b};
    const unsigned char selectTail[]{0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    struct Hook {
        uintptr_t offset;
        const unsigned char (&signature)[15];
        const unsigned char (&tail)[15];
        void* detour;
        void** original;
    };
    const Hook hooks[]{
        {layout->build,gameLayout().expandedRecords?expandedBuildHead:buildHead,
         gameLayout().expandedRecords?expandedBuildTail:buildTail,reinterpret_cast<void*>(buildInteraction),reinterpret_cast<void**>(&originalBuild)},
        {layout->evaluate,evaluateHead,evaluateTail,reinterpret_cast<void*>(evaluateInteraction),reinterpret_cast<void**>(&originalEvaluate)},
        {layout->select,selectHead,selectTail,reinterpret_cast<void*>(selectInteraction),reinterpret_cast<void**>(&originalSelect)}
    };
    for(const auto& hook:hooks){
        auto* entry=reinterpret_cast<void*>(base+hook.offset);
        if(checkedHead(game,entry,hook.signature,hook.tail))continue;
        const auto issue=appearance::entryIssue(entry,"A native Wardrobe focus hook");
        static std::string previousIssue;
        if(issue!=previousIssue){LOG_ERR("%s",issue.c_str());previousIssue=issue;}
        return false;
    }
    size_t count=0;
    bool queued=true;
    for(const auto& hook:hooks){
        auto* address=reinterpret_cast<void*>(base+hook.offset);
        if(MH_CreateHook(address,hook.detour,hook.original)!=MH_OK)break;
        ++count;
        if(MH_QueueEnableHook(address)!=MH_OK){queued=false;break;}
    }
    if(queued&&count==3&&MH_ApplyQueued()==MH_OK){
        LOG("[HA] Wardrobe native focus prompts ready.");
        return true;
    }
    for(size_t i=0;i<count;++i)MH_RemoveHook(reinterpret_cast<void*>(base+hooks[i].offset));
    return false;
}
bool interactionTableLoaded(){
    const auto* checked=checkedInteractionLayout();
    if(!checked)return false;
    const auto game=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    __try{
        const auto table=*reinterpret_cast<const uintptr_t*>(game+checked->records);
        const auto count=recordCount(table);
        return count&&count<=noRecord&&databaseRecordStorage(table)!=0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
void initializeWardrobeInteraction(){
    static const bool enabled=GetPrivateProfileIntW(L"Wardrobe",L"EnableInteraction",1,(Runtime::get().folder/L"HexAppeal.ini").c_str())!=0;
    static bool focusReady=false,previewReady=false;
    static uint64_t nextCheck=0;
    const auto now=GetTickCount64();
    if(previewReady||now<nextCheck)return;
    const bool first=nextCheck==0;
    nextCheck=now+2000;
    if(!enabled){
        if(first)LOG("Native [HA] Wardrobe is disabled in HexAppeal.ini. The F9 editor remains available.");
        return;
    }
    if(!interactionTableLoaded()){
        if(first)LOG("Native [HA] Wardrobe is waiting for a checked game interaction table.");
        return;
    }
    if(!focusReady)focusReady=installInteraction();
    if(focusReady)previewReady=installPreviewInteraction(GetModuleHandleW(nullptr));
    if(first&&!previewReady)LOG("[HA] Wardrobe will retry the native interaction route. The F9 editor is available.");
}
void requestWardrobeInteractionClose() noexcept{
    previewCloseRequested.store(true);
}
void setWardrobePreviewHudHidden(bool hide){
    std::lock_guard<std::mutex> lock(hudMutex);
    if(!hide){
        InterlockedExchange(&HexWardrobeHideHud,0);
        if(hudHookEnabled&&MH_DisableHook(hudHookSite)==MH_OK)hudHookEnabled=false;
        return;
    }
    if(!previewGroupTracked.load()&&!gameLayout().expandedRecords)return;
    if(hudHookEnabled){InterlockedExchange(&HexWardrobeHideHud,1);return;}
    const auto* game=loadedGameLayout();
    if(!game||!base||!game->hudCall||!game->hudSubmit)return;
    auto* call=reinterpret_cast<unsigned char*>(base+game->hudCall);
    auto* target=reinterpret_cast<void*>(base+game->hudSubmit);
    const unsigned char hudStart[]{0x45,0x3b,0xc1,0x0f,0x84,0xff,0x09,0x00,0x00,
        0x48,0x89,0x5c,0x24,0x10,0x44,0x89,0x44,0x24,0x18};
    const unsigned char hudTail[]{0x89,0x4c,0x24,0x08,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    const unsigned char afterCall[]{0xb8,0xff,0xff,0x00,0x00};
    unsigned char bytes[10]{};
    int32_t displacement{};
    if(!creation::Read(call,bytes,sizeof bytes)||bytes[0]!=0xe8||
       std::memcmp(bytes+5,afterCall,sizeof afterCall)!=0||
       !creation::ExecutableAddress(target))return;
    std::memcpy(&displacement,bytes+1,sizeof displacement);
    if(base+game->hudCall+5+static_cast<intptr_t>(displacement)!=reinterpret_cast<uintptr_t>(target))return;
    if(appearance::checkedGameEntry(target,hudStart,20,hudTail)==appearance::GameEntry::Unavailable)return;
    if(!hudHookSite){
        if(MH_CreateHook(call,reinterpret_cast<void*>(&HexWardrobeHudEntry),&HexWardrobeHudOriginal)!=MH_OK)return;
        hudHookSite=call;
    }
    if(MH_EnableHook(hudHookSite)==MH_OK){
        InterlockedExchange(&HexWardrobeHideHud,1);
        hudHookEnabled=true;
        LOG("[HA] Wardrobe hid the native HUD for this interaction preview.");
    }
}
uintptr_t wardrobePreviewActor() noexcept{
    return previewSession.group?previewSession.actor:0;
}
void tickWardrobeInteraction() noexcept{
    if(!previewSession.menu)return;
    const bool closeRequested=previewCloseRequested.exchange(false);
    const bool drawExpired=!closeRequested&&ml::State::Get().NativeWardrobeDrawExpired(previewSession.startedAt,GetTickCount64());
    const bool groupLost=previewSession.group&&!previewIsCurrent(previewSession);
    if(!closeRequested&&!drawExpired&&!groupLost){
        if(previewSession.group)setPreviewCursorVisibility(previewSession,false);
        return;
    }
    auto session=previewSession;
    previewSession={};
    previewGroupTracked.store(false);
    if(session.group&&!groupLost)setPreviewCursorVisibility(session,true);
    setWardrobePreviewHudHidden(false);
    if(!closeRequested){
        LOG_ERR("[HA] Wardrobe closed its preview because %s.",drawExpired?
            "the editor stopped drawing":"the game closed or replaced the Interact group");
        ml::gui::CancelNativeWardrobe();
        cancelSavedWardrobe(session.witch);
    }
    if(session.group&&!groupLost)closePreviewSession(session);
}
}
