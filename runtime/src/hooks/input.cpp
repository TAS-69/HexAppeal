#include "input.h"

#include <MinHook.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <vector>
#include <intrin.h>

#include "../core/log.h"
#include "../core/settings.h"
#include "../core/state.h"


extern ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM, LPARAM);

namespace ml::input
{
    static WNDPROC g_original = nullptr;
    static HWND    g_hwnd     = nullptr;


    static CRITICAL_SECTION g_cs;
    static bool  g_csReady = false;
    static float g_vx = 0, g_vy = 0;
    static bool  g_rawButtons = false;
    enum class InputKind { Key, Character, MouseButton, Wheel, Focus };
    struct PendingInput
    {
        InputKind kind;
        WPARAM value;
        LPARAM data;
        unsigned modifiers = 0;
        bool down = false;
    };
    static std::array<PendingInput, 512> g_pending;
    static size_t g_pendingCount = 0;
    static bool g_inputOverflow = false;
    static bool g_acceptInput = false;

    static unsigned g_legacyButtons = 0;
    static bool g_ownsCapture = false;
    static bool  g_weRegistered = false;
    static std::atomic<float> g_ovW{0.f},g_ovH{0.f};
    static unsigned g_worldButtons=0,g_menuButtons=0;
    static bool PointerCapturedLocked();
    static uintptr_t g_selfBegin=0,g_selfEnd=0;
    using KeyStateFn=SHORT(WINAPI*)(int);
    using KeyboardStateFn=BOOL(WINAPI*)(PBYTE);
    static KeyStateFn oAsyncKeyState=nullptr,oKeyState=nullptr;
    static KeyboardStateFn oKeyboardState=nullptr;
    static thread_local bool g_pollingBypass=false;
#ifdef MODACTIVATOR_TESTS
    static bool g_testForeground=false;
#endif
    static bool BlockPolledInput(const void* caller,bool keyboard) {
        const auto address=reinterpret_cast<uintptr_t>(caller);
        if(g_pollingBypass||(address>=g_selfBegin&&address<g_selfEnd))return false;
        const auto& state=State::Get();if(!state.Visible())return false;
        bool foreground=State::ForegroundIsOurs();
#ifdef MODACTIVATOR_TESTS
        foreground=foreground||g_testForeground;
#endif
        if(!foreground)return false;
        if(keyboard)return state.CapturesKeyboard();
        if(state.BlocksGameInput())return true;
        if(!g_csReady)return false;EnterCriticalSection(&g_cs);const bool captured=PointerCapturedLocked();LeaveCriticalSection(&g_cs);return captured;
    }
    static SHORT ReadPolledKey(KeyStateFn original,int key,const void* caller) {
        const bool block=BlockPolledInput(caller,key<VK_LBUTTON||key>VK_XBUTTON2);
        const bool before=g_pollingBypass;g_pollingBypass=true;const auto value=original(key);g_pollingBypass=before;
        return block?SHORT(0):value;
    }
    static SHORT WINAPI hkAsyncKeyState(int key){return ReadPolledKey(oAsyncKeyState,key,_ReturnAddress());}
    static SHORT WINAPI hkKeyState(int key){return ReadPolledKey(oKeyState,key,_ReturnAddress());}
    static BOOL WINAPI hkKeyboardState(PBYTE keys){
        const bool block=BlockPolledInput(_ReturnAddress(),true),before=g_pollingBypass;
        g_pollingBypass=true;const auto result=oKeyboardState(keys);g_pollingBypass=before;
        if(result&&keys&&block)ZeroMemory(keys,256);return result;
    }
    SHORT AsyncKeyRaw(int key){const bool before=g_pollingBypass;g_pollingBypass=true;const auto result=oAsyncKeyState?oAsyncKeyState(key):GetAsyncKeyState(key);g_pollingBypass=before;return result;}
    static bool PointerCapturedLocked() {
        if(State::Get().BlocksGameInput())return true;
        if(g_menuButtons)return true;
        if(g_worldButtons)return false;
        int w,h; RECT rc{};
        if(!g_hwnd||!GetClientRect(g_hwnd,&rc))return false;
        w=rc.right-rc.left;h=rc.bottom-rc.top;
        const float x=g_vx*(g_ovW>0&&w>0?g_ovW/w:1.f);
        const float y=g_vy*(g_ovH>0&&h>0?g_ovH/h:1.f);
        return State::Get().Captures()||State::Get().PointerOver(x,y);
    }





    typedef BOOL (WINAPI *FnGetCursorPos)(LPPOINT);
    static FnGetCursorPos oGetCursorPos = nullptr;
    static volatile LONG g_dbgInput  = 0;
    static volatile LONG g_dbgMove   = 0;
    static volatile LONG g_dbgButton = 0;
    static bool    g_gcpHooked = false;
    static int     g_dbgLines  = 3;
    static DWORD   g_dbgNext   = 0;
    static WNDPROC g_ours      = nullptr;

    static void Lock()   { EnterCriticalSection(&g_cs); }
    static void Unlock() { LeaveCriticalSection(&g_cs); }

    static void PushInputLocked(const PendingInput& event)
    {
        if (!g_acceptInput)
            return;
        if (g_pendingCount == g_pending.size())
        {
            g_inputOverflow = true;
            return;
        }
        g_pending[g_pendingCount++] = event;
    }

    static void QueueInput(const PendingInput& event)
    {
        Lock();
        PushInputLocked(event);
        Unlock();
    }

    static void ResetInputLocked(bool accepting)
    {
        g_acceptInput = accepting;
        g_pendingCount = 0;
        g_inputOverflow = false;
    }

    static void SubmitInput(ImGuiIO& io, const PendingInput& event)
    {
        switch (event.kind)
        {
        case InputKind::Key:
        {
            io.AddKeyEvent(ImGuiMod_Ctrl, (event.modifiers & 1) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (event.modifiers & 2) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (event.modifiers & 4) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (event.modifiers & 8) != 0);
            const auto key = ImGui_ImplWin32_KeyEventToImGuiKey(event.value, event.data);
            if (key == ImGuiKey_None)
                break;

            if (key == ImGuiKey_PrintScreen && !event.down)
                io.AddKeyEvent(key, true);
            io.AddKeyEvent(key, event.down);
            io.SetKeyEventNativeData(key, static_cast<int>(event.value),
                                    LOBYTE(HIWORD(event.data)));
            break;
        }
        case InputKind::Character:
            io.AddInputCharacterUTF16(static_cast<unsigned short>(event.value));
            break;
        case InputKind::MouseButton:
            io.AddMouseButtonEvent(static_cast<int>(event.value), event.down);
            break;
        case InputKind::Wheel:
        {
            const float delta = static_cast<short>(event.value) / static_cast<float>(WHEEL_DELTA);
            io.AddMouseWheelEvent(event.data ? -delta : 0, event.data ? 0 : delta);
            break;
        }
        case InputKind::Focus:
            io.AddFocusEvent(event.down);
            break;
        }
    }


    void SetOverlaySize(float w, float h) { g_ovW = w; g_ovH = h; }

    static void ClientSize(int* w, int* h)
    {
        RECT rc = {};
        if (g_hwnd && GetClientRect(g_hwnd, &rc)) { *w = rc.right - rc.left; *h = rc.bottom - rc.top; }
        else { *w = 1920; *h = 1080; }
    }

    static bool OnRawInput(HRAWINPUT h)
    {
        UINT size = 0;
        if (GetRawInputData(h, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0 || size > 1024) return false;
        alignas(8) unsigned char buf[1024];
        if (GetRawInputData(h, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != size) return false;
        const RAWINPUT* ri = reinterpret_cast<const RAWINPUT*>(buf);
        if (ri->header.dwType == RIM_TYPEKEYBOARD) return State::Get().CapturesKeyboard();
        if (ri->header.dwType != RIM_TYPEMOUSE) return false;
        const RAWMOUSE& m = ri->data.mouse;
        int w, hgt; ClientSize(&w, &hgt);
        Lock();
        if (m.usFlags & MOUSE_MOVE_ABSOLUTE)
        {

            const bool virt = (m.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
            const int sw = GetSystemMetrics(virt ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
            const int sh = GetSystemMetrics(virt ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
            POINT p = { static_cast<LONG>(m.lLastX * sw / 65535.0), static_cast<LONG>(m.lLastY * sh / 65535.0) };
            ScreenToClient(g_hwnd, &p);
            g_vx = static_cast<float>(p.x); g_vy = static_cast<float>(p.y);
        }
        else
        {
            g_vx += static_cast<float>(m.lLastX);
            g_vy += static_cast<float>(m.lLastY);
        }
        if (g_vx < 0) g_vx = 0; if (g_vy < 0) g_vy = 0;
        if (g_vx > w - 1) g_vx = static_cast<float>(w - 1);
        if (g_vy > hgt - 1) g_vy = static_cast<float>(hgt - 1);
        const bool capturing=PointerCapturedLocked();
        static const USHORT downs[5]={RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_5_DOWN};
        static const USHORT ups[5]={RI_MOUSE_LEFT_BUTTON_UP,RI_MOUSE_RIGHT_BUTTON_UP,RI_MOUSE_MIDDLE_BUTTON_UP,RI_MOUSE_BUTTON_4_UP,RI_MOUSE_BUTTON_5_UP};
        for(int b=0;b<5;++b) {
            if(m.usButtonFlags&downs[b]){if(capturing)g_menuButtons|=1u<<b;else g_worldButtons|=1u<<b;}
            if(m.usButtonFlags&ups[b]){g_menuButtons&=~(1u<<b);g_worldButtons&=~(1u<<b);}
        }
        if (g_rawButtons)
        {
            static const USHORT down[5] = { RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_5_DOWN };
            static const USHORT up[5]   = { RI_MOUSE_LEFT_BUTTON_UP,   RI_MOUSE_RIGHT_BUTTON_UP,   RI_MOUSE_MIDDLE_BUTTON_UP,   RI_MOUSE_BUTTON_4_UP,   RI_MOUSE_BUTTON_5_UP };
            for (int b = 0; b < 5; ++b)
            {
                if (capturing && (m.usButtonFlags & down[b]))
                    PushInputLocked({InputKind::MouseButton, static_cast<WPARAM>(b), 0, 0, true});
                if (m.usButtonFlags & up[b])
                    PushInputLocked({InputKind::MouseButton, static_cast<WPARAM>(b), 0});
            }
            if (capturing && (m.usButtonFlags & RI_MOUSE_WHEEL))
                PushInputLocked({InputKind::Wheel, m.usButtonData, 0});
            if (capturing && (m.usButtonFlags & RI_MOUSE_HWHEEL))
                PushInputLocked({InputKind::Wheel, m.usButtonData, 1});
        }
        Unlock();
        return capturing;
    }

    void FeedInput(ImGuiIO& io)
    {
        if (!g_csReady) return;
        int cw = 0, chh = 0;
        ClientSize(&cw, &chh);
        const float sx = (g_ovW > 0.0f && cw > 0) ? g_ovW / static_cast<float>(cw) : 1.0f;
        const float sy = (g_ovH > 0.0f && chh > 0) ? g_ovH / static_cast<float>(chh) : 1.0f;
        std::array<PendingInput, g_pending.size()> batch;
        Lock();
        const float vx = g_vx, vy = g_vy;
        const bool overflow = g_inputOverflow;
        const auto count = overflow ? 0 : g_pendingCount;
        std::copy_n(g_pending.begin(), count, batch.begin());
        ResetInputLocked(g_acceptInput);
        Unlock();


        if (overflow)
        {
            io.ClearEventsQueue();
            io.ClearInputKeys();
            io.ClearInputMouse();
        }
        io.AddMousePosEvent(vx * sx, vy * sy);
        for (size_t i = 0; i < count; ++i)
            SubmitInput(io, batch[i]);





        const DWORD now = GetTickCount();
        if (g_dbgLines < 3 && now >= g_dbgNext)
        {
            g_dbgNext = now + 1000;
            ++g_dbgLines;
            POINT os = {};
            if (oGetCursorPos) oGetCursorPos(&os);
            LOG("[input] cursor: %ld raw moves, %ld legacy moves, %ld button messages | virtual %.0f,%.0f fed as %.0f,%.0f | ImGui %.0f,%.0f of %.0fx%.0f | client %dx%d | OS cursor %ld,%ld",
                InterlockedExchange(&g_dbgInput, 0), InterlockedExchange(&g_dbgMove, 0), InterlockedExchange(&g_dbgButton, 0),
                vx, vy, vx * sx, vy * sy, io.MousePos.x, io.MousePos.y, io.DisplaySize.x, io.DisplaySize.y, cw, chh, os.x, os.y);
        }
    }




    static BOOL WINAPI hkGetCursorPos(LPPOINT p)
    {
        const State& st = State::Get();
        if (p && st.Visible() && st.renderTid && GetCurrentThreadId() == st.renderTid && g_hwnd)
        {
            Lock();
            POINT c = { static_cast<LONG>(g_vx), static_cast<LONG>(g_vy) };
            Unlock();
            ClientToScreen(g_hwnd, &c);
            *p = c;
            return TRUE;
        }
        return oGetCursorPos(p);
    }

    static void LogRawTarget(const char* when, HWND target)
    {
        if (!target)
        {
            LOG("[input] %s raw mouse input has no target window, so it follows keyboard focus.", when);
            return;
        }
        if (target == g_hwnd)
        {
            LOG("[input] %s raw mouse input goes to the window we subclassed (%p), so WM_INPUT reaches the menu.", when, (void*)g_hwnd);
            return;
        }
        char cls[64] = "";
        GetClassNameA(target, cls, sizeof cls);
        DWORD pid = 0;
        GetWindowThreadProcessId(target, &pid);
        LOG_ERR("[input] %s raw mouse input goes to %p (class \"%s\", pid %lu) and not to the window we subclassed (%p). WM_INPUT never reaches the menu, so its cursor cannot move.",
                when, (void*)target, cls, pid, (void*)g_hwnd);
    }

    static void EnsureRawInput()
    {


        UINT n = 0;
        GetRegisteredRawInputDevices(nullptr, &n, sizeof(RAWINPUTDEVICE));
        std::vector<RAWINPUTDEVICE> devs(n);
        if (n) GetRegisteredRawInputDevices(devs.data(), &n, sizeof(RAWINPUTDEVICE));
        for (UINT i = 0; i < n; ++i)
        {
            if (devs[i].usUsagePage == 0x01 && devs[i].usUsage == 0x02)
            {
                g_rawButtons = (devs[i].dwFlags & RIDEV_NOLEGACY) != 0;
                LOG("[input] game registered raw mouse input (flags 0x%X)%s", devs[i].dwFlags, g_rawButtons ? ", no legacy button messages" : "");
                LogRawTarget("at startup", devs[i].hwndTarget);
                return;
            }
        }
        RAWINPUTDEVICE rid = { 0x01, 0x02, 0, g_hwnd };
        g_weRegistered = RegisterRawInputDevices(&rid, 1, sizeof rid) != 0;
        LOG("[input] raw mouse input %s", g_weRegistered ? "registered for the menu cursor" : "registration FAILED; the menu cursor will not move");
    }




    static void ReportOnOpen(int w, int h)
    {
        InterlockedExchange(&g_dbgInput, 0);
        InterlockedExchange(&g_dbgMove, 0);
        InterlockedExchange(&g_dbgButton, 0);
        g_dbgLines = 0;
        g_dbgNext  = GetTickCount() + 1000;

        RECT wr = {};
        GetWindowRect(g_hwnd, &wr);
        LOG("[input] menu opened. Window %p, client %dx%d, frame at %ld,%ld size %ldx%ld, unicode %d.",
            (void*)g_hwnd, w, h, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top, IsWindowUnicode(g_hwnd) ? 1 : 0);

        const HWND fg = GetForegroundWindow();
        LOG("[input] foreground %p%s, focus %p, active %p.",
            (void*)fg, fg == g_hwnd ? " (ours)" : " (not the window we subclassed)", (void*)GetFocus(), (void*)GetActiveWindow());

        const WNDPROC live = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(g_hwnd, GWLP_WNDPROC));
        if (live == g_ours)
            LOG("[input] our window procedure is still the outermost one.");
        else
        {
            HMODULE owner = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(live), &owner);
            wchar_t path[MAX_PATH] = L"";
            if (owner) GetModuleFileNameW(owner, path, MAX_PATH);
            LOG("[input] something subclassed the window after us: procedure %p in %S. Messages reach us only if it passes them on.",
                (void*)live, path[0] ? path : L"an unknown module");
        }

        UINT n = 0;
        GetRegisteredRawInputDevices(nullptr, &n, sizeof(RAWINPUTDEVICE));
        std::vector<RAWINPUTDEVICE> devs(n);
        if (n) GetRegisteredRawInputDevices(devs.data(), &n, sizeof(RAWINPUTDEVICE));
        bool found = false;
        for (UINT i = 0; i < n; ++i)
            if (devs[i].usUsagePage == 0x01 && devs[i].usUsage == 0x02)
            {
                found = true;
                LOG("[input] raw mouse registration right now: flags 0x%X.", devs[i].dwFlags);
                LogRawTarget("right now", devs[i].hwndTarget);
            }
        if (!found)
            LOG_ERR("[input] nothing in this process is registered for raw mouse input any more, so the menu cursor cannot move.");

        POINT os = {};
        if (oGetCursorPos) oGetCursorPos(&os);
        POINT osc = os;
        ScreenToClient(g_hwnd, &osc);
        RECT clip = {};
        GetClipCursor(&clip);
        Lock();
        const float vx = g_vx, vy = g_vy;
        Unlock();
        LOG("[input] GetCursorPos detour %s. OS cursor screen %ld,%ld client %ld,%ld, clipped to %ld,%ld..%ld,%ld. Virtual cursor starts at %.0f,%.0f.",
            g_gcpHooked ? "installed" : "MISSING, so ImGui polls the real cursor and fights the virtual one",
            os.x, os.y, osc.x, osc.y, clip.left, clip.top, clip.right, clip.bottom, vx, vy);
    }

    void InputModeChanged(){
        if(!g_hwnd||!State::Get().menuOpen||!State::Get().blockGameInput)return;
        for(int key=VK_BACK;key<255;++key)if(AsyncKeyRaw(key)&0x8000)PostMessageW(g_hwnd,WM_KEYUP,WPARAM(key),LPARAM(0xC0000001u|(MapVirtualKeyW(UINT(key),MAPVK_VK_TO_VSC)<<16)));
        const int buttons[]={VK_LBUTTON,VK_RBUTTON,VK_MBUTTON};const UINT messages[]={WM_LBUTTONUP,WM_RBUTTONUP,WM_MBUTTONUP};
        for(int i=0;i<3;++i)if(AsyncKeyRaw(buttons[i])&0x8000)PostMessageW(g_hwnd,messages[i],0,0);
    }
    void MenuOpened()
    {
        if (!g_csReady) return;
        int w, h; ClientSize(&w, &h);
        Lock();
        g_vx = w * 0.5f; g_vy = h * 0.5f;
        ResetInputLocked(true);
        g_worldButtons=0;g_menuButtons=0;
        const int buttons[]={VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2};
        for(int b=0;b<5;++b)if(GetAsyncKeyState(buttons[b])&0x8000)g_worldButtons|=1u<<b;
        Unlock();
        InputModeChanged();


        if (Settings::Get().debugInput)
            ReportOnOpen(w, h);
    }
    void MenuClosed()
    {
        if (!g_csReady) return;
        Lock();
        ResetInputLocked(false);
        g_worldButtons=0;g_menuButtons=0;
        State::Get().mouseActive=false;State::Get().textEditing=false;
        Unlock();
    }


    static bool IsMouse(UINT m)    { return m >= WM_MOUSEFIRST && m <= WM_MOUSELAST; }
    static bool IsKeyboard(UINT m) { return m == WM_KEYDOWN || m == WM_KEYUP || m == WM_SYSKEYDOWN || m == WM_SYSKEYUP || m == WM_CHAR || m == WM_SYSCHAR; }

    static bool KeyDown(int key)
    {
        return (GetKeyState(key) & 0x8000) != 0;
    }

    static void QueueKeyboard(HWND hwnd, UINT msg, WPARAM key, LPARAM data)
    {
        if (msg == WM_CHAR)
        {
            if (!IsWindowUnicode(hwnd))
            {
                wchar_t decoded = 0;
                const char byte = static_cast<char>(key);
                const auto layout = reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(0));
                DWORD page = CP_ACP;
                GetLocaleInfoW(MAKELCID(LOWORD(layout), SORT_DEFAULT),
                               LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                               reinterpret_cast<wchar_t*>(&page), sizeof(page) / sizeof(wchar_t));
                if (MultiByteToWideChar(page, 0, &byte, 1, &decoded, 1) != 1)
                    return;
                key = decoded;
            }
            if (key > 0 && key < 0x10000)
                QueueInput({InputKind::Character, key, 0});
            return;
        }
        if (msg == WM_SYSCHAR || key >= 256)
            return;

        const unsigned mods = (KeyDown(VK_CONTROL) ? 1u : 0u) |
                              (KeyDown(VK_SHIFT) ? 2u : 0u) |
                              (KeyDown(VK_MENU) ? 4u : 0u) |
                              ((KeyDown(VK_LWIN) || KeyDown(VK_RWIN)) ? 8u : 0u);
        const bool extended = (HIWORD(data) & KF_EXTENDED) != 0;
        if (key == VK_SHIFT)
            key = MapVirtualKeyW(LOBYTE(HIWORD(data)), MAPVK_VSC_TO_VK_EX);
        else if (key == VK_CONTROL)
            key = extended ? VK_RCONTROL : VK_LCONTROL;
        else if (key == VK_MENU)
            key = extended ? VK_RMENU : VK_LMENU;
        QueueInput({InputKind::Key, key, data, mods, msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN});
    }

    static void QueueLegacyMouse(HWND hwnd, UINT msg, WPARAM wParam, bool capturing)
    {
        if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL)
        {
            if (capturing)
                QueueInput({InputKind::Wheel, HIWORD(wParam), msg == WM_MOUSEHWHEEL});
            return;
        }
        int button;
        switch (msg)
        {
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_LBUTTONUP: button = 0; break;
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: case WM_RBUTTONUP: button = 1; break;
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: case WM_MBUTTONUP: button = 2; break;
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: case WM_XBUTTONUP:
            button = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 3 : 4;
            break;
        default: return;
        }
        const bool down = msg != WM_LBUTTONUP && msg != WM_RBUTTONUP &&
                          msg != WM_MBUTTONUP && msg != WM_XBUTTONUP;
        if (down && capturing)
        {
            g_legacyButtons |= 1u << button;
            if (!GetCapture())
            {
                SetCapture(hwnd);
                g_ownsCapture = GetCapture() == hwnd;
            }
        }
        else if (!down)
        {
            g_legacyButtons &= ~(1u << button);
            if (!g_legacyButtons && g_ownsCapture)
            {
                g_ownsCapture = false;
                if (GetCapture() == hwnd)
                    ReleaseCapture();
            }
        }
        if (capturing||!down)
            QueueInput({InputKind::MouseButton, static_cast<WPARAM>(button), 0, 0, down});
    }

#ifdef MODACTIVATOR_TESTS
    static unsigned g_testForwarded=0;
    static LRESULT CALLBACK FixtureWindowProc(HWND,UINT,WPARAM,LPARAM){++g_testForwarded;return 9;}
#endif
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_INPUT)          InterlockedIncrement(&g_dbgInput);
        else if (msg == WM_MOUSEMOVE) InterlockedIncrement(&g_dbgMove);
        else if (IsMouse(msg))        InterlockedIncrement(&g_dbgButton);
        bool foreground=State::ForegroundIsOurs();
#ifdef MODACTIVATOR_TESTS
        foreground=foreground||g_testForeground;
#endif
        const bool visible=State::Get().Visible()&&foreground;
        bool capturing=false;
        if(visible&&g_csReady){Lock();capturing=PointerCapturedLocked();Unlock();}
        if (msg == WM_CAPTURECHANGED)
        {
            for (int button = 0; button < 5; ++button)
                if (g_legacyButtons & (1u << button))
                    QueueInput({InputKind::MouseButton, static_cast<WPARAM>(button), 0});
            g_legacyButtons = 0;
            g_ownsCapture = false;
        }
        if (visible && (msg == WM_SETFOCUS || msg == WM_KILLFOCUS))
            QueueInput({InputKind::Focus, 0, 0, 0, msg == WM_SETFOCUS});
        if (visible && !g_rawButtons && IsMouse(msg))
            QueueLegacyMouse(hwnd, msg, wParam, capturing);
        if (visible&&msg==WM_INPUT&&GET_RAWINPUT_CODE_WPARAM(wParam)==RIM_INPUT)
        {
            const bool mouseOwned=OnRawInput(reinterpret_cast<HRAWINPUT>(lParam));
            if(mouseOwned||g_weRegistered||State::Get().BlocksGameInput())return DefWindowProcW(hwnd,msg,wParam,lParam);
            return CallWindowProcW(g_original,hwnd,msg,wParam,lParam);
        }
        if(visible&&IsKeyboard(msg)) {
            if(static_cast<ULONG_PTR>(GetMessageExtraInfo())==shortcutInputTag)
                return CallWindowProcW(g_original,hwnd,msg,wParam,lParam);
            if(State::Get().CapturesKeyboard()) {
                QueueKeyboard(hwnd,msg,wParam,lParam);
                if(msg!=WM_KEYUP&&msg!=WM_SYSKEYUP)return 0;
            }
            return CallWindowProcW(g_original,hwnd,msg,wParam,lParam);
        }
        if (visible&&capturing)
        {
            if (msg == WM_SETCURSOR) { SetCursor(nullptr); return TRUE; }
            if (msg == WM_MOUSEMOVE || msg == WM_NCMOUSEMOVE || msg == WM_MOUSELEAVE || msg == WM_NCMOUSELEAVE)
                return 0;
            if (IsMouse(msg)&&msg!=WM_LBUTTONUP&&msg!=WM_RBUTTONUP&&msg!=WM_MBUTTONUP&&msg!=WM_XBUTTONUP)
                return 0;
        }
        else if (msg == WM_INPUT && g_weRegistered)
        {

            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        return CallWindowProcW(g_original, hwnd, msg, wParam, lParam);
    }

    void Init(HWND hwnd)
    {
        if (g_original) return;
        if (!g_csReady) { InitializeCriticalSection(&g_cs); g_csReady = true; }
        g_hwnd = hwnd;
        g_ours = &WndProc;




        g_original = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
        const auto replaced = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
        if (replaced && replaced != g_original)
            g_original = replaced;
        EnsureRawInput();
        if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
        {
            HMODULE self=nullptr;
            if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&Init),&self)){
                const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(self);
                const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const unsigned char*>(self)+dos->e_lfanew);
                g_selfBegin=reinterpret_cast<uintptr_t>(self);g_selfEnd=g_selfBegin+nt->OptionalHeader.SizeOfImage;
            }
            const auto hook=[&](const char* name,void* detour,void** original){if(!g_selfBegin||g_selfEnd<=g_selfBegin){LOG_ERR("[input] %s filter skipped: menu hotkey bypass could not be established.",name);return;}const auto address=reinterpret_cast<void*>(GetProcAddress(user32,name));if(!address||MH_CreateHook(address,detour,original)!=MH_OK||MH_EnableHook(address)!=MH_OK)LOG_ERR("[input] %s polling filter unavailable; live input blocking needs validation.",name);};
            hook("GetAsyncKeyState",reinterpret_cast<void*>(&hkAsyncKeyState),reinterpret_cast<void**>(&oAsyncKeyState));
            hook("GetKeyState",reinterpret_cast<void*>(&hkKeyState),reinterpret_cast<void**>(&oKeyState));
            hook("GetKeyboardState",reinterpret_cast<void*>(&hkKeyboardState),reinterpret_cast<void**>(&oKeyboardState));
            void* target = reinterpret_cast<void*>(GetProcAddress(user32, "GetCursorPos"));
            if (!target || MH_CreateHook(target, reinterpret_cast<void*>(&hkGetCursorPos), reinterpret_cast<void**>(&oGetCursorPos)) != MH_OK || MH_EnableHook(target) != MH_OK)
                LOG_ERR("[input] could not hook GetCursorPos; the menu cursor may jump");
            else
                g_gcpHooked = true;
        }
        if (State::Get().menuOpen) MenuOpened();
    }

#ifdef MODACTIVATOR_TESTS
    extern "C" __declspec(dllexport) int ModActivatorInputQueueTest()
    {
        if (g_csReady)
            return -1;
        InitializeCriticalSection(&g_cs);
        g_csReady = true;
        auto *context = ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        g_dbgLines = 3;
        ResetInputLocked(true);

        const HANDLE producer = CreateThread(nullptr, 0, [](void*) -> DWORD {
            QueueInput({InputKind::Key, 'A', 0, 7, true});
            QueueInput({InputKind::Character, 0xF6, 0});
            QueueInput({InputKind::MouseButton, 0, 0, 0, true});
            QueueInput({InputKind::MouseButton, 0, 0});
            QueueInput({InputKind::Wheel, WHEEL_DELTA, 0});
            QueueInput({InputKind::Key, 'A', 0, 7, false});
            return 0;
        }, nullptr, 0, nullptr);

        int result = 0;
        if (!producer)
        {
            result = 1;
        }
        else
        {
            WaitForSingleObject(producer, INFINITE);
            CloseHandle(producer);
            if (context->InputEventsQueue.Size != 0)
                result = 2;
            FeedInput(io);

            std::vector<int> received;
            for (const auto &event : context->InputEventsQueue)
            {
                if (event.Type == ImGuiInputEventType_Key && event.Key.Down)
                    received.push_back(event.Key.Key);
                else if (event.Type == ImGuiInputEventType_Key && event.Key.Key == ImGuiKey_A)
                    received.push_back(-ImGuiKey_A);
                else if (event.Type == ImGuiInputEventType_Text)
                    received.push_back(event.Text.Char);
                else if (event.Type == ImGuiInputEventType_MouseButton)
                    received.push_back(event.MouseButton.Down ? 1 : -1);
                else if (event.Type == ImGuiInputEventType_MouseWheel)
                    received.push_back(static_cast<int>(event.MouseWheel.WheelY));
            }
            const std::vector<int> expected{ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiMod_Alt,
                                            ImGuiKey_A, 0xF6, 1, -1, 1, -ImGuiKey_A};
            if (!result && received != expected)
                result = 3;
        }

        MenuClosed();
        QueueInput({InputKind::Character, 'x', 0});
        if (!result && g_pendingCount)
            result = 4;
        ResetInputLocked(true);
        for (size_t i = 0; i <= g_pending.size(); ++i)
            QueueInput({InputKind::Character, 'x', 0});
        FeedInput(io);
        if (!result && (g_inputOverflow || g_pendingCount ||
            std::any_of(context->InputEventsQueue.begin(), context->InputEventsQueue.end(),
                        [](const ImGuiInputEvent &e) { return e.Type == ImGuiInputEventType_Text; })))
            result = 5;

        auto& state=State::Get();state.menuOpen=true;state.lastDrawAt=GetTickCount();state.textEditing=false;state.mouseActive=true;state.blockGameInput=false;
        g_testForeground=true;g_original=FixtureWindowProc;g_testForwarded=0;
        WndProc(nullptr,WM_KEYDOWN,'W',0);WndProc(nullptr,WM_KEYDOWN,'R',0);WndProc(nullptr,WM_KEYDOWN,VK_CONTROL,0);
        if(!result&&g_testForwarded!=3)result=6;
        state.textEditing=true;WndProc(nullptr,WM_KEYDOWN,'5',0);
        if(!result&&g_testForwarded!=3)result=7;
        WndProc(nullptr,WM_KEYUP,'5',0);if(!result&&g_testForwarded!=4)result=8;
        state.lastDrawAt=GetTickCount()-501;WndProc(nullptr,WM_KEYDOWN,'W',0);
        if(!result&&g_testForwarded!=5)result=9;
        g_menuButtons=1;if(!result&&!PointerCapturedLocked())result=10;
        g_menuButtons=0;g_worldButtons=1;if(!result&&PointerCapturedLocked())result=11;
        g_worldButtons=0;g_testForeground=false;g_original=nullptr;state.menuOpen=false;state.textEditing=false;state.mouseActive=false;
        state.menuOpen=true;state.lastDrawAt=GetTickCount();state.blockGameInput=true;g_testForeground=true;
        g_worldButtons=1;if(!result&&!PointerCapturedLocked())result=12;g_worldButtons=0;
        g_original=FixtureWindowProc;g_testForwarded=0;
        WndProc(nullptr,WM_KEYDOWN,'W',0);WndProc(nullptr,WM_KEYDOWN,'R',0);if(!result&&g_testForwarded)result=13;
        WndProc(nullptr,WM_KEYUP,'W',0);if(!result&&g_testForwarded!=1)result=14;
        const auto foreign=reinterpret_cast<const void*>(uintptr_t(0x10));g_selfBegin=0x100;g_selfEnd=0x200;
        if(!result&&(!BlockPolledInput(foreign,true)||!BlockPolledInput(foreign,false)||BlockPolledInput(reinterpret_cast<void*>(0x110),true)))result=15;
        const auto fake=[](int)->SHORT{return SHORT(0x8001);};
        if(!result&&(ReadPolledKey(fake,'W',foreign)!=0||ReadPolledKey(fake,VK_LBUTTON,foreign)!=0||ReadPolledKey(fake,VK_F9,reinterpret_cast<void*>(0x110))!=SHORT(0x8001)))result=16;
        oAsyncKeyState=fake;if(!result&&AsyncKeyRaw(VK_F9)!=SHORT(0x8001))result=19;oAsyncKeyState=nullptr;
        oKeyboardState=[](PBYTE keys)->BOOL{if(!keys)return FALSE;for(int i=0;i<256;++i)keys[i]=0x80;return TRUE;};
        BYTE keys[257];std::fill_n(keys,257,BYTE(0x5a));if(!result&&(!hkKeyboardState(keys)||std::any_of(keys,keys+256,[](BYTE key){return key!=0;})||keys[256]!=0x5a||hkKeyboardState(nullptr)))result=20;
        oKeyboardState=nullptr;
        state.blockGameInput=false;if(!result&&ReadPolledKey(fake,'W',foreign)!=SHORT(0x8001))result=17;
        g_menuButtons=1;if(!result&&(ReadPolledKey(fake,VK_LBUTTON,foreign)!=0||ReadPolledKey(fake,'W',foreign)!=SHORT(0x8001)))result=21;g_menuButtons=0;
        state.blockGameInput=true;state.lastDrawAt=GetTickCount()-501;if(!result&&ReadPolledKey(fake,'W',foreign)!=SHORT(0x8001))result=18;
        state.menuOpen=false;state.blockGameInput=true;state.mouseActive=false;state.textEditing=false;g_testForeground=false;g_original=nullptr;g_selfBegin=g_selfEnd=0;
        ResetInputLocked(false);
        ImGui::DestroyContext(context);
        DeleteCriticalSection(&g_cs);
        g_csReady = false;
        g_dbgLines = 0;
        return result;
    }
#endif
}
