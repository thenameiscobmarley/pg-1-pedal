# PG-1 carrier board (v2)

One board, made and assembled by PCBWay from the files in this folder. It hangs from the nuts of the four 1/4" jacks
along the top wall (no extra screws), carries the analog audio (buffers, line driver, headphone amp, protection, power),
and has male pin headers for everything else, so every wire to the Seed3, the screen, the knobs and footswitches is a
plug-on jumper. Nothing gets soldered except the jack, pot, encoder and footswitch lugs at the part end (and the 4 jacks,
which are big through-hole pins, if PCBWay doesn't fit them).

## What it does (signal flow, each channel)

```
line in jack ─ 1k ─┬─ clamp diodes to ±7.5 V ─ 10 µF ─ input stage (OPA1652, inverting, ×0.5, 20 kΩ in) ─► Seed3 AUDIO IN (16/17)
                    └─ ESD/TVS
Seed3 AUDIO OUT (18/19) ─ 10 µF ─ output buffer (OPA1652, inverting, ×1) ─┬─► no amp:  OPA1622, ×2  ─ 1 Ω ─► "no amp" jack
                                                                         ├─► pg-line pot (10k log) ─► OPA1652, ×2 ─ 100 Ω ─► "line out" jack
                                                                         └─► pg-hp pot (10k log) ─► OPA1622, ×4 ─ 1 Ω ─► "phones" jack
```
- **Levels.** The input stage takes 2 V rms (a hot headphone output) down to the Seed3's 1 V rms full scale. The outputs
  give the 6 dB back, so **no amp = the same loudness that came in** (unity), **line out** = up to that (pg-line turns it down),
  **phones** = up to **+12 dB** over the Seed3's output (~4 V rms: plenty for 50 Ω now and 200 Ω later; silent at the bottom).
  The two inversions cancel, so every output has the input's polarity.
- **Low noise.** OPA1622 (2.8 nV/√Hz, made for headphones, drives 32 Ω and up cleanly) for phones and no-amp; OPA1652 (4.5 nV/√Hz)
  everywhere else; ±7.5 V rails from low-noise regulators (TPS7A4901 / TPS7A3001, ~15 µV rms). Expected noise at the phones
  jack with the volume up: well under 10 µV (inaudible on any headset).
- **No thumps.** The OPA1622s have an enable pin: held off for ~1 s at power-up (RC + comparator) and switched off at once when
  the 9 V drops, so plugging / unplugging power doesn't pop in your ears.
- **Protection.** 1 kΩ + clamp diodes to the rails on both inputs (nothing above the rails can reach the Seed3; its inputs
  take ±1.8 V at most), TVS diodes on every jack contact for static, a Schottky in the 9 V line (reverse polarity), a PTC fuse,
  outputs short-circuit proof (OPA1622/OPA1652 current limit + series resistors), DC-coupled outputs with < 1 mV offset.
- **Power.** 9 V jack → Schottky + PTC → +9 V (also feeds the Seed3 VIN through the board) → TPS7A4901 → +7.5 V;
  LT1054 charge pump → −8 V → LC filter → TPS7A3001 → −7.5 V. About 40 mA extra at normal listening levels.

## Headers (male, 2.54 mm, for female jumpers)
- **seed port** 2×20: the Seed3's pins 1-40 in the Seed3's own order (40 female-female jumpers from the glued jumper block in
  the right wall plug straight on). The board routes them to the part headers below.
- **screen** 1×14 (same order as the screen's pins: jumpers plug straight across).
- **pg-1 .. pg-4** 1×5 each (A, C, B, push, push-2), **pg-a .. pg-c** 1×2 each, **pg-hp / pg-line** 1×6 each (the dual pots).
- **9V** 1×2 (from the panel DC jack), **gnd** test pins, and a 1×4 **spare** (pins 34, 35, 36, 37).

## Mechanics
- Hangs from the 4 PCB-mount jacks (Tayda A-1122): their bushings go through the top wall, the nuts clamp the board in place.
- Outline: a 110 mm wide strip under the jacks, reaching down over the screen's top edge on the left half only
  (keeps clear of the screen's 14-pin header and of the Seed3 block on the right wall). Height above the lid ≥ 10 mm.

## Status
- [x] circuit and levels (this file)
- [ ] board: needs KiCad (`sudo pacman -S kicad kicad-library jre-openjdk`), then the netlist in `carrier.py` -> board ->
      autoroute (Freerouting) -> Gerbers + BOM + placement file for PCBWay assembly

## The jacks (Tayda A-1122, datasheet in 07-Datasheets)
- Bushing **M11** (top-wall holes 11.2 mm + powder coat), face plate 15.7 mm, body 21 + 3.8 mm long, 16.2 mm wide (20 mm over the pins).
- Jack centre **8 mm above the board**: the board's top surface sits 26 mm below the face surface, leaving ~8 mm to the lid.
- Pins (1.6 mm holes) in two rows 16.2 mm apart: T / R / S on one side, the switch contacts TS / RS / SS on the other,
  4.2 + 6.3 + 6.3 mm from the front. The switch contacts aren't used.
- Spaced 21 mm apart on the wall (line in -42, line out 0, no amp +21, phones +42; the 9V panel jack at -21 sits above the board).

## Cost (rough, before quotes)
PCBWay: 5 bare boards ~$5-10; assembly of 2 boards: setup + stencil ~$30-50, parts ~$26 a board (OPA1622 x2, OPA1652 x3,
TPS7A4901, TPS7A3001, LT1054 are most of it), shipping ~$20-30. **About $110-130 for 2 finished boards.**
A cheaper version (NE5532 op-amps, a single-chip headphone amp with its own charge pump) would be about $50-70, but with a
lower headphone maximum (~1 V rms instead of ~4 V rms) and a bit more noise.

## To check before ordering
- LT1054 pinout against its datasheet (analog.com didn't load from here): assumed 1 FB/SHDN, 2 CAP+, 3 GND, 4 CAP-, 5 VOUT, 6 VREF, 7 OSC, 8 V+.
- Tayda's preview: the Seed3 window must be on the right wall (side E) beside the screen.
