# PG-1 parts

## Tayda cart upload
- **Cart page → Import → Add from file:** `tayda-cart-import.csv` → **Replace current cart**
- (Quick Order page instead: `tayda-cart.csv`)

You should end up with **21 lines**, exact quantities. The Seed3 is a **cartridge**: it plugs into a socket in the left wall (a glued block of jumper ends). The soldered socket board (way 2 in WIRING.md) is a later upgrade: its parts aren't in the cart.

| SKU | Qty | Each | Line | Part | Used for |
|---|---:|---:|---:|---|---|
| A-5883 | 1 | $12.99 | $12.99 | 1590XX Aluminum Diecast Enclosure WHITE | the box (drilled + printed via drill.taydakits.com) |
| A-8180 | 1 | $7.20 | $7.20 | 2.4in SPI touch TFT 240x320 ILI9341 + XPT2046, 4 M3 holes | color touchscreen |
| A-6331 | 4 | $1.59 | $6.36 | Rotary encoder 20 detents + push switch, D shaft (Alpha RE111F) | pg-1, pg-2, pg-3, pg-4 |
| A-2850 | 4 | $1.29 | $5.16 | Knurled aluminium knob, white, 20 mm, 6 mm set screw | 4 knobs (your pick) |
| A-1091 | 3 | $1.97 | $5.91 | Soft-touch momentary footswitch SPST-NO (PBS24B4) | pg-a, pg-b, pg-c (your pick) |
| (Mouser) | 2 | ~$3 | ~$6 | Alps RK09L1240015: dual 10k LINEAR, centre click, 6 mm D shaft (not sold by Tayda) | pg-line (input gain) + pg-hp (output gain): analog, centre = unity |
| A-8567 | 2 | $0.69 | $1.38 | White ripple knob 14 mm | the two small knobs |
| A-2599 | 3 | $1.20 | $3.60 | KN2310 pink aluminium footswitch cap 23 mm (for PBS-24) | pink caps on pg-a, pg-b, pg-c |
| A-2237 | 1 | $0.13 | $0.13 | DC power jack 2.1mm enclosed (12mm) | 9V in |
| A-3482 | 2 | $2.30 | $4.60 | Jumper wires female/female 200mm, pack of 40 | 34 for the Seed3 pins (their ends form the glued socket) + screen GND, 12 for the 2 pots to the carrier board, spares |
| A-8500 | 4 | $0.02 | $0.08 | Nylon standoff M3 x 5mm | screen spacers |
| A-6395 | 4 | $0.11 | $0.44 | M3 x 12mm black socket screw | screen screws through the face |
| A-1247 | 4 | $0.02 | $0.08 | M3 nut | screen nuts |
| A-8519 | 3 | $0.11 | $0.33 | AWG22 stranded wire BLACK 1ft | the ground chain between parts |
| A-7409 | 1 | $0.49 | $0.49 | Black cable ties 3x100mm (100pcs) | tidy the wire bundles |
| A-199 | 1 | $0.17 | $0.17 | 40 pin 2.54mm right-angle single-row male pin header | carrier board: snap into 10 + 6 + 6 + 4 (J10, J20, J19, J21) |
| A-198 | 1 | $0.21 | $0.21 | 2x40 pin 2.54mm double-row male pin header strip | carrier board: snap off a 2 x 4 (fx loop J22) |
| A-8626 | 1 | $1.21 | $1.21 | Mini rotary switch 1 pole 8 position RS16 (9 mm hole, 6 mm spline shaft) | the page selector (settings page 1-8 of the open tab) |
| A-6623 | 1 | $0.49 | $0.49 | Chicken head knob, pink | on the page selector |
| A-5800 | 4 | $0.28 | $1.12 | LDR 10-15k (5 mm) | analog leveller: the light sensors, OC1 / OC2 on the board's lid side (2 spare) |
| A-8041 | 4 | $0.04 | $0.16 | LED 3 mm red, flat top (3.85 mm head) | analog leveller: one pressed on each LDR (2 spare) |
| A-4918 | 1 | $0.14 | $0.14 | Black heat shrink 6 mm, 20 cm | analog leveller: a 15 mm light-tight sleeve over each LED + LDR pair |
| | | | **$52.25** | **Tayda subtotal** | |

The drilled box + UV print are ordered on drill.taydakits.com, not in this cart: drill service $4.50 (up to 40 holes) + face UV print $3.50.

**Budget ($150 total):** see the table at the top of `AMAZON-LIST.md`.

## Breadboard test first (no soldering): `tayda-cart-benchtest.csv`
Add these to the same Tayda order (one shipping fee). With them the Seed3, screen, knobs, footswitches and jacks all run on the desk
before anything is drilled or soldered: see `05-Wiring-and-Schematics/BENCH-TEST.md`.

| SKU | Qty | Each | Line | Part | Used for |
|---|---:|---:|---:|---|---|
| A-2372 | 1 | $2.49 | $2.49 | 830-point solderless breadboard | the Seed3 + encoders plug into it |
| A-3478 | 1 | $0.90 | $0.90 | Jumper wires female/male 200mm, pack of 40 | screen pins → breadboard |
| A-3480 | 1 | $1.11 | $1.11 | Jumper wires male/male 200mm, pack of 40 | breadboard row to row |
| A-5498 | 2 | $1.50 | $3.00 | Alligator clip to male jumper, 10 lines | clip onto jack and footswitch lugs |
| | | | **$7.50** | | |

## Not sold by Tayda

Shopping list with Amazon search words: `AMAZON-LIST.md`.

| Item | ~Price | Notes |
|---|---|---|
| Daisy **Seed3** **with headers** (its male pins already soldered on: they plug into the sockets) | ~$30 | daisy.audio or a dealer. A spare one just plugs in. |
| 9V **center-negative** pedal adapter, ≥300 mA | $10-20 | Boss PSA-style or a pedal power supply |
| 3.5 mm → 1/4" **TRS** stereo cable (male-male) | ~$8 | SC3 line out → pedal IN |
| **3.5 mm ground loop isolator** (audio transformer type) | ~$10 | between the SC3 and pedal IN: no wire connection, so nothing from the pedal can reach the SC3 / PC |
| **Powered** desktop speakers with a 3.5 mm aux input (e.g. Creative Pebble, USB-powered) | ~$25 | cheap, safe first listening test; plain unpowered speakers won't work |
| 1/4" TRS male → 3.5 mm TRS female adapter | ~$5 | pedal OUT → the speakers' aux cable |
| (optional) 1/4" TRS → 2× 1/4" TS "insert" Y-cable | ~$8 | pedal OUT → a mixer's two line inputs (left + right) |
| **Hot glue gun, high-temp, with a narrow nozzle + sticks** (e.g. a dual-temp mini gun) | ~$10-20 | glues the jumper-end socket block into the window (way 1), and a dab on the screen's plugs |

## Tools
Wire strippers, flush cutters, multimeter, a 3 mm drill bit + cheap hand drill or pin vise (4 board holes), wrenches 10-14 mm (or adjustable), 1.5 mm hex key (knobs), 2.5 mm hex key (M3 screws).

## Carrier board: parts Tayda doesn't sell (buy with the pots at Mouser)
| Qty | Part | For |
|---|---|---|
| 2 | Neutrik NMJ6HFD2 (1/4" stereo PCB jack, plastic nose) | in + out jacks (J1, J2) |
| 1 | Mornsun B0505S-1WR3 (isolated 5 V, SIP-4). Same pins, if it's out of stock: Murata NME0505SC or Recom RB-0505S | isolated power (U3) |

(Headers come from Tayda: A-199 + A-198. The fx loop is closed with 2 short female/female jumper wires from the
A-3482 pack: "s l" to "r l" and "s r" to "r r". No sound without them.)

## Amazon (Prime): small ready-made boards, pins already soldered
| Qty | Part | For |
|---|---|---|
| 1 | **PCF8574 I/O expansion board** (I2C, 3.3 V ok; most have an input and a pass-through header) | reads the 8-way page selector, plugs onto the expansion header |
| 1 | **B0505S-1WR3** (pins in order GND, Vin, 0 V, +Vo) | the carrier board's isolated power (or with the Mouser order) |
