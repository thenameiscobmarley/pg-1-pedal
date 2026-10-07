# Top artwork (UV print)

- **Upload this file:** `pg1-face-uv-print.pdf`. It is exactly 117 × 141 mm (Tayda's 1590XX side A artboard), vector only,
  CMYK with **black = 0/0/0/100** and no other colors, all in one layer named **CMYK**. There's no RDG_WHITE or RDG_GLOSS layer (not needed on a white box).
  All text is converted to outlines, so no fonts are needed. Checked against Tayda's UV guide V2 (Apr 2026): artwork stays ≥2 mm inside the artboard,
  ≥3 mm from every hole and 2 mm from the screen window (their face tolerance is ±0.5 mm), and no objects overlap.
- Optional double-check: Tayda's PDF Analyzer (linked in their UV guide, login pdfman / pdfman).
- Look: white enclosure with everything in black; smooth rounded ring around the edge; all-lowercase rounded font (Nunito, SIL OFL).
  Labels are pg-1 / pg-2 / pg-3 / pg-4 under the encoders (20 dots each = 20 detents),
  fs-1 / fs-2 with rings, jack labels "out / 9v / usb-c / in" plus "trs l+r", and the one-line logo with "pg audio" under it.
- `pg1-face-preview.png` is a mockup with knobs, switches and the screen. `pg1-face-artwork.svg` is the editable vector version.

On drill.taydakits.com → **New UV print template**:
1. Enclosure **1590XX**, side **A (Face)**, upload `pg1-face-uv-print.pdf`.
2. Color layer: **Yes**. White layer: **No** (the box is already white). Gloss layer: **No**.
3. Attach it to the same job as the `pg-1` drill template so the print lines up with the holes.

To change any text or position, edit `../../_Tools/pg_generate.py` and run it again. It rebuilds the
drill table, artwork and drawings from the same numbers, so the holes and the print can't drift apart:

```
pip install fonttools
python3 ../../_Tools/pg_generate.py
```
