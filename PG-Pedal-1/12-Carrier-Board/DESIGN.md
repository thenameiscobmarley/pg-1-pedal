# PG-1 carrier board (v2)

One board, made and assembled by PCBWay from the files in this folder. It hangs from the nuts of the four 1/4" jacks
along the top wall (no extra screws), carries the analog audio (buffers, line driver, headphone amp, protection, power),
and has male pin headers for its wires (Seed3 audio + power, the two small pots, 9 V), so they are plug-on jumpers.
Nothing gets soldered on it by you if PCBWay also fits the 4 jacks and the headers (send them the jacks).

## Budget version (chosen 2026-10-08): what it does, each channel

```
line in jack ─ TVS ─ 1k ─ 10 µF ─ NE5532 inverting ×0.5 (around 4.5 V) ─ 10 µF ─► Seed3 AUDIO IN (16/17)
Seed3 AUDIO OUT (18/19) ─ 10 µF ─ NE5532 follower ─ 10 µF ─ BUS (0 V centred) ─┬─► TPA6139A2 ×−2 ─ 1 Ω ─► "no amp" jack
                                                                              ├─► pg-line pot ─ NE5532 ×−2 ─ 10 µF ─ 100 Ω ─► "line out" jack
                                                                              └─► pg-hp pot ─ TPA6139A2 ×−4 ─ 1 Ω ─► "phones" jack
```
- **Levels.** The input stage halves the level, so up to 2 V rms (a hot headphone output) fits the Seed3's 1 V rms; above
  ~2.5 V rms in it starts to clip (don't feed it a power-amp speaker output). **no amp** gives the 6 dB back = the same loudness
  that came in. **line out** = up to that (pg-line turns it down). **phones** = pg-hp from silent to +12 dB over the Seed3,
  clean up to ~1 V rms into 50 Ω and ~2 V rms into 200 Ω (the TPA6139A2's limit: loud on any headset).
- **Low noise.** NE5532 (5 nV/√Hz) and the TPA6139A2 (DirectPath, about 6 µV rms output noise, no output capacitors, no thump)
  on a quiet 3.3 V regulator (LP2985, 30 µV). Hiss at the phones jack is far below anything you can hear.
- **No thumps.** Both TPA6139A2s are held muted until ~0.5 s after power-up and mute at once when the 9 V goes (TLV7031).
- **Protection.** A 15 V TVS pair on every jack (static), 1 kΩ + 20 kΩ in front of the input op-amp, a Schottky (reverse
  polarity), a 300 mA PTC fuse and a 12 V surge clamp on the 9 V, short-proof outputs (series resistors + the chips' limits),
  pots kept at 0 V (no scratching), 100 kΩ bleeds (no pop when a cable goes in).
- **Power.** One 9 V supply: 9 V → PTC → Schottky → +9 V (op-amps, and the Seed3's VIN through the board); 4.5 V mid-point
  from a divider + 47 µF; LP2985 → +3.3 V for the two TPA6139A2s. Parts cost about $6 a board.
- The full-quality version (OPA1622, ±7.5 V, ~4 V rms phones, ~$26 of parts a board) is kept in `carrier_full.py`.

## Headers (male, 2.54 mm, for female jumpers)
Only audio and power go through the board. The screen, encoders and footswitches plug straight into the Seed3's glued
jumper block, as in v1.
- **seed3** 1×7, right-angle, on the lower edge of the tongue: in l, in r, out l, out r, agnd, 9v, dgnd
  (the Seed3's pins 16, 17, 18, 19, 20, 39, 40).
- **pg-hp** and **pg-line** 1×6 each, right-angle, same edge: bus l, wiper l, gnd, bus r, wiper r, gnd (the dual pots).
- **9v** 1×2, upright on the jack side, right behind the panel DC jack (+, −).
- The right-angle headers sit on the parts side and point off the board's lower edge, so the jumpers lie flat between
  the board and the lid. The pin names are printed beside each pin.

## Mechanics
- Hangs from the 4 PCB-mount jacks (Tayda A-1122): their bushings go through the top wall, the nuts clamp the board in place.
- Outline: a 111 × 27 mm strip under the jacks plus a 62 × 15 mm tongue over the screen's top edge (x −34 .. +28), which
  keeps clear of the small pots on the left, the screen's 14-pin header and the Seed3 block on the right.
- All the small parts are on the **bottom** (the lid side, ~8 mm of room): the jack side is covered by the jack bodies.
  So PCBWay assembles one side only (the bottom).
- Grounds: AGND is poured on both layers. Power ground (GND) and the Seed3's DGND join it at one point each (R70, R71, 0 Ω).

## Status
- [x] circuit and levels (this file)
- [x] board: `python3 make_board.py` (KiCad 10 + Freerouting) places, routes, pours and checks it, and writes `pcbway/`
      (result: every connection routed, 0 DRC errors; only silkscreen warnings, e.g. header outlines past the edge)
- [ ] order (below)

## Ordering at PCBWay
- **PCB:** upload `pcbway/pg1-carrier-gerbers.zip`. 2 layers, 111 × 42 mm, 1.6 mm FR-4, 1 oz copper, any colour,
  HASL lead-free (or ENIG), quantity 5.
- **Assembly:** turn on "PCB Assembly", **bottom side only**, quantity 2. Upload `pg1-carrier-bom.csv` and
  `pg1-carrier-cpl.csv` (placement, 0,0 = the board's lower-left corner). 89 SMD parts + 4 headers.
- **Jacks:** the 4 Tayda A-1122 jacks are not something PCBWay stocks. Either post 4 to them as customer-supplied parts
  (the BOM says so), or tell them "leave J1-J4 unfitted" and fit them later (6 big pins each, easy first soldering).
- `pg1-carrier-top.png` / `-bottom.png` are what it should look like; `pg1-carrier.kicad_pcb` opens in KiCad.
- Remake everything after a change: `python3 make_bom.py && python3 make_board.py` (a few minutes).

## The jacks (Tayda A-1122, datasheet in 07-Datasheets)
- Bushing **M11** (top-wall holes 11.2 mm + powder coat), face plate 15.7 mm, body 21 + 3.8 mm long, 16.2 mm wide (20 mm over the pins).
- Jack centre **8 mm above the board**: the board's top surface sits 26 mm below the face surface, leaving ~8 mm to the lid.
- Pins (1.6 mm holes) in two rows 16.2 mm apart: T / R / S on one side, the switch contacts TS / RS / SS on the other,
  4.2 + 6.3 + 6.3 mm from the front. The switch contacts aren't used.
- Spaced 21 mm apart on the wall (line in -42, line out 0, no amp +21, phones +42; the 9V panel jack at -21 sits above the board).

## Cost (rough, before quotes)
Budget version: 5 bare boards ~$5-10, assembly of 2 (setup + stencil ~$30-40, parts ~$6 a board), shipping ~$20-30:
**about $55-80 for 2 finished boards.** (The full version would be about $110-130.)

## To check before ordering
- Tayda's preview: the Seed3 window must be on the right wall (side E) beside the screen.
