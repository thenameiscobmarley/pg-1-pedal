#ifdef HARDWAREKIT_H_INCLUDED
 /* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own. */
 #error "Incorrect use of JUCE cpp file"
#endif

#include "hardwarekit.h"

using namespace juce::gl;

#include "gfx/GLResources.cpp"
#include "geo/Geometry.cpp"
#include "models/Hardware.cpp"
#include "shaders/Materials.cpp"
