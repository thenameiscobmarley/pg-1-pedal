// Xlib lives in its own translation unit: its macros (None, Bool, Status...) clash with JUCE.
#include "hardwarekit.h"

#if JUCE_LINUX || JUCE_BSD
 #include "input/PointerPoller.cpp"
 #include "input/WindowVisibility.cpp"
#endif
