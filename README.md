# PG-1 pedal

A DIY stereo audio pedal built around a **Daisy Seed3**, in a white Hammond 1590XX from Tayda. A 2.4" colour
touchscreen, four endless push-knobs, three footswitches and a chain of sound tools:

`in -> hum + hiss (with learn) -> input auto-level -> dynamic eq -> compressor -> multiband -> clarity + bass ->
saturation -> de-harsh -> stereo width -> takeback -> quiet loudness -> [bypass] -> ear + speaker safety -> out`

plus a visualizer, health checks (it tells you when something is wrong), 8 saved configs, a fair level-matched
A/B footswitch, and everything remembered over power-off.

The same core code runs on the pedal and in a **Carla / VST3 plugin** with a real-size 3D model of the pedal, so you
can try every screen and sound on a PC first.

## What's where

| Folder | |
|---|---|
| `PG-Pedal-1/01-Questions-and-Specs` | the spec |
| `PG-Pedal-1/02-Parts-and-Cart` | Tayda cart files + parts list (with the few parts Tayda doesn't sell) |
| `PG-Pedal-1/03-Drill-Template` | drill positions for drill.taydakits.com |
| `PG-Pedal-1/04-Top-Artwork` | the UV print (CMYK PDF for Tayda) and previews |
| `PG-Pedal-1/05-Wiring-and-Schematics` | **wiring-diagram.png** (every wire + both ground chains), layout drawings, WIRING.md |
| `PG-Pedal-1/06-Firmware-DaisySeed` | the firmware (`pg1/`) and its shared core (`pg1/core/`) |
| `PG-Pedal-1/10-Carla-Plugin` | the JUCE plugin with the 3D pedal |
| `PG-Pedal-1/11-Wiring-Bench` | `wiring-bench.html`: practise the wiring in a browser; a virtual pedal tells you what's wrong |
| `_Tools/pg_generate.py` | one script makes the drill table, artwork, drawings and the splash logo from the geometry at its top |
| `_Tools/tayda_upload.py` | uploads the drill + print designs to drill.taydakits.com (asks for your Tayda login) |
| `third_party/HardwareKit` | the 3D hardware UI module the plugin uses (Apache-2.0) |

## Building

Firmware (Linux):

```
git submodule update --init           # libDaisy v9.0.0 + DaisySP
cd PG-Pedal-1/06-Firmware-DaisySeed
(cd libDaisy && make -j"$(nproc)") && (cd DaisySP && make -j"$(nproc)")
cd pg1 && make -j"$(nproc)"           # build/pg1.bin (runs from the Daisy bootloader: see the firmware README)
```

Plugin: needs JUCE (https://juce.com, licensed separately).

```
cd PG-Pedal-1/10-Carla-Plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH=/path/to/JUCE
cmake --build build -j"$(nproc)"
```

Artwork and drawings: `pip install fonttools`, then `python3 _Tools/pg_generate.py` (needs `rsvg-convert` and
ImageMagick for the PNG/PDF previews).

## Credits

- libDaisy and DaisySP by Electro-Smith (MIT), as git submodules
- Nunito font (SIL Open Font License, `_Tools/fonts/OFL.txt`)
- HardwareKit (Apache-2.0, `third_party/HardwareKit`)
- JUCE is not included; it has its own licence
