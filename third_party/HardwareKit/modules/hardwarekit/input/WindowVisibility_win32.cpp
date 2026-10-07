// Windows version of WindowVisibility (see WindowVisibility.h): the editor can be seen unless it, or the
// top-level window it lives in, is hidden or minimised.

namespace hwk::input
{
    struct WindowVisibility::Impl {};

    WindowVisibility::WindowVisibility() : impl (std::make_unique<Impl>()) {}
    WindowVisibility::~WindowVisibility() = default;

    bool WindowVisibility::isVisible (unsigned long window) noexcept
    {
        const HWND hwnd = reinterpret_cast<HWND> ((uintptr_t) window);
        if (hwnd == nullptr || ! IsWindow (hwnd))
            return true;   // unknown: never pause because of a failed query
        const HWND root = GetAncestor (hwnd, GA_ROOT);
        for (HWND w : { hwnd, root })
            if (w != nullptr && (! IsWindowVisible (w) || IsIconic (w)))
                return false;
        return true;
    }
}
