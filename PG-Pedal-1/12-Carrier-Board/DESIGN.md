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
codec line out ─ 4.7k ─ [LDR to ground, lit by the leveller's LED] ─ TLV9062 follower ─ 4.7 µF ─ pg-hp stage (−21..0..+21 dB) ─ TPA6139A2 ×2 ─ 1 Ω ─ TVS ─ out jack
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
- **The analog leveller (2026-10-09):** between the codec's line out and pg-hp. Per channel: 4.7 kΩ in series, then an
  LDR to ground (through 10 µF, so the codec's DC stays off it), then a TLV9062 follower (U16, 2 x 2 mm WSON: it fits
  under the DC jack). Each LDR is lit by its own red LED (OC1 / OC2: a 3 mm flat-top LED pressed on a 5 mm LDR in black
  heat shrink, on the lid side, the only parts you make). The two LEDs are in series from the isolated 5 V, so both
  channels get the same light; an MCP4725 DAC (U15, isolated I2C 0x60) sets their current through an NPN (Q1):
  I = (Vdac − 0.65 V) / 100 Ω, ~8 mA at most. The DAC powers up at 0 V: dark LDRs (megohms) = the sound untouched.
  The firmware (pg::AnalogLeveller) rides it 2:1 above −14 dBFS rms. Added to the routed board by add_leveller.py
  (+ place_extra.py / route_one.py for R81 / R86 and two links Freerouting gave up on).
  100 kΩ across each LDR (R81 / R86): the 10 µF charges in ~1 s; with the LDR alone (~1 MΩ dark) it took ~10 s and the
  first squeeze after power-up thumped (found by the simulation). The firmware also keeps it dark for 10 s.
- **fx loop (J22):** stays on the board but **open**: the leveller's followers drive its returns (r l, r r), so a
  bridge would short a follower's output to the codec's. Its 3v3 / gnd / scl / sda are still there for an add-on.
- **Software:** only the codec's one-time set-up (fixed gain, no auto-ranging), an add-on board's settings (if any) and the DSP. If the firmware hangs, a
  hardware watchdog resets the Seed3 in ~2 s and the codec mutes itself when the digital audio stops. If the input is
  too hot for where pg-line is, the screen's health page says "input clip": turn pg-line left.

## Checked by simulation (10-Carla-Plugin/Source/sim, the routed board's real netlist)
- Powers up to +9V 8.22, +5V 5.00, +3V3 3.30, iso 5V 5.05, iso 3V3 3.30, bias 1.65 V; ~197 mA from the 9 V.
- Input (centre click): flat 100 Hz - 20 kHz, clean up to ~2.2 V rms at the jack; pg-line -34 .. +29 dB.
- Output (centre click): 0 dB, flat, ~1.9 V rms / 115 mW into 32 ohm, ~2 V rms into a mixer, ~1 mV DC; pg-hp +-20 dB.
- **C31 / C41 are NOT fitted** (their pads stay empty): 100 pF on the input's 500 k bias divider made a 3.2 kHz
  low-pass (-13 dB at 10 kHz). Found by the simulation; radio is still kept out at the jack.

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
- **fx loop** (2 x 4 straight, isolated side): s l, r l, s r, r r, 3v3, gnd, scl, sda. Leave it open.
- **expansion** (4, pedal side, left of seed3 + 9v): 3v3, gnd, scl, sda (the Seed3's I2C) for a module in the
  expansion bay (a slot in the left wall with 4 glued jumper ends).

## The board
- **Two worlds:** the isolated side is the strip under the jacks (and the tongue above the dashed barrier line); the
  pedal side is the tongue below it. Only the three barrier parts cross the line. Each side has its own ground pour.
- **Jobs in boxes:** every part sits in its job's printed box (input, codec, fx loop, output, iso power, isolation, power),
  arrows show the signal's path, inputs on the left, output on the right (as you look at the pedal's face).
- **All parts on the top side** (with the jacks): JLCPCB assembles one side.

## Headers: soldered (the holes hold them straight first)
The 5 header footprints have **press-fit ("locking") holes**: every other hole sits 0.127 mm off the line, so a
plain 2.54 mm header strip grips by friction and stays straight and flat while you solder it. They're soldered (your
choice: the pedal gets moved and knocked around), not just pressed in.

## You solder the through-hole parts (big pins, easy)
JLCPCB fits every small part. You solder the 2 jacks (Neutrik NMJ6HFD2) and the B0505S-1WR3 power block (4 pins),
from the printed (top) side, the headers, and the leveller's 2 LED + LDR pairs (from the back). The
fx loop stays open. Leaving them off the order saves their per-part-type fees and the hand-soldering charges.

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

## Changed after the scenario simulations (2026-10-09, values only, no re-route)
- **R33 / R43 1M -> 470k** (C25790, preferred part): the input divider is x0.32, not x0.5; clean input up to ~3.4 V rms
  (was 2.1 V rms: a +10 dBu pre-amped source clipped).
- **R34 / R44 20k -> 4.7k**: x0.68 to the codec, not x1/3; the ADC's top 8 dB were never used (more hiss). Now
  pg-line's stage clips just before the ADC's full scale. Firmware kInGain = codec volts per jack volt = 0.1883.
- **D63 (1N4148WS, C2128) + R70 470k -> 1M**: with the power switch, off then quickly on left C73 charged and the
  headphone amp un-muted at once: a ~1 V thump. D63 empties C73 when the power goes; the jack now moves < 1 mV.
- Header holes re-centred in their pads (the press-fit offset is gone: the headers are soldered).


## Hard limits (2026-10-09, user: "if the DSP ever glitches it physically can't output that")
- **Bias centred: R6 10k -> 3.3k** (IBIAS 2.48 V, the middle of the op-amps' 5 V). Every stage swings the same both
  ways; the input buffer is clean to ~5.4 V rms at the jack.
- **Input clamp = R34 / R44 20k** (with R35 / R45 10k and the codec's 20k): whatever the pg-line op-amp does, even
  slammed rail to rail right after a long one-sided hit (the 1 uF in front of the pin remembers the hit), the codec's pin
  stays inside -0.02 .. +1.85 V (safe window -0.3 .. AVDD + 0.3 = 2.1), checked in ngspice over hits of 5-150 ms,
  +-12 / +-24 V, released or reversed, the isolated 5 V up to 5.3 V. (12k let it reach -0.42 / +2.26 V; Schottky
  clamps at the pins didn't fit.) The analog clips at about -7 dBFS of the ADC, so the Seed3 never gets an over.
- **Output ceiling**: the headphone amp runs on +-3.3 V (its own charge pump): the jack can never pass 3.0 V peak =
  2.1 V rms, whatever the DSP or a codec register does (86 mW into 50 ohm). The firmware also checks the codec's line
  out / DAC / PGA gains every second and puts them back (IsoCodec::Guard, PGA never above 20 dB).
- **The leveller can only cut** (an LDR shunt), its LED current tops out at ~11 mA (LEDs rated 20 mA).
- **Guitar**: R31 / R41 not fitted: the input is 1.46 Mohm, flat to 10 kHz (with them 595k, ~2.4 dB less sparkle).
- The 9 V jack moved to the left wall: nothing hangs over the board now (DC_ZONE empty).
