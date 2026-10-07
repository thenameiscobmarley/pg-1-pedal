
// Xlib is kept out of every header: its macros (None, Bool, Status...) clash with JUCE.
#include <X11/Xlib.h>

namespace hwk::input
{
    struct PointerPoller::Impl
    {
        Display* display = nullptr;
        bool failed = false;

        ~Impl()
        {
            if (display != nullptr)
                XCloseDisplay (display);
        }
    };

    PointerPoller::PointerPoller() : impl (std::make_unique<Impl>()) {}
    PointerPoller::~PointerPoller() = default;

    bool PointerPoller::query (unsigned long window, int& windowX, int& windowY) noexcept
    {
        bool left = false, fine = false;
        return query (window, windowX, windowY, left, fine);
    }

    bool PointerPoller::query (unsigned long window, int& windowX, int& windowY, bool& leftDown, bool& fineModifier) noexcept
    {
        if (window == 0 || impl->failed)
            return false;

        if (impl->display == nullptr)
        {
            impl->display = XOpenDisplay (nullptr);

            if (impl->display == nullptr)
            {
                impl->failed = true;
                return false;
            }
        }

        ::Window root = 0, child = 0;
        int rootX = 0, rootY = 0;
        unsigned int mask = 0;

        const bool ok = XQueryPointer (impl->display, (::Window) window, &root, &child,
                                       &rootX, &rootY, &windowX, &windowY, &mask) != 0;
        leftDown = (mask & Button1Mask) != 0;
        fineModifier = (mask & (ShiftMask | ControlMask)) != 0;
        return ok;
    }

    bool PointerPoller::isTopmostUnderPointer (unsigned long window) noexcept
    {
        if (window == 0 || impl->display == nullptr)
            return false;

        auto* d = impl->display;
        const ::Window root = DefaultRootWindow (d);

        // Deepest window under the pointer
        ::Window current = root;
        for (int depth = 0; depth < 32; ++depth)
        {
            ::Window r = 0, child = 0;
            int rx = 0, ry = 0, wx = 0, wy = 0;
            unsigned int mask = 0;
            if (XQueryPointer (d, current, &r, &child, &rx, &ry, &wx, &wy, &mask) == 0 || child == 0)
                break;
            current = child;
        }

        // ...is it ours or inside ours?
        for (int depth = 0; depth < 32 && current != 0 && current != root; ++depth)
        {
            if (current == (::Window) window)
                return true;

            ::Window r = 0, parent = 0, *children = nullptr;
            unsigned int count = 0;
            if (XQueryTree (d, current, &r, &parent, &children, &count) == 0)
                return false;
            if (children != nullptr)
                XFree (children);
            current = parent;
        }

        return false;
    }
}
