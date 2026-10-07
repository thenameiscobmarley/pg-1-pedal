/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 hardwarekit
  vendor:             pgaudio
  version:            1.0.0
  name:               HardwareKit
  description:        3D hardware models (knob styles, buttons, toggles, lamps, racks), materials,
                      GL helpers, control animation and a fisheye loupe for plugin UIs.
  dependencies:       juce_opengl juce_gui_basics
  linuxLibs:          X11
  windowsLibs:        user32
  minimumCppStandard: 17

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define HARDWAREKIT_H_INCLUDED

#include <juce_opengl/juce_opengl.h>
#include <array>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

#include "gfx/GLMath.h"
#include "gfx/GLResources.h"
#include "geo/Geometry.h"
#include "geo/Cable.h"
#include "models/Hardware.h"
#include "anim/Animation.h"
#include "shaders/Materials.h"
#include "fx/Loupe.h"
#include "input/PointerPoller.h"
#include "input/WindowVisibility.h"
