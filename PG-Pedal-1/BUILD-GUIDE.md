# PG-1 build guide (2026-10-09, carrier board v3)

This is the one to follow. It replaces the jack / 9 V / pot / "free pins" parts of `05-Wiring-and-Schematics/WIRING.md`,
which were written before the carrier board existed. Pin numbers here come straight from the firmware
(`06-Firmware-DaisySeed/pg1/src/main.cpp`) and the routed board, and the board itself was checked in simulation.

---

## 0. What to order (today)

| Where | What |
|---|---|
| **JLCPCB** | the carrier board: upload `12-Carrier-Board/jlcpcb/` (gerbers zip, BOM csv, CPL csv). PCB qty 5, PCBA qty 2, Economic, Top side, Confirm Parts Placement yes. JLCPCB also solders on the **B0505S** (no Mouser). The 2 jacks come from Amazon and **you** solder them (section 2). In their placement preview check pin 1 of U3 (B0505S), U15, U16, Q1 and the band of D63. |
| **Tayda** (`02-Parts-and-Cart/tayda-cart-import.csv`, 26 lines) | box, screen, encoders, knobs, footswitches + pink caps, 9 V jack, the **2 gain pots (A-8618)**, 2 packs female/female jumpers, screen screws / spacers / nuts, black wire, cable ties, the 2 header strips, the 2 rotary switches (power, page) + 2 pink chicken heads, the leveller's LEDs + light sensors + heat shrink, the 2 level lights + chrome bezels + 330 ohm resistors |
| **drill.taydakits.com** | the drill + UV print job (templates already uploaded) |
| **Amazon** | a PCF8574 I/O board (page selector + level lights): Comimark 3-pack B07X3KWQZ7 (or any with VCC / GND / SDA / SCL + P0-P7 pins); the 2 Neutrik NMJ6HFD2 jacks (B00FV23QH6) |
| **Seed3** | Daisy Seed3 **with headers** |
| **Tools** | soldering iron kit, **0.8 mm rosin-core solder** (right size for all of this), flush **wire cutters**, wire strippers, multimeter, hot glue gun, a little heat-shrink |

## 1. When the parts arrive: check

- **Carrier board**: every small part is already on it. Two pads in the input box are **empty on purpose** (C31, C41:
  leaving them off keeps the treble). The chips have their names printed next to them.
- Count: 2 pots, 2 header strips, 2 Neutrik jacks, the carrier boards with the B0505S already on, 4 encoders + knobs, 3 footswitches + caps, screen, 9 V jack.

## 2. Solder the carrier board (big through-hole pins only)

Work on the **printed side up** (the side with the labels): every part goes in from that side and is soldered on the back.
Iron on the pad + pin together 2 s, feed solder, a small shiny cone. Snip the leftover leg with the flush cutters.

1. **Headers** (lowest first, so the board lies flat):
   - snap the right-angle strip (A-199) into **10 + 6 + 6 + 4** pins, snap a **2 x 4** off the double strip (A-198).
   - 10 = **seed3 + 9v**, 6 = **pg-line**, 6 = **pg-hp**, 4 = **expansion**, 2 x 4 straight = **fx loop**.
   - the right-angle pins point **off the board edge** (the printed pin names sit beside them). The holes grip the pins,
     so they stay straight while you solder: one end pin first, check it sits flat, then the rest.
2. The **B0505S** comes already soldered by JLCPCB. Check its pin-1 mark (a dot or "1" on its case) sits at the
   square pad marked "PIN 1"; if not, stop and tell me (backwards it feeds power into its output).
3. The **2 Neutrik jacks** (Amazon): from the printed side into **J1 (in)** and **J2 (out)**, noses pointing off the top
   edge. They only fit one way. Press them flat, solder every pin, and fill the big mounting tabs well: the jacks are
   what hold the board up in the box.

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

Each pot (Tayda **A-8618**, Alpha dual 10k linear: no centre click, the middle of its turn is unity) is a **dual**
pot: 2 rows of 3 pins (front gang + back gang). The header pins are printed with the **pot pin
that goes there**: `3 l, 2 l, 1 l, 3 r, 2 r, 1 r` ("l" = the front gang, "r" = the back gang).

Find the pot's pins 1 and 3 with the multimeter (ohms), knob turned fully LEFT:
- the middle pin of each row is **2** (the wiper).
- of the two outer pins, the one with **~0 ohm to the middle pin** (when turned fully left) is **pin 1**; the other is **pin 3**.
  (Turn it right: pin 1 to middle goes up to ~10k. That's the check.)

- Mount each gain pot with its **pins pointing down** (toward the screen): that way they clear the box's corner screw posts (0.8 mm).
- **Input Gain** pot (under "in") -> the **pg-line** header (also under "in").
- **Output Gain** pot (under "out") -> the **pg-hp** header (also under "out").

## 6. The fx loop and the expansion port

- **fx loop** (2 x 4): **leave it empty, no jumpers.** The analog leveller on the board (6c) drives the returns now;
  a bridge from s to r would short its output to the codec's. (The sound works with nothing on it.)
- **expansion** (4 pins: 3v3, gnd, scl, sda): 4 jumpers to the **expansion bay** slot in the LEFT wall. Push their
  female ends into the slot from inside until flush, hot glue them on the inside (warm the wall with a hair dryer
  first). Note which colour is which pin. Modules plug onto them from outside later.

## 6b. The page selector (pink chicken head, top right of the two, 4 clicks)

Whatever tab is open, the selector picks its **settings page**: position 1 = page 1, 2 = page 2... A tab with fewer
pages just shows its last one. Without the selector fitted, swiping / pg-b still change pages.

- **Switch (A-8233)**: 2 poles x 4 positions (45 degree clicks). Only pole A is used. Find pole A's **common** with the
  multimeter (beep mode): turn it fully **left** (position 1), the common beeps to exactly one outer lug: that's lug 1;
  one click right, lug 2 beeps; then 3, 4. (Pole B: leave it empty.)
- **PCF8574 board** (Amazon, pins on): solder 5 wires to the switch: lug 1 -> **P0**, lug 2 -> **P1**, lug 3 -> **P2**,
  lug 4 -> **P3** (female jumper ends on the PCF8574 side), common -> **GND**: the GND pin already has the carrier's
  jumper on it, so solder the common's wire on the board's **underside**, on the GND pin's joint. Bend them flat.
- PCF8574 input header -> the carrier's **expansion** header: VCC -> 3v3, GND -> gnd, SDA -> sda, SCL -> scl.
  (If it has a second, pass-through header, run the 4 wires to the left-wall expansion bay from there.)
- Velcro the PCF8574 to the inside of the lid. The firmware finds it by itself (any address).
- **Mounting** (both switches the same): snap off the little locating tab on the switch's front with pliers, through
  the 9 mm hole, turn the body so position 1 lines up with the "1" dot, tighten the nut (2 mm nut + washer: up to 4 mm
  of panel). Turn it fully left, put the **pink chicken head** (A-6623) on pointing at "1" and tighten its set screw
  (1.5 mm hex key) onto the shaft.

## 6b2. The power switch (pink chicken head, top left of the two: 0 = off, 1 = on)

The same switch (A-8233) in the **+ wire** from the 9 V jack: position 1 = off ("0"), any other position = on ("1": the
other two clicks are on too). Off, the pedal draws nothing. The Seed3 runs from the 9 V through the carrier board's
vin wire, no USB needed; USB-C is only for flashing, and plugging it in while on is fine.

- **The 9 V jack is in the LEFT side wall, low** (between the pg-1 knob and the pg-a footswitch, inside: 3 mm clear of
  both). Plug faces left.
- **Find the lugs** (beep mode): 2 **commons** (one per pole) and 8 outer lugs. Turn it fully **left** (position 1):
  the outer lug that beeps to each common is that pole's position 1: **leave both position-1 lugs empty**.
- **Wiring** (black wire from the cart, solder; ~15 cm from the jack up to the switch):
  - the 9 V jack's **+ lug** (the sleeve lug: the jack is centre-negative) -> both commons, joined;
  - all six other outer lugs (positions 2, 3, 4 of both poles) joined with bare offcuts -> a wire to the carrier
    board's **dc +** pin;
  - the jack's **centre (-) lug** -> **dc -**, straight.
- Mount it like the page selector, with position 1 at the "0" dot and the chicken head pointing at "0".
- Check before closing the box: at 0 -> no beep between the jack's + lug and dc +; at 1 -> beep.

## 6c. The analog leveller (its 2 light pairs)

What it does: the DSP measures what's leaving and, above about -14 dBFS, lights two small LEDs a little (2:1); each
LED shines on a light sensor (LDR) that gently pulls its channel's level down **in the analog path**, before pg-hp:
the classic smooth "opto" leveller. LEDs dark = the sound passes untouched (that's also how it powers up).

JLCPCB fits everything except the two **light pairs**, OC1 (left channel) and OC2 (right), which you make:
each is one **3 mm flat-top red LED** (A-8041) + one **LDR** (A-7629) + **15 mm of the black 6 mm heat shrink**.

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
   9 V jack's old spot; it's in the left wall now, so nothing is above them, but flush is tidy).
5. Same for the other pair.

Check before soldering the next one: multimeter on ohms across the LDR's two pins: sealed in the dark it reads
**hundreds of kOhm or more**. If it reads low, light gets in: add a second layer of heat shrink.

## 6d. The 2 level lights (under the gain knobs)

Each is a **2-leg red/green LED** (A-1076) in a **chrome bezel** (A-661): green = signal, red = close to clipping, off =
silence. The in light is under Input Gain, the out light under Output Gain. They run from the PCF8574 board's spare
pins (P4-P7), so they need no Seed3 pins.

1. Push the plastic insert (A-7116) into the bezel, the LED into the insert from behind. Bezel through its 5.7 mm hole,
   nut on from inside. **No glue**: the nut holds it.
2. The **right (out) light has the Seed3 socket block 1.7 mm behind it**: bend its legs flat sideways straight away,
   toward the top of the box, before you wire them.
3. Solder a wire to each leg (heat shrink each joint), female jumper end on the other end:
   in light -> **P4** and **P5**; out light -> **P6** and **P7** (either leg on either pin).
4. **330 ohm pull-ups**: on the PCF8574 board's underside solder one 330 ohm from each of **P4, P5, P6, P7** to a
   **VCC** pin's joint (join the 4 VCC ends together first).
5. Power up: each light should be green with signal. Red with signal = swap that light's 2 wires (or set
   `kSwapLightColours = true` in `main.cpp` for both).

On the screen: open the **input** tab, page 2 (page selector to 2): **leveller** on / off, **lights** on / off, and the
leveller's live view: what it cuts now, the last 5 seconds of it against the output level, and the 2 lights.

The full drawing of every wire: `05-Wiring-and-Schematics/wiring-diagram.pdf` (zoom in) with `WIRE-LIST.md`
(the numbers on the drawing).

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
footswitch's other lug -> screen pin 2 (GND) -> **spliced onto the gnd jumper** (the one from the carrier's gnd pin to
socket place 40), about 3 cm from the socket: strip 5 mm off that jumper's middle, wrap the chain's end round it,
solder, cover with heat shrink. (A socket place only holds one jumper end, so the chain can't have its own.) (The old "audio chain" to pin 20 is gone: the
jacks' sleeves are on the carrier board's isolated ground and must **not** be connected to anything else.)

## 8. First power-up

1. **Seed3 out**, carrier board wired. Plug in the 9 V adapter. Multimeter DC volts:
   - expansion header: **3v3 to gnd = 3.3 V**
   - fx loop header: **3v3 to gnd = 3.3 V** (the isolated side). Nothing plugged onto the fx loop.
   - nothing warm. If anything is wrong, unplug and tell me what you measured.
2. Unplug, put the Seed3 in, flash the firmware (USB-C on the right side, `06-Firmware-DaisySeed/README.md`).
3. Power up: the screen starts, the **health** tab should say "all ok".
4. Before the expensive headphones: DC volts across the OUT jack (tip to sleeve) with a cable plugged in: about **0 V**
   (the simulation says ~1 mV). Then plug in at low volume, pg-hp at the middle of its turn (the dot).

## What the simulation checked (so you know what to expect)
The whole routed board in the board simulator (10-Carla-Plugin/Source/sim, every part on its real copper), and the
isolated audio path again in ngspice (the standard circuit simulator) for the worst cases.

- **Power**: from the 9 V jack alone (no USB-C needed: the Seed3 runs off the carrier's vin wire); ~200 mA; isolation
  intact. **Power switch** off = 0 mA; off and straight back on with headphones in: the jack moves under 3 mV.
- **Hard limits (physics, not software):**
  - **Input clamp**: whatever comes in and wherever the in gain knob is, the codec's input pin stays inside its safe
    window (ngspice: -0.02 .. +1.85 V for +-12 / +-24 V hits of 5-150 ms; its limit is -0.3 .. +2.1 V). The analog
    stage clips before the converter does, so the Seed3 never receives anything past full scale.
  - **Output ceiling**: the headphone amp runs on +-3.3 V: the jack can never pass 3.0 V peak (2.1 V rms) whatever the
    firmware does: at most 86 mW into your 50 ohm headset, 132 mW into 32 ohm. The firmware also re-checks the codec's
    own gains every second and puts them back.
  - **The leveller only ever cuts** (it can't add gain); its LEDs top out at ~11 mA (rated 20 mA).
- **Guitar**: the input is 1.46 Mohm, flat to 10 kHz: pickups keep their sparkle (595k before: ~2.4 dB duller).
- **What you can plug in** (in gain knob position for a healthy level; "hiss" = how far below the music the noise is):

  | source | in gain knob | result |
  |---|---|---|
  | dynamic mic straight in (2 mV) | full right | quiet (-44 dBFS): use a mic preamp, or let the input tab add gain |
  | condenser / quiet source (10 mV) | full right | -30 dBFS, hiss 63 dB down |
  | passive guitar | ~95% | clean, hiss 81 dB down |
  | phone / laptop headphone out | ~95% | clean, hiss 84 dB down |
  | consumer line (-10 dBV) | ~95% | clean, hiss 82 dB down |
  | pro line (+4 dBu) | ~75% | clean, hiss 81 dB down |
  | pre-amped / hot (+10 dBu) | ~60% | clean, hiss 82 dB down |
  | cranked headphone amp (3 V rms) | ~50% | clean |
  | very hot (+20 dBu, 7.8 V rms) | any | clips (clean up to 4.8 V rms); nothing breaks |

  Never plug a power amp's **speaker** output in: it would push amps into the input clamp.
- **The analog leveller**: dark = untouched (its 0.39 dB is made up in the firmware); -12 dB at 1.3 mA, -21 dB at
  7.8 mA (the firmware's limit); both channels the same; no click (under 5 mV). Dark for the first 10 s after power-up.
- **Wrong wiring**: swapped data / clock / I2C wires or a missing Seed3 ground just mean no sound (nothing breaks);
  jumpers left on the fx loop are flagged; a reversed 9 V is blocked by D1; a short trips the fuse. **The one wiring
  mistake that can damage parts is 9 V landing on a signal pin**: check the vin wire goes to pin 39.
