
// Xlib stays out of every header: its macros clash with JUCE.
#include <X11/Xlib.h>
#include <X11/Xatom.h>

namespace hwk::input
{
    struct WindowVisibility::Impl
    {
        Display* display = nullptr;
        Atom netWmState = 0, hidden = 0;
        bool failed = false;

        ~Impl()
        {
            if (display != nullptr)
                XCloseDisplay (display);
        }

        bool open()
        {
            if (display != nullptr || failed)
                return display != nullptr;

            display = XOpenDisplay (nullptr);
            if (display == nullptr)
            {
                failed = true;
                return false;
            }

            netWmState = XInternAtom (display, "_NET_WM_STATE", False);
            hidden = XInternAtom (display, "_NET_WM_STATE_HIDDEN", False);
            return true;
        }

        bool isHiddenByWm (::Window w)
        {
            Atom type = 0;
            int format = 0;
            unsigned long count = 0, after = 0;
            unsigned char* data = nullptr;
            bool isHidden = false;

            if (XGetWindowProperty (display, w, netWmState, 0, 64, False, XA_ATOM, &type, &format, &count, &after, &data) == Success
                && data != nullptr)
            {
                if (type == XA_ATOM && format == 32)
                {
                    auto* atoms = (Atom*) data;
                    for (unsigned long i = 0; i < count; ++i)
                        isHidden = isHidden || atoms[i] == hidden;
                }
                XFree (data);
            }

            return isHidden;
        }
    };

    WindowVisibility::WindowVisibility() : impl (std::make_unique<Impl>()) {}
    WindowVisibility::~WindowVisibility() = default;

    bool WindowVisibility::isVisible (unsigned long window) noexcept
    {
        if (window == 0 || ! impl->open())
            return true;

        auto* d = impl->display;

        // Unmapped anywhere up the tree (iconified windows are unmapped by the window manager)
        XWindowAttributes attributes {};
        if (XGetWindowAttributes (d, (::Window) window, &attributes) == 0)
            return true;
        if (attributes.map_state != IsViewable)
            return false;

        // Minimised but kept mapped (some compositors): _NET_WM_STATE_HIDDEN on an ancestor
        ::Window current = (::Window) window;
        for (int depth = 0; depth < 16 && current != 0; ++depth)
        {
            if (impl->isHiddenByWm (current))
                return false;

            ::Window root = 0, parent = 0, *children = nullptr;
            unsigned int numChildren = 0;
            if (XQueryTree (d, current, &root, &parent, &children, &numChildren) == 0)
                break;
            if (children != nullptr)
                XFree (children);
            if (parent == 0 || parent == root)
                break;
            current = parent;
        }

        return true;
    }
}
