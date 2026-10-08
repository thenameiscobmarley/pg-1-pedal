# Ordering the drilled enclosure (drill.taydakits.com)

**Easiest:** `python3 _Tools/tayda_upload.py --drill-id 12345 --uv-id 23456`
updates your existing templates. By hand, type these in (mm from the **center of each side**, +X right, +Y up).
Diameters **already include Tayda's +0.4 mm powder-coat allowance**.

## Holes (15)
| # | Side | Diameter | X | Y | What |
|---|---|---|---|---|---|
| 1 | A (Face) | 7.6 | -40.5 | -18.5 | pg-1 |
| 2 | A (Face) | 7.6 | -13.5 | -18.5 | pg-2 |
| 3 | A (Face) | 7.6 | 13.5 | -18.5 | pg-3 |
| 4 | A (Face) | 7.6 | 40.5 | -18.5 | pg-4 |
| 5 | A (Face) | 12.6 | -36 | -50 | pg-a |
| 6 | A (Face) | 12.6 | 0 | -50 | pg-b |
| 7 | A (Face) | 12.6 | 36 | -50 | pg-c |
| 8 | A (Face) | 3.6 | -30.74 | 39.75 | screen screw 1 |
| 9 | A (Face) | 3.6 | 36.52 | 39.75 | screen screw 2 |
| 10 | A (Face) | 3.6 | -30.74 | 3.03 | screen screw 3 |
| 11 | A (Face) | 3.6 | 36.52 | 3.03 | screen screw 4 |
| 12 | B (Top) | 9.9 | -40 | 0 | in |
| 13 | B (Top) | 12.4 | -13 | 0 | 9v |
| 14 | B (Top) | 9.9 | 13 | 0 | exp |
| 15 | B (Top) | 9.9 | 40 | 0 | out |

## Shapes (2 rectangles)
| Side | Type | Center X | Center Y | Width | Height | What |
|---|---|---|---|---|---|---|
| A (Face) | Rectangle | 0 | 21.39 | 47.8 | 35.5 | screen window |
| C (Left) | Rectangle | 0 | 15.5 | 19.4 | 52.4 | seed3 window |

**Check the preview:** side B (top) is drawn above the face, and **"in" must sit above the LEFT half**.
Side C (left) is drawn to the left of the face. The tall Seed3 window  sits level with the "usb-c · seed3" label written up the left edge of the face.
It is centred on the wall's middle, so the X direction doesn't matter. Width 19.4 = across the wall, height 52.4 = along it.

Sizes: encoder M7 → 7.2 + 0.4; footswitch M12 → 12.2 + 0.4; TRS jack 3/8" → 9.5 + 0.4; DC jack → 12.0 + 0.4; M3 → 3.2 + 0.4;
screen window = the lit area minus 0.8 mm per side (no black edge); Seed3 window 19 × 52 (+0.4) for the Seed3 cartridge (18 × 51) (the socket is glued in, no screws).

`pg1-drill-template-1to1.pdf`: print at 100% (the bar must measure 50 mm) to sanity-check with real parts.

## How every part mounts (all plain holes, nothing screws into the box)
Every hole Tayda drills is a smooth clearance hole: no part ever gets turned to screw into the box. Each part is pushed
through from inside and held by **its own nut** on the outside (only the nut turns).
| Part | Hole | Held by |
|---|---|---|
| pg-1..pg-4 encoders (A-6331, RE111F-41**B3**: threaded M7 collar) | 7.6 mm | washer + M7 nut on the face |
| pg-hp / pg-line pots (A-6980, threaded M7 collar) | 7.9 mm | washer + nut on the face (both come with the pot) |
| pg-a..pg-c footswitches (A-1091, M12 thread) | 12.6 mm | one nut inside (sets the height), one on the face |
| 4 audio jacks (Neutrik NMJ6HCD2, fitted on the carrier board by JLCPCB) | 11.8 mm | nut + washer outside the top wall: the 4 nuts also hold the board |
| 9V jack (A-2237) | 12.4 mm | its nut outside |
| screen | 4 x 3.6 mm + window | **M3x12 screw from the face -> 5 mm nylon spacer -> screen board -> M3 nut behind it** (the nut holds it; the hole has no thread) |

- **Anti-rotation tab:** the pots (and maybe the encoders) have a small metal tab sticking up beside the collar, meant for a
  tiny second hole. There isn't one: bend the tab flat or snip it off with flush cutters before mounting, so the part sits flat.
  The nut alone holds it fine.
- **Carrier board:** lower it in from the open back (lid off) about 1 cm away from the top wall, then slide it straight toward
  the wall so the 4 jack collars go through their holes, and put the nuts on. Nothing is twisted.
- If the encoders arrive without nuts: they're M7 x 0.75 (same as most pedal pots).
