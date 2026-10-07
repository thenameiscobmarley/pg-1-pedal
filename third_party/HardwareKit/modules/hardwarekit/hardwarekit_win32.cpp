// Win32 lives in its own translation unit, like Xlib on Linux: its macros must not reach the rest of the module.
#include "hardwarekit.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <cstdint>

 #include "input/PointerPoller_win32.cpp"
 #include "input/WindowVisibility_win32.cpp"
#endif
