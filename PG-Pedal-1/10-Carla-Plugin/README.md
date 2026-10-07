# PG-1 Pedal (VST3 for Carla + standalone app)

Your PG-1 as a plugin: a real-size 3D pedal with the **same screen, knobs, touch gestures and sound
as the Seed3** (8 tabs: hum, dyn eq, comp, clarity, saturate, de-harsh, safety, visual). Both run `06-Firmware-DaisySeed/pg1/core` (PgCore), so whatever you build or change
in the core shows up identically on the PC and on the pedal.

## Use it in Carla
1. Build (below). The VST3 is copied to `~/.vst3/PG-1 Pedal.vst3` automatically.
2. In Carla: **Add Plugin → refresh → PG-1 Pedal**, connect your audio in and out.

## Controls (mouse)
| Do | On the pedal that's |
|---|---|
| drag a knob up/down | turn it (fast drag = big jumps, slow = fine) |
| shift-drag (or right-drag) a knob | hold it down while turning: fine (dyn eq pg-1: q) |
| click a knob / two quick clicks | push / double-push (reset to default) |
| mouse wheel on a knob | spin it |
| hold a knob click 0.6 s | long-push: back to home |
| click / hold a footswitch | fs-1 bypass (safety stays on); fs-2 next band / next page, hold = stage on/off |
| click / drag on the screen | touch: open tabs, drag eq nodes, tap a box = what it does, tap the on/off chip, tap the visualizer = next view |
| drag the background | look around |
| wheel on the background | zoom; double-click the background = reset view |

## Build
```
cd 10-Carla-Plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```
Needs `~/JUCE` and `third_party/HardwareKit` (the 3D hardware library). The standalone app is in
`build/PG1Pedal_artefacts/Release/Standalone/`.

## Changing the pedal
Edit `06-Firmware-DaisySeed/pg1/core/PgCore.cpp` (screen pages, parameters, gestures, DSP), rebuild
this plugin to try it, then rebuild the firmware (`make` in `pg1`) and flash it: same code, same result.

## The screen is the real resolution
The core draws into a 320 x 240 RGB565 framebuffer and pushes only the changed rectangles, exactly what goes
over SPI to the ILI9341. The plugin shows that buffer with no smoothing, so you see the same pixels (zoom
in with the wheel to read it, like leaning over the pedal).

## Latency
The safety stage's look-ahead limiter adds 1 ms; the plugin reports it to the host, which compensates.

## Display rest
Like the pedal: 5 idle minutes = near-black screen with a drifting mark, 20 = backlight off. Any control wakes it.
