# PG-1 parts

## Tayda cart upload
- **Cart page → Import → Add from file:** `tayda-cart-import.csv` → **Replace current cart**
- (Quick Order page instead: `tayda-cart.csv`)

You should end up with **16 lines**, exact quantities. The Seed3 is a **cartridge**: it plugs into a small socket board in the left wall, so it's swapped without soldering.

| SKU | Qty | Each | Line | Part | Used for |
|---|---:|---:|---:|---|---|
| A-5883 | 1 | $12.99 | $12.99 | 1590XX Aluminum Diecast Enclosure WHITE | the box (drilled + printed via drill.taydakits.com) |
| A-8180 | 1 | $7.20 | $7.20 | 2.4in SPI touch TFT 240x320 ILI9341 + XPT2046, 4 M3 holes | color touchscreen |
| A-6331 | 4 | $1.59 | $6.36 | Rotary encoder 20 detents + push switch, D shaft (Alpha RE111F) | pg-1, pg-2, pg-3, pg-4 |
| A-2850 | 4 | $1.29 | $5.16 | Knurled aluminium knob, white, 20 mm, 6 mm set screw | 4 knobs (your pick) |
| A-1091 | 3 | $1.97 | $5.91 | Soft-touch momentary footswitch SPST-NO (PBS24B4) | fs-1, fs-2, fs-3 (your pick) |
| A-1121 | 3 | $0.45 | $1.35 | 6.35mm 1/4in stereo (TRS) enclosed jack | in, out, exp (spare) |
| A-2237 | 1 | $0.13 | $0.13 | DC power jack 2.1mm enclosed (12mm) | 9V in |
| A-3482 | 1 | $2.30 | $2.30 | Jumper wires female/female 200mm, pack of 40 | the wires from each part to the socket board (screen end plugs on) |
| A-1310 | 2 | $0.14 | $0.28 | 20-pin female header, 2.54 mm | the Seed3 socket (one per pin row) |
| A-5465 | 1 | $0.80 | $0.80 | Prototyping board 100x50 mm, single side | cut to 68 x 28 mm: the Seed3 socket board (the sockets are soldered to it) |
| A-8500 | 8 | $0.02 | $0.16 | Nylon standoff M3 x 5mm | screen spacers (4) + socket board spacers (4) |
| A-6395 | 8 | $0.11 | $0.88 | M3 x 12mm black socket screw | screen screws (4) + socket board screws through the left wall (4) |
| A-1247 | 8 | $0.02 | $0.16 | M3 nut | screen nuts (4) + socket board nuts (4) |
| A-8519 | 3 | $0.11 | $0.33 | AWG22 stranded wire BLACK 1ft | the ground chain between parts |
| A-7409 | 1 | $0.49 | $0.49 | Black cable ties 3x100mm (100pcs) | tidy the wire bundles |
| A-7776 | 1 | $59.00 | $59.00 | FNIRSI HS-02A soldering iron, 100 W, 100-450 °C, 6 tips, USB-C powered | soldering (needs a USB-C **PD** charger, 65 W or more, see below) |
| | | | **$103.50** | **Tayda subtotal** | |

The drilled box + UV print are ordered on drill.taydakits.com, not in this cart.

## Not sold by Tayda

| Item | ~Price | Notes |
|---|---|---|
| Daisy **Seed3** **with headers** (its male pins already soldered on: they plug into the sockets) | ~$30 | daisy.audio or a dealer. A spare one just plugs in. |
| 9V **center-negative** pedal adapter, ≥300 mA | $10-20 | Boss PSA-style or a pedal power supply |
| 3.5 mm → 1/4" **TRS** stereo cable (male-male) | ~$8 | SC3 line out → pedal IN |
| **USB-C PD charger, 65 W+** (only if you don't have one; a USB-C laptop charger usually is) | ~$20 | powers the HS-02A iron. A phone charger is too weak |
| **0.8 mm 63/37 rosin-core solder** (e.g. Kester 44 or MG Chemicals 4895) | ~$12 | the solder itself. Tayda doesn't sell it |
| **No-clean flux pen** (e.g. MG Chemicals 8341 or Kester 951) | ~$10 | marker-style: wipe it on the joint first, the solder then flows on by itself |
| Brass wool tip cleaner | ~$8 | wipe the hot tip on it between joints, keeps it shiny |
| **3.5 mm ground loop isolator** (audio transformer type) | ~$10 | between the SC3 and pedal IN: no wire connection, so nothing from the pedal can reach the SC3 / PC |
| **Powered** desktop speakers with a 3.5 mm aux input (e.g. Creative Pebble, USB-powered) | ~$25 | cheap, safe first listening test; plain unpowered speakers won't work |
| 1/4" TRS male → 3.5 mm TRS female adapter | ~$5 | pedal OUT → the speakers' aux cable |
| (optional) 1/4" TRS → 2× 1/4" TS "insert" Y-cable | ~$8 | pedal OUT → a mixer's two line inputs (left + right) |
| (optional) hot glue | — | a dab on the screen's jumper plugs so they can't wiggle loose |

## Tools
Wire strippers, flush cutters, multimeter, wrenches 10-14 mm (or adjustable), 1.5 mm hex key (knobs), 2.5 mm hex key (M3 screws).
