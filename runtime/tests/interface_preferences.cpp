#include "runtime.h"
#include "core/log.h"
#include "core/state.h"
#include "graphics_diagnostics.h"
#include <cmath>
#include <fstream>
#include <stdexcept>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}

static void checkDependencyDigest(const std::filesystem::path& folder){
    const auto file=folder/L"digest-fixture.bin";
    {std::ofstream fixture(file,std::ios::binary);fixture<<"original";}
    const auto original=hex::sha256(file,true);
    const auto modified=std::filesystem::last_write_time(file);
    require(original.size()==64&&original==hex::sha256(file)&&original==hex::sha256(file,true),
        "cached and fresh dependency hashes agree");
    {std::ofstream fixture(file,std::ios::binary|std::ios::trunc);fixture<<"modified";}
    std::filesystem::last_write_time(file,modified);
    const auto changed=hex::sha256(file,true);
    require(changed.size()==64&&changed!=original&&changed==hex::sha256(file),
        "editing a dependency with the same size and mtime invalidates its hash");
    std::filesystem::rename(file,folder/L"digest-replaced.bin");
    require(hex::sha256(file,true).empty(),"a removed path does not return its previous hash");
    {std::ofstream fixture(file,std::ios::binary);fixture<<"original";}
    std::filesystem::last_write_time(file,modified);
    require(hex::sha256(file,true)==original,"a replacement file is hashed again");
}

static void checkGraphicsReport(hex::Runtime& runtime)
{
    const auto options = runtime.profiles.parent_path() / L"video-options.xml";
    const auto log = options.parent_path() / L"HexAppeal.log";
    hex::setGraphicsSettingsPath(options);
    hex::observeGraphicsSwapchain(32, 32);
    {
        std::ofstream previous(log);
        previous << "Previous session\n";
    }
    ml::Log::OpenSession(log);
    LOG("Startup fixture.");
    const auto startup = runtime.recentLog();
    const auto startupMarker = startup.find("Startup fixture.");
    require(startup.rfind("Hex Appeal ", 0) == 0 && startupMarker != std::string::npos &&
            startup.find("\nGraphics\r\n") < startupMarker && startup.find("Previous session") == std::string::npos,
            "the automatic log starts with the new graphics header");
    require(startup.find("Loaded graphics libraries") == std::string::npos,
            "the automatic log does not scan graphics libraries");
    bool previousPreserved = false;
    for (const auto& file : std::filesystem::directory_iterator(log.parent_path())) {
        if (file.path() == log || file.path().extension() != L".log") continue;
        std::ifstream archive(file.path());
        std::string line;
        previousPreserved |= std::getline(archive, line) && line == "Previous session";
    }
    require(previousPreserved, "starting a session preserves its previous log");
    auto summary = hex::graphicsSummary();
    require(summary.find("Game swapchain size: Not observed yet") != std::string::npos,
            "hidden swapchain probes do not become the game resolution");
    require(summary.find("Frame Generation (saved game setting): Settings unavailable") != std::string::npos,
            "missing preferences do not imply a Frame Generation mode");
    {
        std::ofstream saved(options, std::ios::trunc);
        saved << R"(<EngineOptionSave><EngineOption><EngineOptionVideo>
            <OptionStringVector Name="_frameGenerationModeSelect" _value="Intel XeSS Frame Generation"/>
            <OptionStringVector Name="_numFramesToGenerate" _value="2">
                <OptionStringVector Name="_frameGenerationModeSelect" _value="Nested choice"/>
            </OptionStringVector>
            <OptionBool _value="True" Name="_enableHDR"/>
        </EngineOptionVideo></EngineOption></EngineOptionSave>)";
    }
    hex::observeGraphicsSwapchain(3840, 2160);
    ml::Log::RefreshGraphics();
    const auto updated = runtime.recentLog();
    ml::Log::RefreshGraphics();
    require(updated.find("Graphics diagnostics updated.") != std::string::npos &&
            updated.find("Game swapchain size: 3840 x 2160") != std::string::npos && updated == runtime.recentLog(),
            "the automatic log refreshes observed changes and skips repeated snapshots");
    require(updated.find("Loaded graphics libraries") == std::string::npos,
            "automatic log refreshes do not scan graphics libraries");
    std::filesystem::path report;
    std::string error;
    require(runtime.saveDiagnosticReport(report, error), "save report with graphics diagnostics");
    std::ifstream input(report);
    const std::string contents((std::istreambuf_iterator<char>(input)), {});
    const auto graphics = contents.find("\nGraphics\n");
    const auto checks = contents.find("\nFeature checks\n");
    require(graphics != std::string::npos && checks != std::string::npos && graphics < checks &&
            contents.find("Game swapchain size: 3840 x 2160") < checks &&
            contents.find("Frame Generation (saved game setting): Intel XeSS Frame Generation") < checks &&
            contents.find("Frames to generate (saved game setting): 2") < checks &&
            contents.find("HDR (saved game setting): True") < checks &&
            contents.find("VSync (saved game setting): Not recorded") < checks &&
            contents.find("Loaded graphics libraries (presence alone does not establish enablement)") < checks,
            "saved reports start with observed resolution, saved options and independently labelled DLLs");
    {
        std::ofstream saved(options, std::ios::trunc);
        saved << R"(<EngineOptionSave><EngineOption><EngineOptionVideo>
            <OptionStringVector Name="_frameGenerationModeSelect" _value="Off"/>)";
    }
    summary = hex::graphicsSummary();
    require(summary.find("Settings unavailable") != std::string::npos &&
            summary.find("Intel XeSS Frame Generation") == std::string::npos,
            "truncated preferences do not retain stale settings or report partial settings as current");
    hex::setGraphicsSettingsPath({});
}

void checkInterfacePreferences(const std::filesystem::path& profiles){
    hex::Runtime runtime;
    runtime.profiles=profiles;
    std::filesystem::create_directories(runtime.profiles.parent_path());const auto file=runtime.profiles.parent_path()/L"interface.ini";
    {std::ofstream old(file);old<<"[Interface]\nVersion=1\nCompact=1\nUnderwearHiddenMask=8\nFullNudeMask=2\n";}
    runtime.loadInterface(file);require(runtime.blockGameInput&&runtime.compact&&runtime.underwearHiddenMask==8&&runtime.outfitHiddenMask==2,"legacy outfit visibility migrates independently of underwear and input blocking");
    require(runtime.outfitRangeMeters==45,"legacy settings use the default 45 metre outfit range");
    {std::ofstream old(file);old<<"[Interface]\nCompact=1\nUnderwearHiddenMask=8\nFullNudeMask=2\nOutfitHiddenMask=0\n";}
    runtime.loadInterface(file);require(runtime.outfitHiddenMask==0&&runtime.underwearHiddenMask==8,"an explicit disabled outfit preference wins over its legacy key");
    {std::ofstream old(file);old<<"[Interface]\nCompact=1\nUnderwearHiddenMask=8\nUnderwearTakeoverMask=63\n";}
    runtime.loadInterface(file);require(!runtime.allowExternalUnderwear,"preserve an existing all-witch takeover");
    {std::ofstream old(file);old<<"[Interface]\nCompact=1\nUnderwearHiddenMask=8\nUnderwearTakeoverMask=8\n";}
    runtime.loadInterface(file);require(runtime.allowExternalUnderwear,"a legacy per-witch takeover keeps external control enabled globally");
    runtime.outfitHiddenMask=4;
    runtime.allowExternalUnderwear=false;
    runtime.outfitRangeMeters=63;
    for(uint32_t bit=1;bit<=hex::PinHideOutfit;bit<<=1){runtime.quickPins=bit;runtime.blockGameInput=false;require(runtime.saveInterface(),"save interface");hex::Runtime restored;restored.loadInterface(file);require(restored.quickPins==bit&&!restored.blockGameInput&&restored.compact&&restored.underwearHiddenMask==8&&restored.outfitHiddenMask==4&&!restored.allowExternalUnderwear,"each pin, input and independent visibility choice persist");}
    hex::Runtime saved;saved.loadInterface(file);require(saved.outfitRangeMeters==63,"outfit range survives a restart");
    runtime.outfitRangeMeters=0;require(runtime.saveInterface(),"save the game-default outfit range");
    saved.loadInterface(file);require(saved.outfitRangeMeters==0,"game-default outfit range is retained");
    {std::ofstream corrupt(file);corrupt<<"[Interface]\nQuickPins=-1\nBlockGameInput=1\nOutfitRangeMeters=999\n";}
    runtime.loadInterface(file);require(runtime.quickPins==hex::AllQuickPins&&runtime.blockGameInput&&ml::State::Get().blockGameInput,"unknown pin bits bounded and blocking loaded atomically");
    require(runtime.outfitRangeMeters==120,"outfit range is clamped when loading settings");
    const hex::WardrobePosition origin={-10,20,30},nearby={-7,24,42};
    require(hex::inOutfitRange(origin,nearby,13)&&!hex::inOutfitRange(origin,nearby,12),"outfit range uses the full world distance including height");
    require(!hex::inOutfitRange(origin,nearby,0)&&!hex::inOutfitRange(origin,{NAN,20,30},45),"game-default mode and invalid positions skip the extra range scan");
    require(hex::needsObservedWardrobeUpdate(true,true,false)&&hex::needsObservedWardrobeUpdate(false,false,true)&&
            !hex::needsObservedWardrobeUpdate(true,false,false)&&!hex::needsObservedWardrobeUpdate(true,true,true),
            "render observations recover missing actors without repeating a range update or ignoring a known distance");
    checkGraphicsReport(runtime);
    checkDependencyDigest(profiles.parent_path());
}
