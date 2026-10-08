# Ordering the drilled enclosure (drill.taydakits.com)

**Easiest:** `python3 _Tools/tayda_upload.py --drill-id 12345 --uv-id 34248`
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
