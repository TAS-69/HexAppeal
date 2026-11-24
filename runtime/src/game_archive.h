#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace hex {
uint32_t gameArchiveNameHash(std::string_view name);
std::string readLooseGameAsset(const std::filesystem::path& path);

class GameArchive {
public:
    explicit GameArchive(const std::filesystem::path& directory, std::string_view prefix={});
    std::vector<std::string_view> routes() const;
    std::string read(const std::string& route) const;
private:
    struct Entry {uint32_t chunk,offset,stored,size,compression;};
    std::filesystem::path directory;
    std::map<std::string,Entry> entries;
};
}
