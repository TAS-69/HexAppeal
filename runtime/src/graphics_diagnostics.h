#pragma once

#include <Windows.h>
#include <filesystem>
#include <string>

namespace hex {
void setGraphicsSettingsPath(const std::filesystem::path& path);
void observeGraphicsSwapchain(unsigned width, unsigned height, const LUID* adapter = nullptr);
// The loaded-library list reads each graphics DLL's file version, so only
// saved reports include it; the automatic log passes false.
std::string graphicsSummary(bool includeLibraries = true);
}
