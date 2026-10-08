# PG-1 Pedal (VST3 for Carla + standalone app)

Your PG-1 as a plugin: a real-size 3D pedal with the **same screen, knobs, touch gestures and sound
as the Seed3** (16 tabs over two home pages). Both run `06-Firmware-DaisySeed/pg1/core` (PgCore), so whatever you build or change
in the core shows up identically on the PC and on the pedal.

## Runs exactly like the Seed3
- The core always runs at **48 kHz in 48-sample blocks**, the same as the pedal (`main.cpp`: `SAI_48KHZ`, `SetAudioBlockSize(48)`),
  whatever rate and block size the host uses. Other host rates are resampled to 48 kHz and back; at 48 kHz samples pass through untouched.
  The plugin reports its delay to the host (96 samples at 48 kHz: one Seed3 block + the safety limiter's 1 ms look-ahead).
- The **health tab's cpu check shows the Seed3's load, not your PC's**: each tab's cost was measured on an emulated Cortex-M7 built
  with the firmware's own flags (`_Tools/seed3-bench`, table in `Source/Seed3Costs.h`), so the plugin warns exactly when the real pedal would.

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
| click / hold a footswitch | pg-a bypass (safety stays on); pg-b next band / next page, hold = stage on/off |
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

## Your settings stay
Like the pedal's flash, the plugin keeps everything (settings, the 8 configs, the learned hum / hiss) in
`~/.config/PG-1 Pedal/pedal-state.bin`: written about 4 s after you change something and when it closes, loaded when a
new PG-1 starts. A Carla project saved with its own PG-1 state still loads that state instead.
