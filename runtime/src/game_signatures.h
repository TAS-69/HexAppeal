#pragma once
// Game addresses by code signature, for Crimson Desert builds that are not in
// the address table (game_layout.h). Every required address must match exactly
// once and sit in the right kind of section, or nothing is used and features
// wait exactly as they do on any unrecognised build.
#include "game_layout.h"
#include <cstddef>
#include <filesystem>
#include <string>

namespace hex {
// Addresses the native Wardrobe interaction uses (spouse_interaction.cpp).
struct InteractionAddresses {
    uintptr_t build=0,evaluate=0,select=0,records=0,reserve=0,copy=0,makeLabel=0,copyLabel=0,emptyLabel=0;
    uintptr_t focusLists[3]{};
    uintptr_t alternateFocusLists[4]{};
};
// 2.06 addresses outside the required layout. Each is 0 when its signature
// did not resolve, and only the feature that needs it waits.
struct ExpandedAddresses {
    uintptr_t poseUpload=0;
    uintptr_t menuChoose=0,menuCommit=0,menuDisplay=0,menuRefresh=0,menuResolve=0,menuRelease=0,menuClose=0;
    uintptr_t widgetFind=0,widgetResolve=0,widgetSet=0;
    uintptr_t chooseCaller=0,commitDisplay=0,closeEvent=0,cursorClass=0;
};
struct SignatureResult {
    bool ok=false;
    GameLayout layout{};
    InteractionAddresses interaction{};
    ExpandedAddresses expanded{};
    // The checked Steam 2.06 executable: every 2.06 signature must land on its checked address.
    bool checkedBuild=false;
    size_t found=0,total=0;
    std::string detail;   // the field that failed and why, or a summary
    std::string missing;  // optional 2.06 addresses that did not resolve, and why
};
// Resolves every address in the PE image at 'image': the running game, or
// CrimsonDesert.exe mapped as an image. Values are RVAs.
SignatureResult resolveBySignature(const unsigned char* image);
// For tests: compares a result with the addresses the signatures were made
// from. Returns false with a reason; 'compared' is false for another build.
bool signaturesMatchSource(const SignatureResult& result,bool& compared,std::string& detail);
// Starts lookup on its own thread when the running build is not in the table,
// or when [Development] ForceSignatureLayout=1. Returns at once.
void startGameLayoutResolution(const std::filesystem::path& ini);
// The interaction addresses found by signature, once lookup has succeeded.
const InteractionAddresses* signatureInteraction();
// The 2.06 addresses found by signature, once lookup has succeeded on a 2.06 build.
const ExpandedAddresses* signatureExpanded();
}
