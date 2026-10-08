# PG-1 firmware

`pg1/` is a libDaisy project. The pin map lives in one block at the top of `src/main.cpp`; everything
else (screen, controls, DSP) is in `pg1/core`, the same code the Carla plugin runs.

## The tabs (in signal order)

Home page 1 has 8 tabs; page 2 (turn past the last tab, or tap the dots bottom-right) has **input**, **health**, **takeback**, **width**, **loudness**, **config**, **multiband** and **pid**.

`in -> hum -> input -> dyn eq -> comp -> multiband -> clarity -> saturate -> de-harsh -> width -> takeback -> loudness -> [bypass] -> safety -> out`

| Tab | What it does | Knobs pg-1 / pg-2 / pg-3 / pg-4 |
|---|---|---|
| **input** (page 2) | Auto-level: brings any source to the same working level. Quiet ones go up (at most *boost*), hot ones down (at most *cut*). It moves slowly and holds still in silence, so it never pumps up noise. A yellow **in clip** chip shows in the title on every page when the source is hotter than the Seed3's input can take. Only turning the source down fixes that. | target / boost / cut / speed |
| **hum** | *auto / 50 / 60*: finds mains hum and notches it plus 5 harmonics, only as deep as the hum really is, and turns the top end down in quiet parts. ***learned***: set pg-1 to **learn** and push it (or tap **learn** on the screen) while **only the hum / hiss is playing**. In 3 s it learns every tone to the exact frequency (mains hum, monitor or charger whine) and the hiss level in 8 bands. Then it notches those tones all the way down and gates each band to silence whenever nothing louder than that noise plays. It tells you if music or talking spoiled the learning. Runs first, before the auto-level. | mode / hum depth / hiss cut / hiss threshold (learned: margin) |
| **dyn eq** | 4 bands, each moves when that band gets loud. | freq (hold + turn = q) / gain / thresh / range |
| **comp** | Smooth stereo compressor, soft knee, bass-safe sidechain, auto make-up, auto release. | thresh / ratio / attack / release (0 = auto) |
| **clarity** | Dynamic clarity + bass: punch on each kick, bass harmonics so bass is heard on any speaker, cuts mud when it builds up, lifts presence (a quarter as much when the music is already bright), warmth lifts the lows and tames brightness only when it's bright. **Mode** (hold pg-1 + turn, or tap *mode* on the screen): *dynamic* (lifts and cuts), *add* (only ever lifts, never cuts), *normalise* (pulls 8 bands toward a smooth, slightly warm balance; warmth = a warmer slope). | thump (+dB per hit) / detail (amount) / clarity (+dB presence) / warmth (+dB lows) |
| **saturate** | Tape / tube curve, anti-aliased, level-matched. Off by default. | drive / even / tone / mix |
| **de-harsh** | After everything: finds harsh spots between 2 and 8.5 kHz and cuts just those, plus softens 3 kHz as it gets loud. | depth / sens / speed / comfort |
| **safety** | Always on, even in bypass: **only 20 Hz - 19.5 kHz can leave** (very steep filters at both ends; what's outside human hearing is removed, and if it ever reached the output anyway the pedal would mute and say so), woofer and tweeter power limits (coils stay cool), a long-term loudness cap for your ears, a blast guard for sudden jumps, a 1 ms look-ahead limiter and a hard ceiling. Broken numbers or a stuck DC level mute the output for 150 ms and reset every filter. | ceiling / ears / woofer / tweeter |
| **multiband** (page 2) | 4 bands (split at 120 Hz, 1 kHz, 6 kHz; the untouched bands add back to the original exactly). Each band holds back its own peaks, relative to its own recent level, so a boomy note, a muddy burst, a harsh hit or a sharp "s" gets tamed without touching the rest. | bass / low mids / high mids / highs (how firmly) |
| **takeback** (page 2) | Gives back what the processing took or buried, by comparing with the sound as it came in: the **punch** of hits the comp / limiter squashed, quiet **detail** (tails, room, breaths), top-end **air** that went missing, stereo **space** that shrank. Capped, so it restores rather than exaggerates. | punch / detail / air / space |
| **width** (page 2, off by default) | Wider or narrower stereo, bass kept in the middle (mono-safe), a little side air, and a guard that eases off if mono speakers would lose sound. | width / bass mono / air / guard |
| **loudness** (page 2, off by default) | At low volume ears lose bass and treble: this gives back about what they lose, more the quieter you listen (follow = quiet passages too). | listen / bass / treble / follow |
| **config** (page 2) | 8 saved configs (every sound setting + which stages are on). pg-1 picks, **push pg-1 = load**, **push pg-2 twice = save**. Theme: **pearl** (light, default) or classic **bios**. Knobs: normal / reverse. | config / save / theme / knobs |
| **visual** | Spectrum (in and out), waterfall, stereo (vectorscope + correlation + balance), levels (in / out + what each stage is taking off), **scope** (oscilloscope: the last 13 ms, triggered so a steady note stands still, auto-zoom), **bars** (31 bands with falling peak caps), **history** (in and out loudness over the last 15 s). Tap the screen for the next view. | view / fall / range / source |
| **pid** (page 2) | Listens to the processed sound and steers the settings of the groups you pick, on top of your own settings: **eq** (each band towards the target balance, ±6 dB), **comp** (threshold + ratio, towards a 12 dB peak-to-average, ±8 dB), **mband** (how firmly each band is held), **clarity** (thump / warmth + clarity, ±4 dB), **deharsh** (depth, ±6 dB), **width** (towards a correlation of 0.35, ±40 %). P reacts to how far off it is, I slowly clears what's left, D brakes fast swings; any of them can be 0.00 (all three 0.00 = no steering). In silence it holds still. Off by default (hold fs-2). The screen shows what it hears vs the target, and how far it is moving each group. | p / i / d / group (push = steer it or not; hold + turn = target tilt, dB per octave) |

The safety levels are dBFS at the pedal's output. They don't know how loud your amp is, so set them
once at a loud-but-comfortable volume: lower **ears** until it just starts easing down.

## Controls

| Control | Home | A page |
|---|---|---|
| turn a knob | move between tabs | the value in that knob's box (the boxes have the knob number in a cream chip) |
| hold a knob + turn | | fine steps (dyn eq pg-1: q) |
| push a knob | open the tab | flashes its box |
| **double-push** | | that value back to default |
| **long-push** (0.6 s) | | back to home |
| fs-1 | whole-pedal bypass (safety stays on) | same |
| fs-2 | next tab | dyn eq: next band. Other pages: next page |
| hold fs-2 (0.6 s) | that tab's stage on / off | this page's stage on / off |
| touch | tap a tab to open it; tap "on" for bypass | tap "< home"; tap the on/off chip; tap a box = what it does; dyn eq: drag nodes, tap band chips, double-tap a node = gain 0; visual: tap = next view |
| fs-3 | fair A/B: hear your untouched input at the same loudness (a cyan "a/b dry" chip shows); tap again to come back | same |
| touch d-pad (off by default) | turn it on in **config** (tap "touch d-pad", or hold pg-4 + turn), then **tap the title** to show it: arrows move between tabs, the middle opens | left / right pick a value box (yellow frame), up / down change it (hold to repeat), the middle = that knob's push (hold = home). Hides itself after 10 s |
| fs-1 + fs-2 held 2 s | USB flash mode | same |

## First power-up

After the logo, a short tour (4 cards, 3 s each) shows the basics: knobs, the 3 footswitches, page 2, the "!" chip. It only
ever runs once (any control skips it). The config tab's bottom line shows the model, firmware version and this pedal's ID.

## Saving

Everything (settings, the 8 configs, the learned hum / hiss) is saved to the Seed3's flash about 4 s after you stop changing
things, and comes back at power-on. The plugin keeps the same block in the DAW session.

Saves survive firmware and plugin updates: every setting is stored under a permanent name, so a new build that adds, removes
or reorders settings still loads your configs (new settings start at their defaults). When a setting's meaning changes, the
new build converts the old value. The pedal and the plugin use the same format.

## Start-up

On pitch black, the pg audio logo draws itself while slowly zooming out, "pg audio" fades in underneath, both fade away, and
the main screen fades up out of the black (about 4.5 s; any knob, switch or touch skips it). The logo is the exact curve on the
printed face (`core/PgLogo.h` is generated from `_Tools/pg_generate.py`).

## Health checks (page 2: **health**)

The pedal watches itself all the time. When something looks off, a **"! n"** chip shows in the title on every page (yellow =
warning, pink = fault; tap it to jump to the explanation), and on the home screen a one-line pop-up says what it is. The health page lists every check
with *what it thinks is wrong* and *what to try*. **sens** = relaxed / normal / **strict** (default: flags even a little weird);
**view** = now / how often since power-on.

It watches (19 checks): sound above or below hearing coming in (a quiet note: it's removed anyway), anything outside hearing reaching the output (muted), DC on the input, input clipping, one input side silent, inputs out of phase, no input,
the output limiter working hard, the ears guard acting, audio glitches (muted + reset), processor load, dropouts, a knob push stuck,
a knob turning by itself (loose wire), a footswitch stuck, the touchscreen stuck, and screen-link errors.

It **can't** see: the OUT jack wiring after the converter, the 9 V supply level, or a wire touching the box. Those are the one-time
multimeter checks in `USING-WITH-FIFINE-SC3.md`.

## Screen speed

The screen is driven over SPI at **48 MHz** ("fast", the default) with the Seed3 at 480 MHz: graphs and animations aim for
**60 fps** (the busiest pages use ~92% of the link, so expect about 55-60 fps there; the home screen and small changes are
a solid 60). A full-page change takes ~25 ms. If the picture ever shows wrong pixels or streaks (long or loose jumper
wires can do that at 48 MHz), set **config -> screen: safe** (24 MHz, 25 fps): hold pg-3 and turn, or tap it on the config
page. Keep the screen's SCK and SDI (MOSI) jumpers as short as you can. (48 MHz is above the ILI9341 datasheet's own
figure; these modules are commonly run at 40-80 MHz, and the display self-repair below re-sends everything anyway.)

## Display safety

- Every drawing call is clipped, and numbers that go bad (NaN) can't become screen coordinates, so nothing
  outside the 320 x 240 picture is ever sent.
- The panel's mode registers are re-sent every 2 s and every pixel once a minute. If static or a bad power
  moment scrambles the screen, it fixes itself. If SPI keeps failing, the panel is reset and redrawn.
- The backlight stays off until the first frame is drawn (no garbage flash at power-on). Before a reboot to flash mode the panel is put to sleep properly.
- **Display rest:** after 5 idle minutes the screen goes near-black with a small drifting "pg-1" (no static
  picture to burn in). After 20 minutes the backlight turns off. Any knob, footswitch or touch wakes it, and that first touch only wakes it (it doesn't change anything).
- Nothing sent over SPI can physically damage the ILI9341. The real risks are image retention and a
  scrambled panel, and the points above handle those.

## Memory: the Daisy bootloader

The program now runs from the Daisy bootloader. It is stored in the Seed3's 8 MB QSPI flash and copied into
480 KB of fast SRAM at power-on (same speed as before). All 8 tabs use 159 KB (33%) of it, so there is room
for a lot more. The screen picture (150 KB) lives in the 64 MB SDRAM. The old 128 KB internal flash would
have been full after one more tab: libDaisy alone takes ~80 KB of it.

The power-on wait is 2 s while the bootloader listens for USB, then the pedal starts.

## Setup (Linux)

```
# toolchain (once)
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-newlib dfu-util make

# libraries are already cloned here: libDaisy (v9.0.0, adds Seed3 support) and DaisySP
cd libDaisy && make -j$(nproc) && cd ..
cd DaisySP  && make -j$(nproc) && cd ..

cd pg1
make -j$(nproc)          # builds build/pg1.bin (for the bootloader)
```

**Once:** install the Daisy bootloader. Hold BOOT, tap RESET (Seed3 is now in STM32 flash mode), then:
```
make program-boot        # writes the Daisy bootloader into the internal flash
```
**Every update:** within 2 s of power-on (or after fs-1 + fs-2 held 2 s, which waits forever):
```
make program-dfu         # writes build/pg1.bin to the QSPI flash through the bootloader
```
No tools? https://flash.daisy.audio can do both: "flash bootloader" first, then the program
(`../pg1.bin`).

Recovery is always possible: BOOT + RESET gets you the STM32's own flash mode, which nothing can overwrite.

Not yet confirmed: Electro-Smith's bootloader pages don't mention the Seed3. It uses the same STM32H750 and
also has an 8 MB QSPI flash, but I couldn't confirm the bootloader officially supports it. If the pedal
doesn't start after `make program-dfu`, do BOOT + RESET and `make program-boot` again, then tell me.

If the screen is upside down, set `kFlipScreen = true`. If touches land in the wrong place, change `kTouchSwapXY` / `kTouchFlipX` / `kTouchFlipY`. If a knob counts backwards, set `kReverseEncoder = true`.
