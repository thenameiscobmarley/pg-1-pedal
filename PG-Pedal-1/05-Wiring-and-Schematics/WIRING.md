# PG-1 wiring (Seed3 cartridge, no extra parts)

Every part connects to the Daisy Seed3's socket with one wire per pin. There are no resistors, capacitors or chips.
The Seed3 has internal pull-ups for the encoders and switches, its own power regulators, and a reverse-protected VIN (per its datasheet).

**Wiring diagram:** `wiring-diagram.png` (or `.pdf` to print): every wire, in colour.

## How the Seed3 sits: a plug-in cartridge in the left wall

The Seed3 stands on its long edge in a 19 x 52 mm window in the **left side**, parts side facing out, and plugs into a
**socket board** screwed inside the wall. From outside you can reach USB-C, BOOT and RESET, and swapping a Seed3 is
pull out, push in: **no soldering on the Seed3, ever**. Drawings: `seed-socket-board.png` (the board, from the soldering side) and `interior-layout.svg`.

- **Socket board:** cut the A-5465 prototyping board to 68 x 28 mm. Solder the two A-1310 20-pin female headers on it, 15.24 mm apart
  (6 holes between the rows), centred. The strips are soldered to the board, so they stay in it when you pull the Seed3 out.
  4 mount holes, 3.2 mm: 31 mm left and right of the middle, 10 mm above and below the centre line (62 x 20 mm apart).
  Tip: plug a Seed3 into both headers *before* soldering them, so the spacing is exact.
- **First time with a prototyping board?** It's a sheet of holes, each with its own copper ring on ONE side (that's what "single side" means). The holes aren't connected to each other.
  1. Score it with a utility knife along a line of holes, on both faces, and snap it over a table edge. Do that 4 times to get 68 x 28 mm. Sand the edges.
  2. Plug the 2 header strips into the Seed3, then push the header legs through the board from the **plain side** (no copper), centred. The copper rings face you.
  3. Solder each leg to its copper ring (iron on the ring and leg for 2 s, feed solder, a small shiny cone). Do the 4 corner legs first, check it's straight, then the rest. Pull the Seed3 out.
  4. Mount holes: lay the board on the drill template's side C (or mark 62 x 20 mm), then twist a 3 mm drill bit by hand (or a craft knife) in the 4 holes. The board is soft, so each takes about a minute.
  5. Each wire: strip 3 mm, tin it, hold it against its header leg on the copper side and touch the iron: done. One wire per leg, legs never touch each other.
- **Mounting** (x4): `M3x12 screw from outside → side wall → 5 mm nylon spacer → socket board → M3 nut`.
  The headers then poke ~1 mm out through the window, which guides the Seed3 in.
- **Wires:** each wire is soldered to its header tail on the back of the socket board (same pin number as the Seed3 pin).
  Cut the A-3482 jumpers and use them as wire; the screen end still just plugs on. **The back is a mirror image**: use `seed-socket-board.png`, not the Seed3's own pinout.
- **Which way round:** USB-C points toward the footswitches, the parts side faces out, and pins 1-20 are the row nearer the face.
- **Key it so it can't go in backwards:** snip pin 1 (D0, not used) off the Seed3 and glue a snipped resistor leg into socket 1.
  Every wrong way round is then blocked. (Backwards would put 9 V on the wrong pins.) Do the same to a spare Seed3.
- **Swapping:** unplug the 9 V and USB first, pull it straight out (don't rock it hard), push the new one fully home,
  then flash the firmware onto it over USB (`06-Firmware-DaisySeed/README.md`). The saved settings are on the Seed3, so a new one starts at defaults.
- It sticks out ~8.5 mm. Don't step on that side, and give it a little room on your desk or pedalboard.
- **BOOT / RESET** are on the outside now, but you'll rarely need them: holding fs-1 + fs-2 for 2 s does the flash mode.
- **Grounds** (the bottom of `wiring-diagram.png` shows both chains step by step): daisy-chain them, as **two separate chains** of black wire, part to part:
  - **audio chain** (quiet): IN jack sleeve → OUT jack sleeve → one wire to socket **pin 20 (AGND)** (the exp jack sleeve joins it once the exp jack is used).
  - **main chain** (everything else): 9V jack − lug → each encoder's C + 2nd push pin → each footswitch's 2nd lug → screen GND → one wire to socket **pin 40 (DGND)**.
  - The Seed3 joins the two inside itself. Keeping them apart outside means the screen's and switches' ground current never flows
    through the audio sleeves (less hiss and whine).
  - Don't close a chain into a loop, and don't connect the two chains to each other anywhere else.
  - Each joint has to be solid: if one link breaks, everything after it loses ground. The **health** tab notices most of those
    (a knob that jitters or reads stuck, hum, a silent input side) and says which part.

Screen: `M3×12 black screw → face → 5 mm spacer → screen → M3 nut` (×4). The screen's 14-pin header end goes to the right.

## Wire list

Jumper wires come in mixed colours, so stick a small label or a dot of tape-flag on each one. The black AWG22 wire is only for the ground chain.

| From | Soldered to socket pin |
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
**On the socket board's back it's mirrored:** seen from inside the box (face up, USB-C end on your left), **pins 1-20 are the top row from left to right** and **pins 40-21 the bottom row from left to right**. Pin 1 and pin 40 are the ones at the USB-C end.
See `07-Datasheets/Seed3-Pinout.pdf`.

## Expansion port (for later, no new holes needed)
Free Seed3 pins for later: pin 34 (D27), 36 (D29), 37 (D30). (Pin 10 / D9 is fs-3, pin 35 / D28 is the exp jack.) Their header tails on the socket board are free, so nothing ever needs soldering on the Seed3. Anything you add later (LEDs, a sensor, MIDI, a 2nd screen) plugs in there.

**exp jack** (top edge, installed now but not wired): to use it later for an expression pedal, wire **sleeve → ground**, **ring → 3.3 V through a 1 kΩ resistor**, **tip → D28 (pin 35, an analog pin)**,
then it's a wire to socket pin 35 plus a firmware change. Keep the 1 kΩ: a mono cable shorts ring to sleeve.

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
