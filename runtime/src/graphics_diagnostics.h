#pragma once

#include <Windows.h>
#include <filesystem>
#include <string>

struct ID3D12Device;

namespace hex {
struct GraphicsReportSource {
    unsigned width = 0;
    unsigned height = 0;
    ID3D12Device* device = nullptr;
};
void setGraphicsSettingsPath(const std::filesystem::path& path);
std::string graphicsSummary(const GraphicsReportSource& source = {});
}
