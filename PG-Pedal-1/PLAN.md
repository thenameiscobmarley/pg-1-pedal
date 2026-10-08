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
  (PCB-mount 1/4" TRS jacks: Neutrik NMJ6HCD2, fitted by JLCPCB): no extra screws.
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
- [x] a. parts: A-6980 10k log dual pots, A-8567 14 mm white ripple knobs, Neutrik NMJ6HCD2 jacks (on the carrier board), A-2599 pink KN2310 caps (fit PBS-24)
- [x] b. layout: Seed3 window on the right wall (side E - CHECK in Tayda's preview), pg-hp / pg-line pots left of the screen, 5 top-edge holes; face print with IBM Plex Mono small words + pink accents (not uploaded to Tayda yet)
- [x] c. firmware/UI: names from one rule (ui::KnobName / FootName), 2nd settings page (swipe the boxes, tap the dots, slide animation), drag a box up/down to change it, rounded boxes / tiles / strip (more polish possible)
- [x] d. carrier PCB (budget version): circuit (12-Carrier-Board/DESIGN.md, carrier.py), board made by make_board.py
        (KiCad 10 + Freerouting: all parts on the bottom, fully routed, 0 DRC errors), PCBWay files in 12-Carrier-Board/pcbway/
        (Gerbers zip, BOM, CPL); ~$55-80 for 2 assembled boards. Next: user orders (jacks: send to PCBWay or fit later)
- [~] g. AUDIT + FIX before ordering (2026-10-08, IN PROGRESS - resume here):
        DONE: JLCPCB CPL rewritten (make_jlc.py: from the board, Gerber coordinates, THT parts = centre of pins,
        rotation fixes from jlc_rotations.csv = JLCKicadTools table; BOM one line per LCSC part -> no "matched twice").
        Jack = NMJ6HFD2 official drawing: pins 4.35/10.7/17.05 mm from panel (was 16.7: fixed JACK_T), axis 8.29 mm.
        Hammond 1590XX drawing: corner lid-screw posts at +-51/+-63 -> jacks moved to +-34, 9v to -13 (pg_generate
        SIDE_B + plugin sideB), board top corners notched (POST_X 45, POST_Y 57). Passives -> 0402 (JLC basic).
        B0505S datasheet: body sits to ONE side of pins -> footprint courtyard both sides, U3 at x 21.
        Flat-only zone under the DC jack (DC_ZONE). Checked OK: AMS1117, PESD15VL2BT, SMAJ12A, LP2985, ISO7741,
        ISO1540, AIC3204, TPA6139A2, OPA1652 pinouts; encoder M7 (7.2 hole), screen holes/window, pots RK09L (M7, snap
        off the lug, female jumpers fit its pins).
        LAST ROUTE: 0 unconnected, isolation OK, but 2 courtyard overlaps: pot headers J20/J19 (row y 44.5) touch the
        jack courtyards (jacks moved in + JACK_T 17.05). Fix idea: header rows at y 44.2 and copper-edge clearance 0.3
        (make_board.py: m_CopperEdgeClearance + write_project min_copper_edge_clearance), then re-route.
        NEXT: check DRC (0.127 rules in .kicad_pro) + check_isolation.py; python3 make_bom.py && python3 make_jlc.py;
        Tayda upload (drill + UV: holes MOVED) via _Tools/tayda_upload.py with the saved ids; rebuild plugin;
        export_repo.py (PG_PY = venv with fonttools) + push; tell user: re-upload all 3 JLC files, Standard + Top side,
        Confirm Parts Placement yes, send screenshot of JLC placement preview to check rotations.
        MODULE BAY + ANALOG FX (user: "Connector + one analog effect", bay in the LEFT wall, filter + VCA) DONE in
        code: MCP4461 quad digipot U13 (0x2C, C638707; datasheet pins checked) = per side attenuator + rheostat into
        10 nF C0G (C22400107) low-pass, TLV9062 VSSOP buffer U15 (C398356), between codec LO and pg-hp. J21
        "expansion" 4-pin RA header (3v3 gnd scl sda, C32713263) left of J10 (J10 moved to x 3.5). Board: "analog fx"
        box right of iso power, output box only upper, codec narrower, isolation title bottom-right. Firmware
        AnalogFx (isoaudio.*): Init = unity/open (+ NV EEPROM), SetLevel / SetCutoff; builds. pg_generate: side C
        "expansion bay" 4.0 x 11.9 slot at Y 17 (Tayda CSV + drill SVG). Docs updated (DESIGN, WIRING).
        NEXT (if not yet ticked): full route result + DRC + check_isolation; make_bom/make_jlc; copy renders;
        Tayda re-upload; export + push; tell user.
- [x] f. v3 carrier = ISOLATED CODEC (decided 2026-10-08, replaces the 4-jack analog board; resume here).
        DONE: carrier.py netlist (87 parts, barrier check), PG1:DCDC_SIP4_B0505S footprint, firmware (src/isoaudio.*:
        codec + ADS1015 drivers, SAI2 + I2C1, pins moved, touch polled, auto-ranging + too-hot mute, health checks
        C_HOT / C_CODEC; builds), face print (in / 9v / out, pg-line = input gain -12..+24), WIRING.md v3 note,
        plugin 3D model (3 holes, black plastic jack noses; NOT rebuilt yet). make_board.py adapted (two domains,
        barrier keepout, per-domain pours, groups / flow arrows); placement run in progress.
        CHANGED (user wants safety analog, not software): pg-hp is now a REAL analog pot in the isolated output
        (codec LOL/LOR -> 4.7 uF -> pot -> TPA6139A2 x-2 -> jack; J19 6-pin header on the iso side under the in jack);
        firmware: IWDG watchdog (2 s), codec DAC auto-mute on DC, fixed unity digital output gain.
        CHANGED AGAIN (user): NO auto-ranging. Both knobs analog: inverting OPA1652 stages with centre-detent dual 10k
        LINEAR pots (Alps RK09L1240015, Mouser): pg-line -33..0..+33 dB (220R ends), pg-hp -21..0..+21 dB (1k ends);
        centre click = unity. ADS1015 + mic path removed; J20 (pg-line) + J19 (pg-hp) 6-pin headers on the iso side
        under the in / out jacks. Firmware: fixed gains (kInGain/kOutGain), no knob reading. Face: pink centre dot = unity.
        Board: groups have fixed areas (make_board.py GROUPS), barrier at y 29.5, all parts top side.
        DONE 2026-10-08: routed 100 %, DRC clean at JLCPCB limits (5 mil), check_isolation.py OK; jlcpcb/ files made;
        Tayda drill + UV templates uploaded; plugin rebuilt. NEXT: user orders (JLCPCB Standard, Top side) + Mouser pots.
        TODO: finish layout + route + DRC, make_jlc.py / make_bom.py for the new parts, DESIGN.md rewrite, Tayda
        upload (drill + UV), plugin rebuild, export + push, memory.
        - jacks: ONE stereo IN (left) + ONE stereo OUT (right), both isolated, Neutrik NMJ6HFD2 (plastic nose, C368491),
          plus the 9v panel jack. Top wall: in / 9v / out. Face print, drill, 3D model follow.
        - iso side: TLV320AIC3204 (C24109, QFN-32) does mic preamp (0..47.5 dB) / line / hot-amp input (2nd attenuated
          input path + clamps), headphone (16-600 ohm) or line output; own power: B0505S (C512048) -> LDO 3.3 V.
        - barrier: ISO7741 (C571196: BCLK, WCLK, DIN -> codec; DOUT <- codec) + ISO1540 (C179739, I2C).
        - pedal side: 5 V LDO for the B0505S, 3.3 V for the isolators, ADS1015-type I2C ADC reads the 2 pots
          (pg-hp = output level, pg-line = input gain; no audio through the pots).
        - Seed3 pins: SAI2 32 SD B (rx), 33 SD A (tx), 34 FS, 35 SCK; I2C1 12 SCL / 13 SDA; LCD_DC -> 37, LCD_RESET ->
          pin 1 (D0), T_DO -> 36, T_IRQ dropped (touch polled). Seed's own codec (16-19) unused.
        - firmware: SAI2 + codec init over I2C, auto-ranging input + safety (clip -> range switch + mute + warning),
          output level from pg-hp, input gain from pg-line.
        - board: groups in printed boxes + flow arrows (make_board.py GROUPS/FLOW, already written), small text,
          logo on the back corner, all parts on top, inputs left. Then JLCPCB files (make_jlc.py), Standard/Economic top side.
- [~] e. plugin 3D model done (Seed3 right, 5 jacks, small knobs, pink caps); docs + BOM/budget for the carrier board still to do

- [ ] 34. (after hardware test) SPI DMA so rendering overlaps sending: solid 60 on graph pages
- [ ] 27. Try it all on the real Seed3 (bootloader, flash saving, CPU load) and re-upload drill + print to Tayda
