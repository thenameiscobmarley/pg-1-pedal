# HardwareKit

Shared JUCE module for pg audio plugin UIs rendered as real-looking 3D hardware.

```cmake
set(HARDWAREKIT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../HardwareKit" CACHE PATH "HardwareKit checkout")
juce_add_module("${HARDWAREKIT_PATH}/modules/hardwarekit")
target_link_libraries(MyPlugin PRIVATE hardwarekit juce::juce_opengl)
```

```cpp
#include <hardwarekit/hardwarekit.h>
```

| Namespace | What |
|---|---|
| `hwk::gfx` | `Vec3`, `Mat4`, `projectToNdc`, `screenQuad`; `MeshData`, `GpuMesh`, `ShaderProgram`, `Texture2D`, `RenderTarget` |
| `hwk::geo` | primitives: swept rounded rects / polygons (bevel profiles), plates with holes, wells, quads, boxes, annuli, domes, pointer plates |
| `hwk::models` | multi-part models with material roles: `knob (KnobStyle)`, `pushButton`, `batToggleBase/Lever`, `jewelLamp`, `ledLens`, `rackScrew`, `rackChassis`, `faceplateEdge` |
| `hwk::shaders` | shared vertex shader, `Material` snippets, `fragmentSource()`, built-in `library::` materials (panels, chrome, plastic, emissive, recess, print rings, shadows, screen overlays, glow sprites, value arcs, magnifier lens) |
| `hwk::anim` | `approach`, `KnobAnimator` (critically damped), `ButtonAnimator`, `ToggleAnimator` |
| `hwk::fx` | `Loupe::zoomedViewProj` for a fisheye magnifier that re-renders the scene zoomed |
| `hwk::input` | `PointerPoller` (X11 pointer on the render thread), `WindowVisibility` (pause rendering when minimised) |

## Knob styles

`proXl` · `fluted` (Davies 1900 type) · `chickenHead` · `aluminium` · `softTouch` · `jewelCap`

Every knob points at -z at its centre value and turns about +y. Parts carry a `Role`
(`body`, `metal`, `pointer`, `accent`) plus colour, grip ridges and polish, so a renderer maps them to
`plastic`, `chrome` and `emissive` materials without knowing the style.

## Conventions

- Panel-local space: x across, z down the panel, y out of the panel. Map to world with a rotation of +90° about x.
- Materials are compiled one program each (no per-fragment branching); plugins add their own `Material`s
  for displays and similar one-off surfaces.
- Everything allocates at context creation; drawing never allocates.

## Platforms

Linux/X11 today. Everything except `input/PointerPoller` and `input/WindowVisibility` is portable;
those two are the only platform code, and `hardwarekit_x11.cpp` is the only translation unit that
includes `Xlib.h`. The ENH Master repository ships `PORTING-TO-WINDOWS.md`, which lists the Win32
equivalents (`GetCursorPos`, `GetAsyncKeyState`, `WindowFromPoint`, `IsIconic`, …) and a prompt you
can hand to a coding AI to do the port.

## Used by

- **ENH Master** - adaptive clarity / footstep enhancer with a two-unit 3D rack UI.
