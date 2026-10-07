# Breadboard test (no box, no soldering)

Run the whole pedal on your desk first: Seed3, screen, 4 knobs, 3 footswitches, IN and OUT jacks. Same pin numbers as `WIRING.md`,
so the firmware is exactly the one the finished pedal uses. Nothing is soldered, so every part can go in the box later.

Extra parts (Tayda, `02-Parts-and-Cart/tayda-cart-benchtest.csv`): breadboard, female/male jumpers, male/male jumpers, 2 packs of alligator-clip jumpers.
Power comes from your computer's USB-C cable; the 9V jack isn't used on the bench. **Never plug in 9 V on the bench.**

## Layout
```
 breadboard rows 1-20                       the 2 long rails on each edge
   a b c [d] e  | gap |  f g [h] i j
          Seed3 pins 1-20 in column d (pin 1 at row 1),  pins 40-21 in column h (pin 40 at row 1)
          → USB-C end at row 1, hanging off the end of the board
```
- The Seed3 sits across the middle gap: one pin row in column **d**, the other in column **h**. Press it in evenly; it's stiff the first time.
- Every Seed3 pin then has free holes beside it in its own breadboard row (a, b, c on one side; i, j on the other). That's where its wire goes.
- **Rails:** one M/M jumper from pin 40's row to the left **–** rail = main ground. One from pin 20's row to the right **–** rail = audio ground.
  Keep the two grounds apart, like the two chains in the pedal.

## Wiring (wire list in `WIRING.md`)
- **Screen:** female/male jumpers, female end on the screen's pin, male end into the Seed3 pin's row. GND → left rail.
- **Encoders:** push each into its own spot lower on the breadboard (rows 30 and up). If the 2 push-switch legs don't line up, bend them gently.
  Male/male jumpers: A, B, push → their Seed3 rows. Middle pin C and the 2nd push leg → left rail.
- **Footswitches:** alligator clip on one lug → its Seed3 row (male end in the breadboard), clip on the other lug → left rail.
- **IN / OUT jacks:** clips on tip, ring and sleeve. Tip/ring → their Seed3 rows, sleeve → **right** rail (audio ground).
  Make sure no two clips touch each other. A bit of tape over a clip's jaws helps.

## Then
1. USB-C from the computer to the Seed3, flash the firmware (`06-Firmware-DaisySeed/README.md`).
2. The splash and tour should play. Turn and push every knob, press every footswitch, tap the screen.
3. Open the **health** tab: it says what it thinks is wrong (a knob that never moves = check that knob's 3 wires).
4. Sound: SC3 line out → ground loop isolator → IN jack, OUT jack → your headset, **volume low** at first.

When it all works, unclip one part at a time and build it into the box.
