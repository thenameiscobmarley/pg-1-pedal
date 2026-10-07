# PG-1 plan (resume here)

Carla plugin + shared core (started 2026-10-06)
- [x] 1. Shared core: 06-Firmware-DaisySeed/pg1/core (PgCore.h/.cpp, Canvas.h) - knobs, touch, footswitch, screen, DSP; no libDaisy hardware
- [x] 2. Firmware main.cpp becomes a thin wrapper around the core; build passes
- [x] 3. Plugin project 10-Carla-Plugin (JUCE VST3 + HardwareKit), processor runs the core
- [x] 4. 3D view: shell, printed face, live screen texture, knobs, footswitches, jacks, desk
- [x] 5. Mouse: knob drag/click/wheel, footswitch press, touchscreen, camera orbit/zoom
- [x] 6. Build, install to ~/.vst3, screenshot check, docs

Dynamic EQ + BIOS/aurora UI (2026-10-06)
- [x] 7. Core rewritten: BIOS blue/grey, pearl aurora bleed (boot, tab open, page wipe, value change), 8-tab home
- [x] 8. 4-band dynamic EQ + 1024-pt FFT spectrum; core keeps its own 320x240 framebuffer, dirty-rect Blit
- [x] 9. Firmware + plugin on the new API, LCD texture NEAREST (true pixels), native frame test, docs
- [x] 10. All 8 tabs working: hum, dyn eq (+ q), comp, clarity (replaces delay), saturate, de-harsh, safety (replaces limit), visual (replaces meters); reverb + setup dropped for now

Room + safety (2026-10-07)
- [x] 11. Daisy bootloader (BOOT_SRAM): 159 KB of 480 KB; framebuffer in SDRAM
- [x] 12. Hard-edged white / cream / pearl outline animations (composited at push time, never in the picture)
- [x] 13. Display safety: clipping, NaN-proof coordinates, register refresh, repush, SPI fault reset, rest + backlight off
- [x] 14. Eq: q (hold pg-1), knob chips, live thresh meter + range bar, threshold line + range rail on the graph, help line
- [ ] 15. Try the bootloader on the real Seed3 (not confirmed for Seed3 yet)
- [x] 16. Second home page (dots bottom-right), first page-2 tab: input auto-level + in-clip warning
- [x] 17. Jacks swapped: in on the LEFT, out on the RIGHT (drill CSV, print, plugin texture, docs) - re-upload to Tayda
- [x] 18. fs-3 footswitch (right, pin D9) = fair A/B; row re-spaced to -36 / 0 / 36, y -50; usb-c label vertical, level with the socket
- [x] 19. Health checks tab (16 checks, title chip, pop-ups, explanations) + two ground chains (audio -> pin 20, main -> pin 40)
- [x] 20. Start-up logo animation (PgSplash.cpp + generated PgLogo.h)
- [x] 21. Hum learn mode (PgLearn.cpp): exact tones + 8-band hiss floor, notch + gate; plugin saves the profile
- [x] 22. Hearing range only: 8th-order 20 Hz HP + 16th-order 19.5 kHz LP; in/out detectors, mute on escape; 4 health checks
- [x] 23. Wiring diagram: two ground chains drawn step by step; screen box split around fs-3
- [x] 24. Pedal keeps settings / configs / learned profile over power-off (QSPI 7 MB, PersistentStorage; PgState.cpp)
- [x] 25. Clarity modes (dynamic / add / normalise), takeback, width, loudness, config tab (8 slots), pearl theme
- [x] 26. Polish: face (9v polarity mark, fs jobs, brand + model), firmware 1.0 on the splash, about line, first-power-up tour, page titles
- [x] 28. Wiring bench: 11-Wiring-Bench/wiring-bench.html (artifact https://claude.ai/artifact/67aSXRn4dnsirR5RoWpt1i), built from bench-template.html + the diagram
- [x] 29. Multiband dynamics (LR4 split 120 / 1k / 6k, per-band adaptive compressors)
- [x] 30. Public repo (made by _Tools/export_repo.py: scrubbed, brand 'pg audio', checks for personal info before finishing)
- [x] 31. Version-proof saves (format 2): settings/tabs stored by permanent name (PgState.cpp kRestKeys/kTabKeys), per-setting versions + Migrate(), format 1 still loads; state block 4 KB
- [x] 32. Touch d-pad mode (config, off by default; tap the title to show; no new parts - a physical d-pad didn't fit the 1590XX)
- [x] 33. Screen link 6 MHz -> 48 MHz (PLL1Q 192 MHz / 4, pins high speed, 480 MHz boost), UI paced at 60 fps, live numbers 15/s, config screen fast/safe
- [x] 35. Fixes: false 'infrasonic out' mute on loud sudden bass (output checks now need a sustained leak: 0.3 s / 0.1 s, detector 5 Hz); plugin keeps its state in ~/.config/PG-1 Pedal (configs survive reloading the plugin)
- [x] 36. Seed3 cartridge: stands on its edge in a 19 x 52 window in the left wall, plugs into a socket board (2 female headers, 2 M3 screws), keyed by snipping pin 1; USB-C panel cable + foam tape removed; drill/print/drawings/BOM/3D model updated
- [ ] 34. (after hardware test) SPI DMA so rendering overlaps sending: solid 60 on graph pages
- [ ] 27. Try it all on the real Seed3 (bootloader, flash saving, CPU load) and re-upload drill + print to Tayda
