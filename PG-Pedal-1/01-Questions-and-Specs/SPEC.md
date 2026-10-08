# PG-1 spec (decided 2026-10-06)

| Topic | Decision |
|---|---|
| Brain | Daisy Seed3 (STM32H750, 65MB, TAC5242 codec, USB-C), libDaisy v9+ |
| Enclosure | Tayda 1590XX **white** (A-5883), 145 × 121 × 39 mm, drilled + UV printed by Tayda |
| Controls | 4 endless push-encoders **pg-1 ... pg-4** (20 detents) |
| Footswitches | 3 soft-touch **momentary** A-1091 (pressed = on, released = off) **pg-a, pg-b, pg-c**, all programmable |
| Screen | 2.4" color **touch** TFT 240×320 ILI9341 + XPT2046 (A-8180), bolted with 4 M3 screws |
| Audio in | 1/4" **TRS stereo** straight into the Seed3 (line level, max ~1 Vrms). **Auto-leveling in firmware** (−12 to +40 dB, gated) |
| Audio out | 1/4" **TRS stereo** straight from the Seed3 (0 dBFS = 1 Vrms) |
| Power | 9V DC center-negative straight to the Seed3 VIN (reverse-protected on the Seed3) + USB-C. Screen powered from the Seed3's 3.3 V |
| USB-C | The Seed3's own USB-C, on the cartridge sticking out of the LEFT side (hold pg-a + pg-b for 2 s = flash mode; BOOT/RESET reachable too) |
| Layout | Screen at the top, then pg-1 · pg-2 · pg-3 · pg-4 in a row, then pg-a / pg-b. Jacks on the top edge, left to right: in · 9v · exp (spare) · out; USB-C on the left side |
| Look | All text lowercase in a rounded font (Nunito), everything black on white, smooth rounded border ring, one-line logo + "pg audio" |
| Mounting | Panel parts clamp with their own nuts; the screen uses 4 M3 screws. **Seed3 cartridge:** the Seed3 stands on its edge in a window in the left wall and plugs into 2 female headers on a small socket board screwed inside (swap = pull out, push in, no soldering). Every part is wired to that socket board. Nothing on the bottom edge |
| Build | **Seed3 only, no resistors, capacitors or chips**; every part wired to the Seed3 socket board |

## Answers from the question rounds
1. Size 1590XX · power: you weren't sure, so 9V jack + USB with protection · input: "anything" · USB: panel/cutout
2. Gain knob on top · screen 1.8" (later changed to 2.4" because the 1.8" can't be screwed down) · perfboard · black skirted knobs
3. Layout "screen top, knobs row" · footswitch labels pg-a/pg-b · a one-line logo + tagline "pg audio" · rounded font
4. 2.4" screen with holes · USB-C via a panel extension (no bottom-plate mods) · Seed3 + 9V adapter bought separately

5. Only the Seed3 board (no extra parts) · gain knob → pg-4 · auto-leveling · normal gear on the output · add a soldering iron · stands on end, so nothing on the bottom

8. No board/sheet: jumper wires straight onto the Seed3, Seed3 on its back, USB-C panel cable back, pinholes removed
7. Touch wired · BOOT/RESET pinholes in the top · dock super-glued to a removable plastic sheet
9. Seed3 as a cartridge in the left wall: socket board + 2 female headers, keyed by snipping pin 1; the USB-C panel cable and foam tape are gone
6. Screen higher, knobs lower · Seed3 socketed, sideways, USB-C out of the left side · bottom-plate tape mount · spare exp jack + expansion header · no USB MIDI · no headphone amp (HD 599 SE is fine)

## Limits to know
- Condenser mics need phantom power, which this pedal doesn't have. Dynamic mics are fine.
- A passive guitar plugged straight in sounds a bit duller (there's no 1 MΩ buffer). A buffered pedal in front fixes it.
- Dynamic mics work, but auto-leveling lifts some hiss with them.
