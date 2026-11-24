#pragma once

namespace ml::gui
{

    void InitStyle(float displayHeight);
    void SetDisplayHeight(float height);

    void PollToggle();
    bool RequestSpouseWardrobe(int witch) noexcept;
    void CancelNativeWardrobe() noexcept;
    void OverlayInterrupted() noexcept;
    void NoteNativeWardrobeDrawn() noexcept;

    bool WantsDraw();



    void FeedInput();

    void Render();
}
