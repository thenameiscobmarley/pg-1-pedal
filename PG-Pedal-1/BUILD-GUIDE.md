# PG-1 build guide (2026-10-09, carrier board v3)

This is the one to follow. It replaces the jack / 9 V / pot / "free pins" parts of `05-Wiring-and-Schematics/WIRING.md`,
which were written before the carrier board existed. Pin numbers here come straight from the firmware
(`06-Firmware-DaisySeed/pg1/src/main.cpp`) and the routed board, and the board itself was checked in simulation.

---

## 0. What to order (today)

| Where | What |
|---|---|
| **JLCPCB** | the carrier board: upload `12-Carrier-Board/jlcpcb/` (gerbers zip, BOM csv, CPL csv). PCB qty 5, PCBA qty 2, Economic, Top side, Confirm Parts Placement yes, Global Standard Direct Line shipping. Dry-ice cleaning yes, bake no, function test no. |
| **Tayda** (`02-Parts-and-Cart/tayda-cart-import.csv`, 15 lines) | box, screen, encoders, knobs, footswitches + pink caps, 9 V jack, 2 packs female/female jumpers, screen screws / spacers / nuts, black wire, cable ties, the 2 header strips (A-199, A-198) |
| **drill.taydakits.com** | the drill + UV print job (templates already uploaded) |
| **Mouser** (or as noted) | 2 x Alps **RK09L1240015** (dual 10k linear, centre click), 2 x Neutrik **NMJ6HFD2** jacks, 1 x **B0505S-1WR3** (also sold on Amazon, often 2-packs; any brand with the same 4 pins: GND, Vin, 0 V, +Vo) |
| **Seed3** | Daisy Seed3 **with headers** |
| **Tools** | soldering iron kit, **0.8 mm rosin-core solder** (right size for all of this), flush **wire cutters**, wire strippers, multimeter, hot glue gun, a little heat-shrink |

## 1. When the parts arrive: check

- **Carrier board**: every small part is already on it. Two pads in the input box are **empty on purpose** (C31, C41:
  leaving them off keeps the treble). The chips have their names printed next to them.
- Count: 2 pots, 2 Neutrik jacks, 1 B0505S, 2 header strips, 4 encoders + knobs, 3 footswitches + caps, screen, 9 V jack.

## 2. Solder the carrier board (big through-hole pins only)

Work on the **printed side up** (the side with the labels): every part goes in from that side and is soldered on the back.
Iron on the pad + pin together 2 s, feed solder, a small shiny cone. Snip the leftover leg with the flush cutters.

1. **Headers** (lowest first, so the board lies flat):
   - snap the right-angle strip (A-199) into **10 + 6 + 6 + 4** pins, snap a **2 x 4** off the double strip (A-198).
   - 10 = **seed3 + 9v**, 6 = **pg-line**, 6 = **pg-hp**, 4 = **expansion**, 2 x 4 straight = **fx loop**.
   - the right-angle pins point **off the board edge** (the printed pin names sit beside them). The holes grip the pins,
     so they stay straight while you solder: one end pin first, check it sits flat, then the rest.
2. **B0505S** (the small black block, 4 pins in a row). **Pin 1 goes in the square pad marked "PIN 1"**.
   The module has its own pin-1 mark (a dot or "1" on the case): match them. Backwards would feed power into its output.
3. **The 2 jacks** (in: left, out: right as you look at the printed side with the jacks at the top). They only fit one
   way. Solder all pins, they also hold the board.

Check with the multimeter (beep mode): no beep between the header's **3v3** and **gnd** pins, and none between
**vin** and **gnd** on the seed3 + 9v header.

## 3. The box

The box comes drilled and printed from Tayda. Fit in this order (more elbow room):
1. Screen: `M3 x 12 screw -> face -> 5 mm spacer -> screen -> M3 nut` (x 4), its 14-pin header end to the right.
2. Encoders (snap off their little anti-rotation tab), footswitches, the 9 V jack, the 2 pots (snap off their tab too).
3. The carrier board hangs from its 2 jacks: push the jack noses through the top wall's in / out holes and screw the
   Neutrik nuts on from outside. Nothing else holds it.

## 4. Wires: carrier board -> Seed3 (the seed3 + 9v header, 10 female/female jumpers)

The Seed3 socket is the jumper-end block in the **RIGHT wall** window (section 7). Each header pin is printed on the board.

| header pin | goes to |
|---|---|
| dc + | the 9 V jack's **sleeve** lug (+) (centre-negative jack: find the lugs with the multimeter, adapter plugged in) |
| dc - | the 9 V jack's **centre pin** lug (-) |
| vin | Seed3 pin **39** |
| gnd | Seed3 pin **40** |
| scl | Seed3 pin **12** |
| sda | Seed3 pin **13** |
| sck | Seed3 pin **35** |
| fs | Seed3 pin **34** |
| tx | Seed3 pin **33** |
| rx | Seed3 pin **32** |

(The two 9 V wires are soldered on the jack's lugs and end in a female jumper on the header.)

## 5. The two pots (pg-line and pg-hp: 6 jumpers each, no solder on the board)

Each pot is a **dual** pot: 2 rows of 3 pins (front gang + back gang). The header pins are printed with the **pot pin
that goes there**: `3 l, 2 l, 1 l, 3 r, 2 r, 1 r` ("l" = the front gang, "r" = the back gang).

Find the pot's pins 1 and 3 with the multimeter (ohms), knob turned fully LEFT:
- the middle pin of each row is **2** (the wiper).
- of the two outer pins, the one with **~0 ohm to the middle pin** (when turned fully left) is **pin 1**; the other is **pin 3**.
  (Turn it right: pin 1 to middle goes up to ~10k. That's the check.)

Then plug female jumpers onto the pot pins and onto the header pin with the same label. Wrong ends = the knob works
backwards (the simulation showed it), so do the multimeter check.

- **pg-line** pot (the lower one, next to the screen, label "pg-line") -> the **pg-line** header (under the IN jack).
- **pg-hp** pot (the upper one, label "pg-hp") -> the **pg-hp** header (under the OUT jack).

## 6. The fx loop and the expansion port

- **fx loop** (2 x 4): put 2 short female/female jumpers from **s l to r l** and from **s r to r r**.
  **No sound without them.** Later an add-on board (an analog effect) plugs in here instead.
- **expansion** (4 pins: 3v3, gnd, scl, sda): 4 jumpers to the **expansion bay** slot in the LEFT wall. Push their
  female ends into the slot from inside until flush, hot glue them on the inside (warm the wall with a hair dryer
  first). Note which colour is which pin. Modules plug onto them from outside later.

## 7. The Seed3 socket (RIGHT wall window) and the controls

Build the socket block exactly as `05-Wiring-and-Schematics/WIRING.md` "The socket, way 1" says (jumper ends on the
Seed3's pins, hot-glued into a 2 x 20 block, pushed into the window from inside). Two changes since that was written:
- the window is in the **RIGHT** wall (USB-C still toward the footswitches).
- **key it with pin 16, not pin 1** (pin 1 now drives the screen's RESET). Pin 16 is the Seed3's own audio input, unused
  now: snip pin 16 off the Seed3 and block socket place 16 with hot glue.

Wire list for the controls (other end of each socket jumper):

| part | Seed3 pin |
|---|---|
| pg-1 encoder A / B / push | 2 / 3 / 4 |
| pg-2 encoder A / B / push | 5 / 6 / 7 |
| pg-3 encoder A / B / push | 14 / 15 / 22 |
| pg-4 encoder A / B / push | 26 / 27 / 28 |
| pg-a / pg-b / pg-c footswitch (one lug) | 24 / 25 / 10 |
| screen 1 VCC | 38 |
| screen 3 CS / 4 RESET / 5 DC | 8 / 1 / 37 |
| screen 6 SDI / 7 SCK / 8 LED | 11 / 9 / 23 |
| screen 10 T_CLK / 11 T_CS / 12 T_DIN / 13 T_DO | 29 / 30 / 31 / 36 |
| screen 9 SDO, 14 T_IRQ | leave empty |
| carrier board (section 4) | 12, 13, 32, 33, 34, 35, 39, 40 |

Unused Seed3 pins: 16-21 (its own audio: not used, the carrier board does the audio).

**Ground**: one chain of black wire, part to part: each encoder's middle pin C + its 2nd push pin -> each
footswitch's other lug -> screen pin 2 (GND) -> socket place **40**. (The old "audio chain" to pin 20 is gone: the
jacks' sleeves are on the carrier board's isolated ground and must **not** be connected to anything else.)

## 8. First power-up

1. **Seed3 out**, carrier board wired. Plug in the 9 V adapter. Multimeter DC volts:
   - expansion header: **3v3 to gnd = 3.3 V**
   - fx loop header: **3v3 to gnd = 3.3 V** (the isolated side)
   - nothing warm. If anything is wrong, unplug and tell me what you measured.
2. Unplug, put the Seed3 in, flash the firmware (USB-C on the right side, `06-Firmware-DaisySeed/README.md`).
3. Power up: the screen starts, the **health** tab should say "all ok".
4. Before the expensive headphones: DC volts across the OUT jack (tip to sleeve) with a cable plugged in: about **0 V**
   (the simulation says ~1 mV). Then plug in at low volume, pg-hp at the centre click.

## What the simulation checked (so you know what to expect)
- Powers up to 9 V 8.2, 5 V 5.00, 3.3 V 3.30, isolated 5 V 5.05, isolated 3.3 V 3.30; draws ~200 mA.
- Input at pg-line's click: flat 100 Hz - 20 kHz, clean up to ~2.2 V rms (headphone outs, hot line levels). pg-line
  -34 .. +29 dB (mics / quiet sources turned right, hot sources left). The DSP's input tab auto-levels on top.
- Output at pg-hp's click: unity, ~1.9 V rms / 115 mW into 32 ohm headphones, ~2 V rms into a mixer, ~1 mV DC.
- Wrong wiring: swapped data / clock / I2C wires, a missing fx loop bridge or a missing Seed3 ground just mean no sound
  (nothing breaks); a reversed 9 V is blocked by D1; a short trips the fuse. **The one wiring mistake that can damage
  parts is 9 V landing on a signal pin**: check the vin wire goes to pin 39.
