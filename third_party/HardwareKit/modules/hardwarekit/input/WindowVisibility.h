#pragma once

#include <memory>

namespace hwk::input
{
    /** Asks the X server whether a window can actually be seen: it (and every ancestor) is mapped,
        and no ancestor carries _NET_WM_STATE_HIDDEN (minimised). Plugin editors are embedded in a
        host window, so JUCE's own peer state does not always notice the host being minimised.
        Uses a private X connection (message thread only). */
    class WindowVisibility
    {
    public:
        WindowVisibility();
        ~WindowVisibility();

        /** True when unknown (no X server, window 0): never pause because of a failed query. */
        bool isVisible (unsigned long window) noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
