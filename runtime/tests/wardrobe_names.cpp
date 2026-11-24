#include "wardrobe_items.h"
#include <cstdio>
#include <cstdlib>

static void Check(bool condition, const char* name)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}

int main()
{
    using hex::unlistedPartName;
    Check(unlistedPartName("cd_phw_00_cloak_00_0137", "Cloak", {}) == "Cloak (cd_phw_00_cloak_00_0137)",
          "equipment without a known wearer keeps the slot and part label");
    Check(unlistedPartName("cd_phw_00_ub_00_0164", "Upper body", {"White Crow"}) ==
              "White Crow's default top (cd_phw_00_ub_00_0164)",
          "a witch default garment is named after its wearer");
    Check(unlistedPartName("cd_phw_00_foot_0038", "Feet", {"Bari", "Sylvia"}) ==
              "Default boots of Bari and Sylvia (cd_phw_00_foot_0038)",
          "a garment shared by two witches names both");
    Check(unlistedPartName("cd_phw_00_foot_0038", "Feet", {"A", "B", "C"}) ==
              "Default boots of A, B and C (cd_phw_00_foot_0038)",
          "three wearers are listed in order");
    Check(unlistedPartName("ha_phw_00_foot_00_0160", "Feet", {"Elowen"}, "White Bloodwind Leather Boots") ==
              "White Bloodwind Leather Boots (witch copy)",
          "a witch copy takes the name of the item it duplicates");
    Check(unlistedPartName("cd_phw_00_xx_0001", "", {"Bari"}) == " (cd_phw_00_xx_0001)",
          "an unknown slot keeps the original label shape");
    std::puts("PASS: unnamed mounted equipment gets wearer or source item names.");
    return 0;
}
