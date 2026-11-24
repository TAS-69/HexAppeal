#pragma once

#include <filesystem>

namespace ml::Log
{

    void OpenSession(const std::filesystem::path& path);
    void Write(const char* level, const char* fmt, ...);
    // Record when the game creates a swapchain or starts presenting frames.
    void MarkStartupComplete();
    bool PreviousStartupIncomplete();
}

#define LOG(...)     ::ml::Log::Write("info ", __VA_ARGS__)
#define LOG_OK(...)  ::ml::Log::Write("ok   ", __VA_ARGS__)
#define LOG_ERR(...) ::ml::Log::Write("error", __VA_ARGS__)
