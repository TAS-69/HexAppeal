#pragma once

namespace ml
{

    struct Config
    {
        int wrapSwapChain = -1;
        bool enableDred = false;
        bool hookXInput = true;
        bool debugInput = false;
    };
    namespace Settings
    {
        inline Config& Get() { static Config c; return c; }
    }
}
