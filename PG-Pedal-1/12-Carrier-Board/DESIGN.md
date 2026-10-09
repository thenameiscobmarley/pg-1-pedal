# PG-1 carrier board (v3: isolated)

One board, made and fully assembled by JLCPCB (every part, the 2 jacks too) from the files in `jlcpcb/`. It hangs
from the nuts of the two 1/4" jacks on the top wall (no extra screws) and does all of the pedal's audio.

**One stereo input, one stereo output, both isolated:** nothing you plug in has a wire path into the pedal (or into
the other jack's gear through the pedal's power), not even ground. The audio crosses to the Seed3 as digital data.

```
            ISOLATED SIDE (its own power, ground IGND)                   |  PEDAL SIDE (9 V, ground GND)
in jack ─ TVS ─ 1k ─ 100 nF ─ 1M/1M ÷2 ─ TLV9062 buffer ─ pg-line stage (−33..0..+33 dB) ─ ÷3 ─► codec IN2 (fixed gain)
                                     TLV320AIC3204 codec ◄═ I2S ═╪═ ISO7741 ═╪═ Seed3 SAI2 (pins 32-35)
                                                         ◄═ I2C ═╪═ ISO1540 ═╪═ Seed3 I2C1 (pins 12, 13): set-up only
codec line out ─ fx loop (J22: 2 jumper wires or caps, or an add-on) ─ 4.7 µF ─ pg-hp stage (−21..0..+21 dB) ─ TPA6139A2 ×2 ─ 1 Ω ─ TVS ─ out jack
power: B0505S (isolated 5 V, across the barrier) ─ LP2985 3.3 V ─ codec / headphone amp; filtered 5 V ─ buffer
```

## What's hardware and what's software
- **Both levels are analog.** pg-line (input gain) and pg-hp (output gain) are centre-click dual LINEAR pots that set
  the gain of op-amp stages in the signal path: the pot sits between two end resistors in an inverting stage, so
  gain = (R_end + R_one side) / (R_end + R_other side). **Centre click = unity** ("whatever comes in goes out,
  processed"); left = less (to −33 / −21 dB), right = more (to +33 / +21 dB). The face print's dots show the dB.
- **Hardware only (no software can change it):** both levels, the isolation (ISO7741, ISO1540, B0505S and a no-copper
  strip across the board), the overvoltage clamps on both jacks, the 1 kΩ current limit on the input, the output ceiling
  (~1.1 V rms: the output chips run on 3.3 V), the headphone amp's short-circuit / thermal shutdown, the fuse,
  reverse-polarity and surge protection on 9 V.
- **fx loop (J22):** the codec's output goes out to J22 and comes back into the pg-hp stage. Two short female/female jumper wires or jumper caps (on
  "s l"-"r l" and "s r"-"r r") close it: **without them there's no sound.** A future add-on board (an analog filter /
  VCA, etc.) plugs in there instead and gets the isolated 3.3 V, ground and the codec's I2C, so adding to the pedal
  never needs a new order of this board. The firmware already has a driver for an MCP4461 add-on (pg::AnalogFx).
- **Software:** only the codec's one-time set-up (fixed gain, no auto-ranging), an add-on board's settings (if any) and the DSP. If the firmware hangs, a
  hardware watchdog resets the Seed3 in ~2 s and the codec mutes itself when the digital audio stops. If the input is
  too hot for where pg-line is, the screen's health page says "input clip": turn pg-line left.

## Levels
- **In:** guitar (sees ~670 kΩ, like any pedal), a line output, a headphone output (up to ~2 V rms) or a mic (pg-line
  turned right). Mic quality is "works", not studio (the guitar-friendly input adds some hiss at full gain).
- **Out:** headphones (16-600 Ω) or a line input.
- **Pots:** dual 10k LINEAR with a centre detent, panel mount, e.g. Alps **RK09L1240015** (9 mm, Mouser) or Bourns
  PTM902-125S-103B2. Both gangs used (left / right). Each header pin is labelled with the pot pin that goes on it: pot pin 3 (the clockwise end) on header pin 1, so turning right = more.

## Headers (male, 2.54 mm, right-angle: they point off the board's edge so the jumpers lie flat)
- **seed3 + 9v** (10, pedal side): dc +, dc − (from the panel DC jack), then vin → Seed3 39, gnd → 40, scl → 12,
  sda → 13, sck → 35, fs → 34, tx → 33, rx → 32.
- **pg-line** (6, isolated side, under the in jack): printed 3 l, 2 l, 1 l, 3 r, 2 r, 1 r = the pot pin for each header pin, left gang then right.
- **pg-hp** (6, isolated side, under the out jack): the same for pg-hp.
- **fx loop** (2 x 4 straight, isolated side): s l, r l, s r, r r, 3v3, gnd, scl, sda. Jumper caps on s-r pairs.
- **expansion** (4, pedal side, left of seed3 + 9v): 3v3, gnd, scl, sda (the Seed3's I2C) for a module in the
  expansion bay (a slot in the left wall with 4 glued jumper ends).

## The board
- **Two worlds:** the isolated side is the strip under the jacks (and the tongue above the dashed barrier line); the
  pedal side is the tongue below it. Only the three barrier parts cross the line. Each side has its own ground pour.
- **Jobs in boxes:** every part sits in its job's printed box (input, codec, fx loop, output, iso power, isolation, power),
  arrows show the signal's path, inputs on the left, output on the right (as you look at the pedal's face).
- **All parts on the top side** (with the jacks): JLCPCB assembles one side.

## You solder 8 through-hole parts (big pins, easy: ~50 joints)
JLCPCB fits every small part. You solder the 2 jacks (Neutrik NMJ6HFD2), the B0505S-1WR3 power block (4 pins), the
right-angle headers (10 + 6 + 6 + 4 pins: snap them off a 2.54 mm right-angle male strip) and the 2 x 4 straight
header (fx loop), all from the printed (top) side, then bridge the fx loop with 2 short female/female jumper wires (s l to r l, s r to r r). Leaving them off the
order saves their per-part-type fees and the hand-soldering charges.

## Ordering at JLCPCB
Files in `jlcpcb/` (`python3 make_bom.py && python3 make_board.py && python3 make_jlc.py`). At https://cart.jlcpcb.com/quote:
1. "Add gerber file" -> `pg1-carrier-gerbers.zip`. Board settings as in the chat (2 layers, 1.6 mm, 5 pcs; black for looks).
2. **PCB Assembly**: Economic, **Top side**, PCBA qty: the lowest it allows, Confirm Parts Placement: Yes.
3. BOM -> `pg1-carrier-bom-jlc.csv`, CPL -> `pg1-carrier-cpl-jlc.csv`. Every part should show as matched.

## The jacks (Neutrik NMJ6HFD2: plastic nose)
- 1/4" stereo TRS. The **plastic** nose means the jack's sleeve never touches the metal box: that's what keeps the
  isolated ground isolated. Top-wall holes 11.2 mm (+ powder coat), same pins as the NMJ6HCD2 (KiCad footprint
  `Jack_6.35mm_Neutrik_NMJ6HFD2_Horizontal`), jack centre 8.14 mm above the board.
- Top wall: in -34, 9v -13 (panel DC jack, above the board), out +34 (clear of the box's corner lid-screw posts).

## Old versions
`carrier_v2.py` = the 4-jack analog board (line in, line out, no amp, phones), not isolated.
