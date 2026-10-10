# Top artwork (UV print)

- **Upload this file:** `pg1-face-uv-print.pdf`. It is exactly 117 × 141 mm (Tayda's 1590XX side A artboard), vector only,
  CMYK with **black = 0/0/0/100**, the accent **pink = 0/58/20/0** (the top dot of each knob ring, the small labels) and the
  **gems** (pixel-art diamonds in 4 deep sea blues, from light 58/4/12/0 to the outline 100/78/35/50), in a layer named
  **CMYK**. A second layer, **RDG_GLOSS**, is a copy of everything in the RDG_GLOSS spot colour: Tayda prints it last as
  clear gloss, so the whole print is shiny and the gems' white glints (left unprinted) shine too. No RDG_WHITE layer
  (the box is white already). (In a PDF viewer the gloss layer shows as a grey-blue tint over everything: that's normal.)
  All text is converted to outlines, so no fonts are needed. Checked against Tayda's UV guide V2 (Apr 2026): artwork stays ≥2 mm inside the artboard,
  ≥3 mm from every hole and 2 mm from the screen window (their face tolerance is ±0.5 mm), and no objects overlap.
- Optional double-check: Tayda's PDF Analyzer (linked in their UV guide, login pdfman / pdfman).
- Look: white enclosure, black print with a few pink accents (matching the pink footswitch caps); smooth rounded ring around the edge;
  all-lowercase rounded font (Nunito, SIL OFL) for the control names and the plain monospace IBM Plex Mono (SIL OFL) for the small words.
  Labels are pg-1 / pg-2 / pg-3 / pg-4 under the encoders (20 dots each = 20 detents),
  pg-a / pg-b with rings, jack labels "out / 9v / usb-c / in" plus "trs l+r", and the one-line logo with "pg audio" under it.
- `pg1-face-preview.png` is a mockup with knobs, switches and the screen. `pg1-face-artwork.svg` is the editable vector version.

On drill.taydakits.com → **New UV print template**:
1. Enclosure **1590XX**, side **A (Face)**, upload `pg1-face-uv-print.pdf`.
2. Tick **"My Design has a CMYK Color"** and **"My Design has RDG Gloss Spot Colors"** (choose **Varnish**: the shiny
   one). Leave **"My Design has RDG White Spot Color"** unticked (the box is already white).
3. Attach it to the same job as the `pg-1` drill template so the print lines up with the holes.

To change any text or position, edit `../../_Tools/pg_generate.py` and run it again. It rebuilds the
drill table, artwork and drawings from the same numbers, so the holes and the print can't drift apart:

```
pip install fonttools
python3 ../../_Tools/pg_generate.py
```
