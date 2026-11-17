#pragma once
#include <Windows.h>
#include <filesystem>
#include <string>
#include <atomic>
#include <mutex>
#include "model.h"
#include "mounted_dependencies.h"
#include "compatibility.h"

namespace hex {
enum QuickPin : uint32_t {
    PinTop=1u<<0,PinBottom=1u<<1,PinPad=1u<<2,PinHeight=1u<<3,PinWeight=1u<<4,
    PinUnderwear=1u<<5,PinOutfit=1u<<6,PinCopy=1u<<7,PinPaste=1u<<8,PinReset=1u<<9,
    PinUnderwearColour=1u<<10,PinDancerColour=1u<<11,
    AllQuickPins=(1u<<12)-1
};
struct NativeState {
    bool instantLoaded=false;
    bool instantSupported=false;
    bool instantReady=false;
    const HexAppealBackendV1* backend=nullptr;
    std::string detail="Checking the installed editor.";
    std::string shapingDetail;
};
struct Runtime {
    using BackendFactory=const HexAppealBackendV1*(*)(std::string&);
    std::atomic<BackendFactory> providerFactory{nullptr};
    Model model;
    NativeState native;
    std::filesystem::path folder;
    std::filesystem::path profiles;
    int key=VK_F9;
    bool control=false,alt=false,shift=false;
    bool compact=false,livePreview=false;
    bool blockGameInput=true;
    uint32_t quickPins=PinUnderwear|PinOutfit;
    int uiScalePercent=100;
    Profile copied{};
    bool hasCopied=false;
    std::atomic<uint32_t> underwearHiddenMask{0};
    std::atomic<uint64_t> underwearVersion{1};
    std::atomic<bool> weightAvailable{false};
    std::atomic<bool> shapingAvailable{false},hexAvailable{false};
    std::atomic<bool> dependencyScanned{false};
    std::atomic<uint64_t> dependencyRevision{1};
    std::mutex dependencyMutex;
    DependencyPaths dependencyPaths;
    DependencyStatus dependencies;
    std::mutex compatibilityMutex;
    CompatibilitySnapshot compatibility;
    bool preview=false;
    uint32_t queuedMask=0;
    std::atomic<uint32_t> requestedRefitMask{0};
    bool savePending=false,autoError=false;
    uint64_t nextSaveAt=0,nextApplyAt=0;
    static Runtime& get();
    void initialize(HMODULE self);
    void refreshNative();
    bool openInstant();
    bool save();
    bool saveInterface();
    bool saveUnderwearPreference(uint32_t mask,std::string& error);
    void loadInterface(const std::filesystem::path& file);
    DependencyPaths configuredDependencyPaths();
    DependencyStatus dependencyStatus();
    bool setDependencyPaths(const DependencyPaths& paths,std::string& error);
    void publishDependencies(const DependencyStatus& status);
    FeatureCompatibility featureCompatibility(Feature feature);
    CompatibilitySnapshot compatibilityStatus();
    void reportCompatibility(Feature feature,CompatibilityState state,const std::string& detail);
    void selectionChanged();
    void requestShapeReapply(int witch);
    void tickAuto(uint64_t now=GetTickCount64(),bool flush=false);
};
std::string sha256(const std::filesystem::path& file);
int callMenu(void* fn,unsigned command,unsigned key);
}
