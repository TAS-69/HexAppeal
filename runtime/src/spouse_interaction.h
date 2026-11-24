#pragma once
#include <Windows.h>
#include <cstdint>
namespace hex {
void initializeWardrobeInteraction();
uintptr_t wardrobePreviewActor() noexcept;
void requestWardrobeInteractionClose() noexcept;
void setWardrobePreviewHudHidden(bool hide);
void tickWardrobeInteraction() noexcept;
}
