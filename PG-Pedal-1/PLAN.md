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
- [x] 37. PID tab (auto-adjust: steers eq / comp / mband / clarity / deharsh / width with user P, I, D; safe ranges; holds in silence) + visualizer scope, bars, history
- [x] 38. Clarity: up to 10 self-placing bands (own freq / q / gain, cut or lift, 20 detectors, detrended); anti-duck everywhere (bass-free detectors, bass-first safety limiter, false infrasonic mute fixed, final duck guard): onset dips 7-12 dB -> <= 0.3 dB
- [x] 39. Seed3 load checked on an emulated Cortex-M7 (_Tools/seed3-bench): was 102-153 % worst case, optimised to ~59 % (defaults ~51 %); plugin runs the core at 48 kHz / 48-sample blocks and reports the Seed3 load
- [ ] 40. (decide) carrier PCB: line/instrument input buffers, headphone amp, ESD/over-voltage protection, pin headers (PCBWay/JLCPCB assembly)
## v2 redesign (decided 2026-10-08, in progress) - resume here
Decisions from the user (question rounds):
- Carrier PCB, made + assembled by PCBWay from files we generate (Tayda doesn't make custom PCBs). Hangs from the top-wall jack nuts
  (PCB-mount 1/4" TRS jacks, e.g. Tayda A-1122 / REAN NYS216): no extra screws.
- Top edge jacks (all 1/4" TRS): line-in, line-out, headphones, no-amp, + 9V. The exp jack is dropped.
- no-amp = the processed sound at the same loudness that came in (unity gain), through a clean buffer that can drive a headset.
- Headphone amp: very low noise, drives 50 ohm now and 200 ohm later, max +12 dB over line (~4 V rms) -> needs more than a 9 V single
  supply (plan: charge-pump -9 V, filtered, OPA1622-class amp).
- Two small knobs LEFT of the screen: top = headphone volume (analog, silent .. +12 dB), below = line-out level (analog).
  They need the left wall free -> the Seed3 cartridge moves to the right wall.
- Footswitches renamed pg-a, pg-b, pg-c (every control starts with "pg-", from one naming rule); pink footswitch caps;
  the 4 knobs stay white knurled aluminium.
- Plain monospace, terminal-like lowercase font for some text (my choice where); keep the diamonds, don't overdo shapes / black ink,
  pink accents allowed if they don't look out of place.
- Screen: fancier, more spacious, touch-smart; tabs can have more than 4 settings: swipe the 4 boxes sideways as a page, with page dots
  (knobs always work the boxes you see).
Steps:
- [x] a. parts: A-6980 10k log dual pots, A-8567 14 mm white ripple knobs, A-1122 PCB jacks, A-2599 pink KN2310 caps (fit PBS-24)
- [x] b. layout: Seed3 window on the right wall (side E - CHECK in Tayda's preview), pg-hp / pg-line pots left of the screen, 5 top-edge holes; face print with IBM Plex Mono small words + pink accents (not uploaded to Tayda yet)
- [x] c. firmware/UI: names from one rule (ui::KnobName / FootName), 2nd settings page (swipe the boxes, tap the dots, slide animation), drag a box up/down to change it, rounded boxes / tiles / strip (more polish possible)
- [x] d. carrier PCB (budget version): circuit (12-Carrier-Board/DESIGN.md, carrier.py), board made by make_board.py
        (KiCad 10 + Freerouting: all parts on the bottom, fully routed, 0 DRC errors), PCBWay files in 12-Carrier-Board/pcbway/
        (Gerbers zip, BOM, CPL); ~$55-80 for 2 assembled boards. Next: user orders (jacks: send to PCBWay or fit later)
- [~] e. plugin 3D model done (Seed3 right, 5 jacks, small knobs, pink caps); docs + BOM/budget for the carrier board still to do

- [ ] 34. (after hardware test) SPI DMA so rendering overlaps sending: solid 60 on graph pages
- [ ] 27. Try it all on the real Seed3 (bootloader, flash saving, CPU load) and re-upload drill + print to Tayda
