// Windows version of PointerPoller (see PointerPoller.h): the pointer and buttons straight from Win32,
// on whichever thread calls it. `window` carries the editor peer's HWND (user handles fit in 32 bits,
// so the unsigned long handle type of the interface holds one on 64-bit Windows too).

namespace hwk::input
{
    struct PointerPoller::Impl {};

    PointerPoller::PointerPoller() : impl (std::make_unique<Impl>()) {}
    PointerPoller::~PointerPoller() = default;

    static HWND toHwnd (unsigned long window) noexcept { return reinterpret_cast<HWND> ((uintptr_t) window); }

    bool PointerPoller::query (unsigned long window, int& windowX, int& windowY) noexcept
    {
        bool left = false, fine = false;
        return query (window, windowX, windowY, left, fine);
    }

    bool PointerPoller::query (unsigned long window, int& windowX, int& windowY, bool& leftDown, bool& fineModifier) noexcept
    {
        const HWND hwnd = toHwnd (window);
        if (hwnd == nullptr || ! IsWindow (hwnd))
            return false;

        POINT p {};
        if (! GetCursorPos (&p) || ! ScreenToClient (hwnd, &p))
            return false;

        windowX = (int) p.x;
        windowY = (int) p.y;
        // The physical left button, whatever the user has swapped in the mouse settings
        const int primary = GetSystemMetrics (SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
        leftDown = (GetAsyncKeyState (primary) & 0x8000) != 0;
        fineModifier = (GetAsyncKeyState (VK_SHIFT) & 0x8000) != 0 || (GetAsyncKeyState (VK_CONTROL) & 0x8000) != 0;
        return true;
    }

    bool PointerPoller::isTopmostUnderPointer (unsigned long window) noexcept
    {
        const HWND hwnd = toHwnd (window);
        POINT p {};
        if (hwnd == nullptr || ! GetCursorPos (&p))
            return true;   // unknown: never block a press because a query failed
        const HWND under = WindowFromPoint (p);
        if (under == nullptr)
            return false;
        // The window under the pointer is ours when it is the editor or one of its children, or shares
        // the editor's top-level window (a plugin editor lives inside the host's)
        return under == hwnd || IsChild (hwnd, under) || GetAncestor (under, GA_ROOT) == GetAncestor (hwnd, GA_ROOT);
    }
}
