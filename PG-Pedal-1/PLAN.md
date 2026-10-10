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
- [x] g. AUDIT + FIX before ordering (2026-10-08):
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
        DONE 17:20: routed 100 %, check_isolation OK, DRC only 3 courtyard-MARGIN overlaps (J21/J10, J19/J2, J20/J1:
        real plastic gaps ~0.7 mm, accepted); jlcpcb/ files remade (38 kinds, 107 parts, all with LCSC codes); Tayda
        drill + UV templates updated; plugin rebuilt; repo exported (id regex fixed: no false hit on coordinates) +
        pushed. NEXT: user re-uploads the 3 JLC files and orders; Mouser pots.
        COST CUT (user: $50 max before shipping, must stay isolated + quiet + safe): analog fx REMOVED (firmware
        driver kept for an add-on); J22 "fx loop" 2x4 vertical header (LO -> FXR, jumper caps; iso 3v3/gnd/scl/sda)
        for add-on boards; PSM712 (basic C32677) for D2/D61/D62; C30/C40 -> 100nF 0402 basic; THT parts (J1 J2 J10
        J19 J20 J21 J22 U3) first skipped, then user said NO soldering: all fitted again (J22 = C32713277). Re-route + DRC, make_jlc, tell user (cheap shipping: Global Standard Direct Line, watch tariff).
        COST CUT 2 (user: ~$50 total incl. shipping, slight IEM hiss OK): OPA1652 x3 -> TLV9062IDR (C398355, same
        pinout, RRIO, CMOS input); LP2985 x2 -> XC6206P332MR (basic C5446, SOT-23: 1 GND 2 OUT 3 IN; bypass caps
        C5/C13 removed); THT parts SKIPPED again (user solders 8: jacks, headers, B0505S). Estimate ~$50 + $8.47.
        IL300 through-hole redesign was costed and dropped (~$65-70, hissier, may not fit).
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
- [x] 41. Rounded aurora: pearl edge + outline animations follow the tiles' rounded corners (EdgeInset in PgCore.cpp;
      Outline.rad: tabs 6, param boxes 4, band chips 3, boot/tour square). Page open/close move is now LIQUID
      (Outline.kind 1 + LiquidField SDF: melt + drips, falling drops, gooey neck, springy settle; kWipeMs 800).
      Native frame test: scratchpad liq/t.cpp. Plugin + firmware rebuilt.
- [~] 42. BOARD SIMULATOR in the plugin (user chose FULL TRANSIENT). STATUS 2026-10-09:
      a. DONE: 12-Carrier-Board/board_export.py -> Source/sim/BoardData.h (real copper: islands, tracks, vias).
      b. IN PROGRESS: Source/sim/BoardSim.{h,cpp} (not in CMake yet). Netlist from BoardData + wiring (DefaultWiring =
         WIRING.md incl. pots pin3->header1). Slow solver = smooth models + Newton (numeric central-diff Jacobian,
         line search with uniform step scaling, pseudo-cap per node, chip enables read from step-start voltages Ven).
         Steady state WORKS and is fast (~1 ms per 30 ms tick): +9V 8.22, +5V 5.00, +3V3 3.30, ISO5V 5.05,
         ISO3V3 3.30, IBIAS 1.65, 197 mA, HP_ON 0.5 s unmute. PROBLEM: power-up from 0 V is slow (~15 s compute):
         operatingPoint() (pseudo-transient continuation) oscillates g 2 <-> 0.2. Ideas: gentler g schedule
         (x0.5, fail x3), or start from a hand-built guess (rails at nominal). Native tests: scratchpad simt/
         d2.cpp (timeline), d3.cpp (tick timing), t.cpp (gain + wrong-wiring cases). Debug env vars NDEBUG*.
         Then: fault check for op-amps should use rail proximity of V (not PWL st); fast audio islands untested
         since the slow-solver rewrite; then plugin integration (Seed3Runner hooks runInput/runOutput around
         core.Process, small-knob params, slow thread), board view, wiring editor.
      a. _Tools/board_export.py (pcbnew): 12-Carrier-Board -> 10-Carla-Plugin/Source/BoardData.h (parts, pads, nets,
         track/via geometry, copper islands per net from KiCad connectivity = opens/shorts from the REAL copper).
      b. Source/sim/: MNA engine (dense LU, trapezoidal companions, Newton for diodes / op-amp rails), auto-partitioned:
         power + slow nodes (rails, IBIAS) at ~2 kHz, audio islands (input + output analog per channel) at 48 kHz.
         Models: R, C, diode/TVS/PSM712, PTC (I^2t heating), AMS1117/XC6206 (dropout, current limit), B0505S
         (isolated, own ground), TLV9062 (GBW pole, rails, output R), TPA6139A2 (x-2, charge pump), codec ADC/DAC
         (needs power + I2C config; DAC = DSP output), ISO7741/ISO1540 (pass only if both sides powered), jacks, pots.
      c. Wiring: external jumpers (J10 <-> Seed3 pins, pots <-> J19/J20, DC jack, fx-loop bridges) as an editable map,
         default = WIRING.md; wrong wiring -> the sim shows it (no sound, wrong level, smoke).
      d. Board view (2D, in the plugin window): real copper, current-flow dots on traces, part heat, fault list.
      e. Audio: plugin audio goes jack -> simulated input stage -> codec ADC -> core DSP -> DAC -> simulated output.
- [x] 43. UI (finding things + navigation): home footer = what the focused tab is for (ui::kTabWhat); pg-b = "next": settings page 2 first, then the next tab; title shows 1/2 / 2/2.
- [x] 44. BUG FOUND (while modelling the pots): pot pin 3 = clockwise end, so "1 l" on header pin 1 made the knobs work
      BACKWARDS (right = less). Silk relabelled on the routed board (3 l, 2 l, 1 l, 3 r, 2 r, 1 r; make_board HEADER_PINS
      too), gerbers / renders / JLC files regenerated (no copper change), docs updated. User must re-upload the gerbers zip.
- [x] 45. Press-fit ("locking") header holes on J10 J19 J20 J21 J22: each other HOLE offset 0.127 mm (pad copper kept in
      place via pad offset, DRC clean), so header strips jam in without solder (+ hot glue). Gerbers regenerated.
      Backup of the unstaggered board: 12-Carrier-Board/build/pg1-carrier.before-lock.kicad_pcb.
- [x] 46. 2026-10-09 (buy day): silk labels with part names + U3 "PIN 1" (regen_outputs.py, no re-route); headers
      to be SOLDERED (press-fit holes still solder fine). AC analysis added to BoardSim (acResponse): found the
      C31/C41 3.2 kHz low-pass -> DNP (make_jlc / make_bom / BoardSim kNotFitted). Firmware kInGain 0.118 -> 0.0895
      (codec's 20k input loads the divider). Plugin face texture was stale (export only rebuilt the copy) ->
      pg_generate.py now writes 10-Carla-Plugin/assets/face-print.png; plugin rebuilt. Wrong-wiring battery:
      scratchpad simt/ww.cpp; bench: simt/ac.cpp.
- [x] 47. 2026-10-09: UI back to square edges, aurora 3 px in the strong middle of the pearl palette, liquid page move
      reverted (straight slide, kWipeMs 420, settings slide 150 ms). BUILD-GUIDE.md written (current wiring; WIRING.md
      had stale bits: Seed3 window is RIGHT wall, key with pin 16 not pin 1, no audio ground chain, no exp jack).
- [x] 48. 2026-10-09 face redesign: gain pots under the jacks (x +-48, y 52; "Input Gain dB / any level, not too hot",
      "Output Gain dB / phones or line in"), PAGE SELECTOR (A-8626 1P8T + pink A-6623 chicken head at (8, 50), print 1..8 +
      "page"): it picks the SETTINGS PAGE of whatever tab is open (closest page it has), NOT the tab (user's correction).
      Core: Core::PageSelector(pos) / OpenPage(); firmware: PCF8574 on the Seed I2C (expansion header J21), position k ->
      P(k-1), common -> GND. Logo + name sideways left of the screen, all diamonds pink. Plugin: draggable gain knobs and
      chicken head (click / drag / wheel). Tayda drill + UV re-uploaded (17 holes).
- [x] 49. Analog leveller ON THE CARRIER BOARD (user: "why a breadboard?", 2026-10-09): codec LO -> 4.7k (R80/R85) ->
      LDR || 100k (R81/R86) -> 10 uF (C80/C85) to IGND; TLV9062 WSON-8 2x2 (U16, under the DC jack) followers -> FXR;
      OC1/OC2 = home-made vactrols (3 mm flat-top LED A-8041 + LDR A-7629 in black heat shrink) on the LID side, pins
      under the DC jack (clip flush); LEDs in series from ISO5V_RAW, NPN Q1 + 100R, MCP4725 U15 (iso I2C 0x60).
      J22 stays OPEN now (sim flags bridges). Added to the routed board by add_leveller.py (+ place_extra.py,
      route_one.py), old tracks untouched; DRC clean (3 accepted courtyard overlaps). Sim: dark 0.04 dB loss,
      1.3 mA -12 dB, 7.8 mA -21 dB, DC at the jack < 5 mV. Firmware: pg::AnalogLeveller, V = 0.65 + 0.1 x mA, dark 10 s.
      JLC: +2 extended parts (C144198, C2058009) ~ +$6 fees + ~$5 parts. Tayda cart 21 lines (breadboard parts dropped).
- [ ] 50. Board simulator in the plugin (board view + wiring editor), fast power-up: see 42.
- [x] 51. 2026-10-09 evening: power switch = A-8233 (RS16 2P4T) + 2nd pink chicken head at face (-47, 2), "0 / 1",
      in the dc + wire (pos 1 off, 2-4 on); logo/name shrunk into y 17-33; Tayda re-uploaded (18 holes), cart 22 lines.
      Header holes re-centred (no press-fit offset), "pg-line"/"pg-hp" silk -> "in gain knob"/"out gain knob".
      Scenario sims (scratchpad simt/scen*.cpp) found 3 things, all value-only fixes + one new part:
      R33/R43 1M->470k (input clean to 3.4 V rms), R34/R44 20k->4.7k (ADC range used; kInGain 0.1883),
      D63 1N4148WS + R70 1M (quick off/on thump 1.1 V -> 2 mV). Sim: Newton stall tolerance 1e-8 -> 1e-11 (1e-8 let a
      0.25 V false DC through at 250 ohm; 1e-14 too slow). Tried R6 3.3k (bias 2.48 V): solver can't settle it -> kept 10k.
      make_jlc now checks every footprint against the routed board. Plugin: power knob (click = off: silence, dark).
- [x] 52. 2026-10-09 night: hard limits. Bias centred (R6 3.3k; the sim's start-up search now scales its pretend
      capacitors per node, which is what made this solvable), R34/R44 12k = the input clamp (codec pin +0.02..+1.98 V
      for any input; analog clips at -4 dBFS), R31/R41 DNP (1.46 Mohm for guitars), output ceiling 3.0 V peak (TPA rails),
      firmware IsoCodec::Guard (codec gains re-checked every second, PGA <= 20 dB), kInGain 0.1152.
      Face: measured the RS16 (45 deg clicks, 16 mm body) and the chicken heads: pink 32 mm can't work on a 4-click
      switch (sweeps over the screen) -> both switches A-8233 with black A-6741 (23.4 x 16.5) at (-16, 51.5) power and
      (16.4, 51.5) page 1..4; check_knob_sweeps proves 5.4 mm clearance over the whole travel. 9 V jack -> LEFT wall
      (side C, y -34) at the user's request. Tayda re-uploaded; cart 21 lines.
- [x] 53. Input clamp settled: ngspice (scratchpad spice/chain.cir: the isolated path, behavioural op-amps with rails)
      showed a long one-sided hit swings the codec pin past its window with R34 12k (-0.42 / +2.26 V: the 1 uF in front
      of the pin remembers the hit). Schottky clamps at the pins (BAS40W-04) didn't fit even with a local re-route.
      -> R34 / R44 20k: worst over 5-150 ms +-12/+-24 V hits, released or reversed, 5 V up to 5.3 V: -0.02 .. +1.85 V.
      kInGain 0.0807. The board simulator mis-solves sustained +-12 V DC at the input (output side goes to nonsense);
      ngspice shows the real output untouched (< 20 mV) - a known limit of our simulator, not the board.
- [x] 54. 2026-10-10: no Mouser. JLCPCB fits only U3 (B0505S, ~$8 with fees for the 2 boards = JLC's minimum PCBA qty,
      one design); the 2 Neutrik jacks from Amazon (B00FV23QH6), soldered by the user; gain pots Tayda A-8618.
      PINK chicken heads back (A-6623, 32 x 19.5): power (-14, 56) "0" at 205 deg, page (14, 54) pages 1-4 from 125 deg;
      1.8 mm apart at the closest along their whole travel, every label on the face, pointers up to 6 mm past the top
      edge (above the face; the jack plugs are below it). Tayda re-uploaded; cart 22 lines $56.69.
- [x] 55. 2026-10-10: knobs level (both y 56, x +-14; power "0" 205 deg, page 1 at 115 deg, 4.0 mm clear), thin arcs
      along each dial's travel on the print, labels kept under the border. LDR A-5800 out of stock -> A-7629 (LXD5528A,
      560 nm). XYG PCF8574 board out of stock -> any PCF8574 board (NOYITO / DEVMO / Comimark). Tayda re-uploaded.
- [x] 56. 2026-10-10: border pushed out (ART - 2 mm) so the gain dot rings clear it; check_corner_posts() audits every
      body behind the face against the 4 lid-screw posts (tightest: gain pots 0.8 mm, pins pointing down). Leveller light
      pairs made OPTIONAL (out of the cart: A-7629 / A-8041 / A-4918; pads stay). Comimark PCF8574 3-pack OK. Cart 19 lines.
- [x] 57. 2026-10-10: leveller light pairs back in the cart. Level lights: one 2-leg red/green LED (A-1076) per side in a
      chrome bezel (A-661, 5.7 mm hole, nut) at (+-48, 37) under the gain knobs (only spot with room: the right one is
      1.7 mm from the Seed3 block), driven by PCF8574 P4/P5, P6/P7 with 330R pull-ups. Firmware: P_LEV_ON / P_LIGHTS on
      the input tab's page 2 + GraphLeveller live view; plugin simulates both and lights its 3D LEDs. Full point-to-point
      wiring drawing (_Tools/pg_wiring.py -> wiring-diagram.svg/png/pdf + WIRE-LIST.md, 92 wires). Fixed: ground chain
      and J10 gnd both claimed socket 40 (chain now spliced onto that jumper); guide said JLC fits the jacks (it doesn't).
- [x] 58. 2026-10-10: Minecraft-style pixel gems (deep metallic sea blue CMYK, white glints left unprinted) replace
      every diamond; divider line full width both sides. UV PDF now has 2 layers: CMYK + RDG_GLOSS (copy of everything,
      Separation /RDG_GLOSS) -> Tayda template saved with gloss Varnish, no white. Docs dated / de-staled (8-way,
      exp jack, free pins). 3D wiring (_Tools/pg_wiring3d.py -> wiring-3d.pdf, 7 pages); flat drawing names the far end
      in words. Headphones: 2.1 V rms ceiling -> 600 ohm gets 7.5 mW.
