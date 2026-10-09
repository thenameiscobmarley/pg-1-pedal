# PG-1 build guide (2026-10-09, carrier board v3)

This is the one to follow. It replaces the jack / 9 V / pot / "free pins" parts of `05-Wiring-and-Schematics/WIRING.md`,
which were written before the carrier board existed. Pin numbers here come straight from the firmware
(`06-Firmware-DaisySeed/pg1/src/main.cpp`) and the routed board, and the board itself was checked in simulation.

---

## 0. What to order (today)

| Where | What |
|---|---|
| **JLCPCB** | the carrier board: upload `12-Carrier-Board/jlcpcb/` (gerbers zip, BOM csv, CPL csv). PCB qty 5, PCBA qty 2, Economic, Top side, Confirm Parts Placement yes, Global Standard Direct Line shipping. Dry-ice cleaning yes, bake no, function test no. |
| **Tayda** (`02-Parts-and-Cart/tayda-cart-import.csv`, 28 lines) | box, screen, encoders, knobs, footswitches + pink caps, 9 V jack, 2 packs female/female jumpers, screen screws / spacers / nuts, black wire, cable ties, the 2 header strips, the page selector + pink chicken head, the analog leveller parts + mini breadboard |
| **drill.taydakits.com** | the drill + UV print job (templates already uploaded) |
| **Mouser** (or as noted) | 2 x Alps **RK09L1240015** (dual 10k linear, centre click), 2 x Neutrik **NMJ6HFD2** jacks, 1 x **B0505S-1WR3** (also sold on Amazon, often 2-packs; any brand with the same 4 pins: GND, Vin, 0 V, +Vo) |
| **Amazon** | PCF8574 I/O board (page selector), MCP4725 DAC board (leveller), B0505S-1WR3 (if not from Mouser) |
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

## 5. The two gain pots (pg-line = Input Gain, pg-hp = Output Gain)

They sit **under the jacks** now: Input Gain under "in" (top left), Output Gain under "out" (top right). Right behind
them is the carrier board, only ~9 mm away, so **solder short wires to the pot pins** (not jumper plugs) and bend
them flat; each wire ends in a female jumper that goes on the header right next to it.

Each pot is a **dual** pot: 2 rows of 3 pins (front gang + back gang). The header pins are printed with the **pot pin
that goes there**: `3 l, 2 l, 1 l, 3 r, 2 r, 1 r` ("l" = the front gang, "r" = the back gang).

Find the pot's pins 1 and 3 with the multimeter (ohms), knob turned fully LEFT:
- the middle pin of each row is **2** (the wiper).
- of the two outer pins, the one with **~0 ohm to the middle pin** (when turned fully left) is **pin 1**; the other is **pin 3**.
  (Turn it right: pin 1 to middle goes up to ~10k. That's the check.)

- **Input Gain** pot (under "in") -> the **pg-line** header (also under "in").
- **Output Gain** pot (under "out") -> the **pg-hp** header (also under "out").

## 6. The fx loop and the expansion port

- **fx loop** (2 x 4): put 2 short female/female jumpers from **s l to r l** and from **s r to r r**.
  **No sound without them** (or with the analog leveller, section 6c, in their place).
- **expansion** (4 pins: 3v3, gnd, scl, sda): 4 jumpers to the **expansion bay** slot in the LEFT wall. Push their
  female ends into the slot from inside until flush, hot glue them on the inside (warm the wall with a hair dryer
  first). Note which colour is which pin. Modules plug onto them from outside later.

## 6b. The page selector (chicken head, 8 positions)

Whatever tab is open, the selector picks its **settings page**: position 1 = page 1, 2 = page 2... A tab with fewer
pages just shows its last one. Without the selector fitted, swiping / pg-b still change pages.

- **Switch (A-8626)**: 8 position lugs around the edge + a common lug. Find the common with the multimeter (beep mode):
  it beeps to exactly one outer lug, and which one changes as you turn. Turn it fully **left** (= position 1, the pink
  dot) and note the lug that beeps: that's lug 1; turning right goes 2, 3, ... 8.
- Snap off the anti-rotation tab, mount it (9 mm hole) with its nut, push on the pink chicken head pointing at "1"
  when turned fully left.
- **PCF8574 board** (Amazon, pins on): solder 9 wires to the switch: lug 1 -> **P0**, lug 2 -> **P1**, ... lug 8 ->
  **P7**, common -> **GND** (female jumper ends on the PCF8574 side). Bend them flat (the carrier board is ~6 mm behind
  the switch).
- PCF8574 input header -> the carrier's **expansion** header: VCC -> 3v3, GND -> gnd, SDA -> sda, SCL -> scl.
- If the PCF8574 has a second (pass-through) header, run the 4 wires to the left-wall expansion bay from there instead.
- Velcro the PCF8574 to the inside of the lid. The firmware finds it by itself (any address).

## 6c. The analog leveller add-on (fx loop, mini breadboard)

What it does: the DSP measures what's leaving and, above about -14 dBFS, turns up two LEDs a little (2:1); each LED
shines on a light-dependent resistor that gently pulls its channel's level down in the analog path (the classic
smooth "opto" compressor). Dark LEDs = untouched sound. It replaces the 2 fx-loop bridges.

**Parts:** MCP6002 (8-pin chip), 2 LDRs, 2 red LEDs, 2 x 4.7k, 2 x 470 ohm, 2 x 10 uF, 1 x 100 nF, black heat shrink,
the MCP4725 board (Amazon), the 170-point mini breadboard, U-shape wires.

**Which way round:**
- **MCP6002**: the notch (or dot) is pin 1's end. Put it across the middle gap with the **notch to the LEFT** (column 7):
  then pins 1 2 3 4 are the bottom row (row f, columns 7-10) and pins 8 7 6 5 the top row (row e, columns 7-10).
- **10 uF capacitors**: the **long leg (+)**, the stripe on the side is -. + goes where the table says +.
- **LEDs**: the **long leg is +** (anode); the flat side of the rim is -.
- Resistors, LDRs and the 100 nF: either way round.

The board: columns 1-17 left to right, rows **a-e** (top half) and **f-j** (bottom half); each column's 5 holes in a
half are joined. Left channel on the bottom half, right channel on the top half.

| part | from hole | to hole |
|---|---|---|
| MCP6002 (notch left) | pin 1 = f7 ... pin 4 = f10 | pin 8 = e7 ... pin 5 = e10 |
| U-wire (left follower) | i7 | i8 |
| U-wire (right follower) | c8 | c9 |
| 4.7k (left) | h1 | h9 |
| 4.7k (right) | b1 | b10 |
| LDR (left) | i9 | i12 |
| LDR (right) | c10 | c12 |
| 10 uF (left) | **+ j12** | - j13 |
| 10 uF (right) | **+ d12** | - d13 |
| U-wire (ground) | g10 | g13 |
| U-wire (ground across the gap) | e13 | f13 |
| 100 nF | b7 | b13 |
| LED (left) | **+ h15** | - h14 |
| U-wire (left LED -) | i13 | i14 |
| 470 ohm (left) | g15 | g17 |
| LED (right) | **+ b15** | - b14 |
| U-wire (right LED -) | a13 | a14 |
| 470 ohm (right) | c15 | c17 |
| U-wire (DAC to both 470s) | e17 | f17 |

Jumpers (female/female):

| from | to |
|---|---|
| fx loop **s l** | j1 |
| fx loop **r l** | j7 |
| fx loop **s r** | a1 |
| fx loop **r r** | b8 |
| fx loop **3v3** | a7 |
| fx loop **gnd** | j10 |
| fx loop **sda** / **scl** | MCP4725 SDA / SCL |
| MCP4725 VCC | d7 |
| MCP4725 GND | c13 |
| MCP4725 VOUT | a17 |

**The light-tight pairs:** bend each LED so its dome touches the face of its LDR (left LED -> left LDR at i9-i12,
right LED -> right LDR at c10-c12), slide a 15 mm piece of the black heat shrink over LED + LDR together and shrink it
(hair dryer or the iron's barrel, not the tip). No outside light may get in, or the level will drift.

**Check before plugging into the pedal:** beep test: no beep between a7 (3v3) and j10 (gnd).
With it plugged in, the sound should be exactly as with the bridges until it gets loud; then it eases down smoothly.

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
