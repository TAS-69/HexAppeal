#pragma once
#include <string>
namespace hex {
inline bool verifiedCreatorVersion(const std::string& digest){
    return digest=="de9b8b3acf79dce35d219cc9daf2f1daf7f6e482950cab389fc2dd4b8f236b32"||
        digest=="9d78133641350e0776667efba765538c72cf37620a69f58cd8d9f8c44843f42b"||
        digest=="331b0e06c89b347bfd08f83e88e83c2f8ba02c7c358dee36199410dd91cffe71";
}
}
