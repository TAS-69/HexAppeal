#pragma once
#include <array>
#include <string>
namespace hex {
struct VenusBodyVersion {const char* version;const char* variant;const char* sha256;};
inline constexpr std::array<VenusBodyVersion,2> venusBodyVersions={{
    {"2.6","Elf Body","8f101b8cc7d5c3af0b1ba2d38a6c2bfbbadecd733bd39f9491f1bf6a654466eb"},
    {"2.6","Human Body","f8c7f181d42b50e441251553630b708f9780ef54c319464db4ed4a17af6189f1"}
}};
inline const VenusBodyVersion* venusBodyVersion(const std::string& digest){for(const auto& body:venusBodyVersions)if(digest==body.sha256)return &body;return nullptr;}
}
