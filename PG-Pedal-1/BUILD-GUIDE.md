# PG-1 build guide (2026-10-09, carrier board v3)

This is the one to follow. It replaces the jack / 9 V / pot / "free pins" parts of `05-Wiring-and-Schematics/WIRING.md`,
which were written before the carrier board existed. Pin numbers here come straight from the firmware
(`06-Firmware-DaisySeed/pg1/src/main.cpp`) and the routed board, and the board itself was checked in simulation.

---

## 0. What to order (today)

| Where | What |
|---|---|
| **JLCPCB** | the carrier board: upload `12-Carrier-Board/jlcpcb/` (gerbers zip, BOM csv, CPL csv). PCB qty 5, PCBA qty 2, Economic, Top side, Confirm Parts Placement yes, Global Standard Direct Line shipping. Dry-ice cleaning yes, bake no, function test no. |
| **Tayda** (`02-Parts-and-Cart/tayda-cart-import.csv`, 22 lines) | box, screen, encoders, knobs, footswitches + pink caps, 9 V jack, 2 packs female/female jumpers, screen screws / spacers / nuts, black wire, cable ties, the 2 header strips, the page selector, the power switch, 2 pink chicken heads, the leveller's LEDs + light sensors + black heat shrink |
| **drill.taydakits.com** | the drill + UV print job (templates already uploaded) |
| **Mouser** (or as noted) | 2 x Alps **RK09L1240015** (dual 10k linear, centre click), 2 x Neutrik **NMJ6HFD2** jacks, 1 x **B0505S-1WR3** (also sold on Amazon, often 2-packs; any brand with the same 4 pins: GND, Vin, 0 V, +Vo) |
| **Amazon** | PCF8574 I/O board (page selector), B0505S-1WR3 (if not from Mouser) |
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
4. **The leveller's 2 light pairs (OC1, OC2)**: section 6c. These are the only parts that go in from the **back**.

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

- **fx loop** (2 x 4): **leave it empty, no jumpers.** The analog leveller on the board (6c) drives the returns now;
  a bridge from s to r would short its output to the codec's. (The sound works with nothing on it.)
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

## 6b2. The power switch (pink chicken head, left of the screen: 0 = off, 1 = on)

A mini rotary switch (A-8233, 2 poles x 4 positions) in the **+ wire** from the 9 V jack: position 1 = off, any other
position = on. With it off the pedal draws nothing; nothing else changes (the Seed3 runs from the 9 V through the
carrier board's vin wire, no USB needed; USB-C is only for flashing, and plugging it in while on is fine).

- **Find the lugs** (multimeter, beep mode): the switch has 2 **commons** (one per pole, nearer the middle) and
  8 outer lugs. Turn it fully **left** (position 1). The outer lug that beeps to a common is that pole's position 1:
  **leave both position-1 lugs empty**. Turn one click right: the next lug beeps (position 2), and so on.
- **Wiring** (black wire from the cart, ~6 cm each, solder):
  - both commons joined together -> the 9 V jack's **+ lug** (the sleeve lug: the jack is centre-negative);
  - all six other outer lugs (positions 2, 3, 4 of both poles) joined with bare wire offcuts -> the wire to the
    carrier board's **dc +** pin;
  - the jack's **centre (-) lug** -> **dc -**, as before.
- **Mounting**: snap off the little locating tab on the switch's front with pliers. Put it through the 9 mm hole, turn
  the body so position 1 lines up with the "0" dot, tighten the nut, push on the pink chicken head pointing at "0".
- Check before the box goes together: switch at 0 -> no beep between the jack's + lug and dc +; at 1 -> beep.

## 6c. The analog leveller (on the carrier board)

What it does: the DSP measures what's leaving and, above about -14 dBFS, lights two small LEDs a little (2:1); each
LED shines on a light sensor (LDR) that gently pulls its channel's level down **in the analog path**, before pg-hp:
the classic smooth "opto" leveller. LEDs dark = the sound passes untouched (that's also how it powers up).

JLCPCB fits everything except the two **light pairs**, OC1 (left channel) and OC2 (right), which you make:
each is one **3 mm flat-top red LED** (A-8041) + one **LDR** (A-5800) + **15 mm of the black 6 mm heat shrink**.

They sit on the **back** of the board (the lid side, plain except for their two printed outlines "LED / LDR"), next to
the "in" jack. Each outline has 4 holes: 2 at the LED end (the square one marked **+**), 2 at the LDR end.

1. **LED**: the **long leg is +** (also: the flat spot on its rim is -). Bend both legs 90 degrees, 1 mm from the
   body, so the LED lies flat along the outline with its flat top toward the LDR end. Long leg into the **+** (square)
   hole. Push it in from the back until it lies on the board.
2. **LDR**: either way round. Bend both legs 90 degrees, 1 mm from its body, so its face (the squiggly side) stands
   up and faces the LED. Legs into the 2 LDR holes from the back.
3. Slide the LED and LDR together until the LED's flat top **touches** the LDR's face. Slip the 15 mm heat shrink over
   both (it covers the whole pair), shrink it with a hair dryer (or the side of the iron's barrel, not the tip).
4. Turn the board over and solder the 4 pins on the printed side, then **snip the legs flush** (they sit under the
   9 V jack, which is 2.3 mm above the board there).
5. Same for the other pair.

Check before soldering the next one: multimeter on ohms across the LDR's two pins: sealed in the dark it reads
**hundreds of kOhm or more**. If it reads low, light gets in: add a second layer of heat shrink.

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
   - fx loop header: **3v3 to gnd = 3.3 V** (the isolated side). Nothing plugged onto the fx loop.
   - nothing warm. If anything is wrong, unplug and tell me what you measured.
2. Unplug, put the Seed3 in, flash the firmware (USB-C on the right side, `06-Firmware-DaisySeed/README.md`).
3. Power up: the screen starts, the **health** tab should say "all ok".
4. Before the expensive headphones: DC volts across the OUT jack (tip to sleeve) with a cable plugged in: about **0 V**
   (the simulation says ~1 mV). Then plug in at low volume, pg-hp at the centre click.

## What the simulation checked (so you know what to expect)
The whole routed board, simulated (10-Carla-Plugin/Source/sim): every part on its real copper.

- **Power**: from the 9 V jack alone (no USB-C needed: the Seed3 runs off the carrier's vin wire); 9 V 8.2, 5 V 5.00,
  3.3 V 3.30, isolated 5 V 5.05, isolated 3.3 V 3.30; ~200 mA. Isolation intact.
- **Power switch**: off = 0 mA. Off and straight back on with headphones in: the jack moves less than 1 mV (the amp
  stays muted ~0.4 s, ~1 s from cold: D63 + R70 1M, both added because the simulation found a ~1 V thump there).
- **What you can plug in** (in gain knob position for a healthy level; "hiss" = how far below the music the noise is):

  | source | in gain knob | result |
  |---|---|---|
  | dynamic mic straight in (2 mV) | full right | quiet (-37 dBFS), hiss 56 dB down: use a mic preamp, or let the input tab add gain |
  | condenser / quiet source (10 mV) | full right | -23 dBFS, hiss 70 dB down |
  | passive guitar | ~95% | clean, 7 dB headroom, hiss 83 dB down |
  | phone / laptop headphone out | ~90% | clean, 8 dB headroom, hiss 82 dB down |
  | consumer line (-10 dBV) | ~85% | clean, 8 dB headroom, hiss 82 dB down |
  | pro line (+4 dBu) | centre click | clean, 8 dB headroom, hiss 82 dB down |
  | pre-amped / hot (+10 dBu) | ~35% | clean, 3 dB headroom |
  | cranked headphone amp (3 V rms) | ~30% | clean, 1 dB headroom |
  | very hot (+20 dBu, 7.8 V rms) | any | clips (clean up to 3.4 V rms); nothing breaks |

  Abuse: +-24 V peaks at the input are clamped (+13 / -7.5 V), the circuit behind sees nothing outside its rails.
  **Never plug a power amp's speaker output in**: it would push amps into the clamp.
- **What you can plug it into** (out gain knob at its click = unity): 16 ohm in-ears ~1.75 V rms / 190 mW, 32 ohm
  headphones ~1.8 V rms / 104 mW, 80-600 ohm ~1.9 V rms, a mixer / interface line input ~1.9 V rms; DC at the jack
  ~1 mV.
- **The analog leveller**: dark = untouched (its 0.39 dB is made up in the firmware); -12 dB at 1.3 mA, -21 dB at
  7.8 mA (the firmware's limit); both channels the same; no click (under 5 mV). Dark for the first 10 s after power-up.
- **Wrong wiring**: swapped data / clock / I2C wires or a missing Seed3 ground just mean no sound (nothing breaks);
  jumpers left on the fx loop are flagged; a reversed 9 V is blocked by D1; a short trips the fuse. **The one wiring
  mistake that can damage parts is 9 V landing on a signal pin**: check the vin wire goes to pin 39.
