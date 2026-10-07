# PG-1 wiring (Seed3 only, no extra parts)

Every part connects straight to the Daisy Seed3 with one wire per pin. There are no resistors, capacitors or chips.
The Seed3 has internal pull-ups for the encoders and switches, its own power regulators, and a reverse-protected VIN (per its datasheet).

**Wiring diagram:** `wiring-diagram.png` (or `.pdf` to print): every wire, in colour.

## How the Seed3 sits: no board at all

- **Jumper wires plug straight onto the Seed3's pins.** Use female-to-female jumpers (Tayda A-3482, a 40-pack of 200 mm). Push one end onto the Seed3 pin, cut the other end off and solder it to the part.
  The **screen** has its own pins, so both ends just plug on, with no soldering. Swapping a Seed3 means pulling the jumpers off and pushing them onto the new one, so **photograph or label them first**.
- **The Seed3 lies on its back** (pins pointing up) on ~5 mm of 3M VHB foam tape (2-3 layers) on the inside of the bottom plate, in the empty strip left of the screen.
  The foam insulates it from the metal. Put the tape under the middle and the far end, not under the USB-C end. Its USB-C end points toward the footswitches.
- **USB-C**: a short SparkFun CAB-15455 panel cable plugs into the Seed3 and screws into the rectangular slot in the **left side** (2 M3 screws, included with the cable).
- **BOOT / RESET** face down, so to press them take the bottom plate off (4 screws) and tilt the Seed3 up. You'll rarely need them: holding fs-1 + fs-2 for 2 s does the flash mode.
- **Grounds** (the bottom of `wiring-diagram.png` shows both chains step by step): daisy-chain them, as **two separate chains** of black wire, part to part:
  - **audio chain** (quiet): IN jack sleeve → OUT jack sleeve → one jumper to **pin 20 (AGND)** (the exp jack sleeve joins it once the exp jack is used).
  - **main chain** (everything else): 9V jack − lug → each encoder's C + 2nd push pin → each footswitch's 2nd lug → screen GND → one jumper to **pin 40 (DGND)**.
  - The Seed3 joins the two inside itself. Keeping them apart outside means the screen's and switches' ground current never flows
    through the audio sleeves (less hiss and whine).
  - Don't close a chain into a loop, and don't connect the two chains to each other anywhere else.
  - Each joint has to be solid: if one link breaks, everything after it loses ground. The **health** tab notices most of those
    (a knob that jitters or reads stuck, hum, a silent input side) and says which part.
- Optional: a small dab of hot glue on the row of jumper plugs keeps them from wiggling loose. It peels off when you want to swap.

Screen: `M3×12 black screw → face → 5 mm spacer → screen → M3 nut` (×4). The screen's 14-pin header end goes to the right.

## Wire list

Jumper wires come in mixed colours, so stick a small label or a dot of tape-flag on each one before plugging it on. The black AWG22 wire is only for the ground chain.

| From | Jumper pushed onto Seed3 pin |
|---|---|
| **IN jack** tip (left) | 16 (AUDIO IN L) |
| IN jack ring (right) | 17 (AUDIO IN R) |
| IN jack sleeve | audio chain → 20 |
| **OUT jack** tip (left) | 18 (AUDIO OUT L) |
| OUT jack ring (right) | 19 (AUDIO OUT R) |
| OUT jack sleeve | audio chain → 20 |
| **9V jack** + lug (sleeve) | 39 (VIN) |
| 9V jack − lug (center pin) | main chain → 40 |
| **pg-1** encoder A / B / push | 2 (D1) / 3 (D2) / 4 (D3) |
| **pg-2** encoder A / B / push | 5 (D4) / 6 (D5) / 7 (D6) |
| **pg-3** encoder A / B / push | 14 (D13) / 15 (D14) / 22 (D15) |
| **pg-4** encoder A / B / push | 26 (D19) / 27 (D20) / 28 (D21) |
| each encoder: middle pin C + 2nd push pin | main chain → 40 |
| **fs-1** lug / other lug | 24 (D17) / main chain |
| **fs-2** lug / other lug | 25 (D18) / main chain |
| **fs-3** lug / other lug | 10 (D9) / main chain |
| **screen** 1 VCC | 38 (3V3 digital) |
| screen 2 GND | main chain → 40 |
| screen 3 CS | 8 (D7) |
| screen 4 RESET | 13 (D12) |
| screen 5 DC | 12 (D11) |
| screen 6 SDI (MOSI) | 11 (D10) |
| screen 7 SCK | 9 (D8) |
| screen 8 LED | 23 (D16) |
| screen 9 SDO | leave empty |
| screen 10 T_CLK / 11 T_CS / 12 T_DIN / 13 T_DO / 14 T_IRQ (touch) | 29 (D22) / 30 (D23) / 31 (D24) / 32 (D25) / 33 (D26) |

Seed3 pin numbers: right-side up with USB-C at the bottom, **pins 1-20 run up the right side** (pin 1 next to USB-C) and **pins 21-40 run down the left side** (pin 40 next to USB-C).
**Lying on its back in the pedal it's mirrored:** looking down into the box with USB-C toward the footswitches, **pins 1-20 are along the left wall**, and **21-40 face the screen**. Pin 1 and pin 40 are the ones next to USB-C.
See `07-Datasheets/Seed3-Pinout.pdf`.

## Expansion port (for later, no new holes needed)
Free Seed3 pins for later: pin 34 (D27), 36 (D29), 37 (D30). (Pin 10 / D9 is fs-3, pin 35 / D28 is the exp jack.) The 6 spare jumpers from the pack plug straight on, so nothing needs soldering on the Seed3. Anything you add later (LEDs, a sensor, MIDI, a 2nd screen) plugs in there.

**exp jack** (top edge, installed now but not wired): to use it later for an expression pedal, wire **sleeve → ground**, **ring → 3.3 V through a 1 kΩ resistor**, **tip → D28 (pin 35, an analog pin)**,
then it's a jumper onto pin 35 plus a firmware change. Keep the 1 kΩ: a mono cable shorts ring to sleeve.

- **Encoders**: 3 pins on one side are A, C, B (C in the middle). The 2 pins on the other side are the push switch.
- **Footswitches (A-1091, soft-touch, normally open)**: 2 solder eyelets on the side. One goes to the Seed3 pin, the other to the ground chain. It's pressed = on, released = off; latching is done in firmware.
- **Knobs (A-2850)**: slide on and tighten the set screw against the flat side of the shaft. No need to line anything up, because the encoders turn endlessly.
  If a knob counts backwards, set `kReverseEncoder = true` in the firmware.
- **Jacks and the 9V jack**: find the tip/ring/sleeve and +/− lugs with a multimeter before soldering (beep test with a cable or adapter plugged in).
- **Snap off the little anti-rotation tab** on each encoder; the holes are round. Every part clamps with its own nut.
- Screen wires: about 20 cm, so the plate can come off. If the picture ever glitches, tell me and I'll slow the screen link down in firmware.

## What "no extra parts" means for sound
- **Line level, PC audio, synths, mixers**: ideal.
- **Auto-leveling** happens in the firmware. Quiet sources are brought up and loud ones down to a steady level, and a gate keeps hiss from being pumped up during silence.
- **Guitar or bass plugged in directly** will sound a bit duller and quieter. The Seed3 input isn't the 1 MΩ high-impedance input a passive pickup wants (Electro-Smith adds a buffer chip for that).
  If you play guitar into it a lot, put any buffered pedal (most tuners) in front, or ask me and I'll add a 1-chip buffer later.
- **Dynamic mics** work through a 1/4" adapter, but they are very quiet, so auto-leveling brings up some hiss with them. Condenser mics (phantom power) won't work.
- **Before connecting the output to expensive gear the first time**, set a multimeter to DC volts and measure OUT tip-to-sleeve with the pedal on.
  It should read about 0 V. If it shows more than about 0.1 V, tell me and I'll add a blocking capacitor.

## Flashing
Plug USB-C into the left side. **First time:** take the bottom plate off, hold **BOOT**, tap **RESET** (or flash the Seed3 on your desk before installing it), then flash `06-Firmware-DaisySeed/pg1.bin` at https://flash.daisy.audio
(or run `make program-dfu`). After that, **holding fs-1 + fs-2 for 2 seconds** puts it into flash mode over the same USB-C, with the box closed.
