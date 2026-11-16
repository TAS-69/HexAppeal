#pragma once
#include <atomic>
#include <Windows.h>

namespace ml
{


    inline constexpr ULONG_PTR shortcutInputTag = 0x4D414354;



    struct State
    {
        std::atomic<bool> sendingShortcut{false};
        std::atomic<bool> menuOpen{false};
        std::atomic<bool> mouseActive{false};
        std::atomic<bool> textEditing{false};
        std::atomic<bool> blockGameInput{true};
        std::atomic<float> panelLeft{0},panelTop{0},panelRight{0},panelBottom{0};
        std::atomic<DWORD> renderTid{0};


        std::atomic<unsigned> padShortcut{0};




        std::atomic<DWORD> lastDrawAt{0};










        bool Visible() const
        {
            if (!menuOpen) return false;
            const DWORD drawn = lastDrawAt.load();
            return drawn && (GetTickCount() - drawn) < 500;
        }
        bool PointerOver(float x,float y) const {
            return Visible()&&x>=panelLeft&&x<panelRight&&y>=panelTop&&y<panelBottom;
        }
        bool BlocksGameInput() const {return Visible()&&blockGameInput;}
        bool CapturesKeyboard() const {return Visible()&&(blockGameInput||textEditing);}
        bool Captures() const {return Visible()&&(blockGameInput||mouseActive);}



        static bool ForegroundIsOurs()
        {
            DWORD pid = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &pid);
            return pid == GetCurrentProcessId();
        }

        static State& Get()
        {
            static State s;
            return s;
        }
    };
}
