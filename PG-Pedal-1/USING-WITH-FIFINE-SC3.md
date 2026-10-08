# Using PG-1 with a Fifine SC3

```
SC3 LINE OUT (3.5 mm) ──[ground loop isolator]──[3.5 mm → 1/4" TRS stereo cable]──► pedal IN
pedal OUT ──[1/4" → 3.5 mm adapter]──► your headset
```

## Input: use the SC3's LINE OUT
- LINE OUT is a fixed, clean stereo level, the best match for the pedal. Auto-leveling handles the rest.
- The SC3 headphone/headset output also works. Set the SC3 volume to about half, because a full-volume headphone output can clip the Seed3 input (max about 1 V RMS).
- Cable: **3.5 mm male → 1/4" (6.35 mm) male, TRS stereo**. A mono (TS) cable loses the right channel.

## Output: straight to the headset
- **The Seed3 has no headphone amp.** Its outputs are line level (0 dBFS = 1 V rms); Electro-Smith's datasheet adds a headphone amp
  chip (TPA6110A2, its "Figure 3.9") for headphones. A sensitive gaming headset may still get loud enough straight from the line out,
  so treat it as "test it". If it's too quiet or thin, the fix is that amp chip (it's what the planned carrier board would add).
- **The pedal has no volume knob of its own.** The safety tab caps the level (peak *ceiling*, long-term *ears*), and the input auto-level keeps it steady,
  but how loud that is in your ears depends on the headset. Hold the headset near your ears (not on) the first time.
- **The auto-level undoes the SC3 volume knob** (it brings any level back to its target). So set loudness with the **input** tab's
  **target** (pg-1, -30 to -10 dB). Or switch the input tab off (hold pg-b on it) and use the SC3 volume as usual.
- **Headset with a mic on one plug (4-contact TRRS)**: a plain 1/4" TRS adapter can leave the headphone ground unconnected, which sounds thin, hollow or one-sided.
  Use the headset's separate headphone plug if it has one, or a TRRS splitter (headphone + mic) and plug in the headphone branch.
- If it's too quiet or distorts: tell me. The fix is either a tiny headphone amp (one chip, like Electro-Smith's design)
  or sending the pedal OUT into the SC3's **LINE IN** so you keep listening on the SC3.
  If you try LINE IN, start with the SC3 volume low: if the SC3 also sends LINE IN back out of LINE OUT, it will loop and howl.

## Protecting the SC3 and the PC
A **3.5 mm ground loop isolator** (~$10) between the SC3 and the pedal IN. It's two small audio transformers in a plug:
sound passes through as a magnetic field, and **no wire connects the two sides**. No DC voltage, no 9V, no ground current can
reach the SC3 (and the PC behind it) from the pedal, whatever happens inside the pedal. It also stops ground-loop hum.

Before the first connection, with a multimeter (DC volts) and the pedal powered:
1. **IN jack:** tip to sleeve, ring to sleeve. Write down what you read (expect about 0 V, maybe up to ~1.5 V if the Seed3 input is DC-biased).
   With the isolator in place this can't reach the SC3 either way. **Anything near 9 V = a wiring mistake: stop.**
2. **OUT jack:** tip to sleeve, ring to sleeve: should be about 0 V (below 0.05 V) before a headset goes on.
3. **9V jack:** sleeve lug to the enclosure (bare metal, e.g. a scraped screw hole): must NOT beep / must read open.

Also:
- Use only a **9 V, center-negative, regulated** pedal adapter (a Boss PSA-style one). Not 12 V, not 18 V, not an unregulated wall wart.
- When the pedal's USB-C is plugged into the PC (flashing), unplug the audio cable from the SC3. Two ground paths to the same PC
  only cause hum, not damage, but there's no reason to have both.

## Before plugging in the headset the first time
With the pedal powered, measure DC volts between the OUT jack's tip and sleeve. It should be about 0 V. If it isn't, don't plug the headset in; tell me.
