#!/usr/bin/env python3
"""PG pedal file generator: one source of truth for every coordinate.

Makes (inside ../PG-Pedal-1):
  03-Drill-Template/  tayda-drill-holes.csv, pg1-drill-template-1to1.svg
  04-Top-Artwork/     pg1-face-uv-print.pdf (CMYK, K=100 only, 117x141mm artboard),
                      pg1-face-artwork.svg, pg1-face-preview.svg
  05-Wiring-and-Schematics/ interior-layout.svg, depth-check.svg, wiring-diagram.svg

Needs: python3 + fonttools  (pip install fonttools)
Font:  Nunito (SIL OFL) instanced to SemiBold/Bold, in ../_Tools/fonts
All units are millimetres. Face origin = centre of the face, +x right, +y toward
the top edge (the edge with the jacks), looking down at the pedal.
"""
import math, os, zlib, csv
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(os.path.dirname(HERE), "PG-Pedal-1")
FONT_LABEL = os.path.join(HERE, "fonts", "Nunito-Bold.ttf")
FONT_TAG = os.path.join(HERE, "fonts", "Nunito-SemiBold.ttf")
FONT_MONO = os.path.join(HERE, "fonts", "IBMPlexMono-Medium.ttf")   # the plain, terminal-like lowercase for small words

# ---------------------------------------------------------------- geometry
FACE_W, FACE_H = 121.1, 145.5          # 1590XX face A, full outer size (Tayda origin = its centre)
SIDE_H = 36.1                          # side B height used by Tayda's box tool
ART_W, ART_H = 117.0, 141.0            # Tayda UV artboard for 1590XX side A
PC = 0.4                               # powder-coat allowance added to every hole

KNOB_Y = -18.5
FS_Y = -50.0
KNOBS = [  # label, x, kind, bare hole (before powder coat)
    ("pg-1", -40.5, "enc", 7.2),
    ("pg-2", -13.5, "enc", 7.2),
    ("pg-3", 13.5, "enc", 7.2),
    ("pg-4", 40.5, "enc", 7.2),
]
PG = "pg-"                             # every control's name starts with this (knobs, footswitches, small pots)
FOOTSW = [(PG + "a", -36.0), (PG + "b", 0.0), (PG + "c", 36.0)]   # pg-c (right) = fair A/B compare
FS_JOB = {PG + "a": "bypass", PG + "b": "next", PG + "c": "a/b"}  # printed next to each name, smaller
FS_CAP = "pink"                        # KN2310 aluminium caps (A-2599) on the PBS-24 footswitches
# two small analog pots left of the screen (10k log dual, A-6980, 14 mm white ripple knobs A-8567), wired to the carrier board
SMALL_POTS = [  # label, x, y, bare hole, what it does
    (PG + "hp", -45.5, 36.0, 7.5, "headphone volume (silent .. +12 dB)"),
    (PG + "line", -45.5, 13.0, 7.5, "line-out level"),
]
SMALL_KNOB_D = 14.0
CARRIER_H = 26.0                       # carrier board depth (mm, from the top wall in)
FS_HOLE = 12.2

# 2.4" MSP2402 module, landscape, 14-pin header on the RIGHT.
# Active area centred on x=0; module centre is 4.85mm right of that.
LCD_CY = 21.39
LCD_AA = (48.96, 36.72)
LCD_WIN = (47.4, 35.1)                 # final opening, 0.8mm INSIDE the lit area each side (no black edge); +PC added below
LCD_PCB = (77.18, 42.72)
LCD_CX = 4.85
SCREWS = [(-30.74, 39.75), (36.52, 39.75), (-30.74, 3.03), (36.52, 3.03)]
SCREW_HOLE = 3.2

# Side B (top edge). All on Y=0 (middle of the wall) so the +Y direction never matters.
SIDE_B = [  # label, x, bare hole, part. The 4 audio jacks are PCB-mount and hold the carrier board up by their nuts.
    ("line in", -42.0, 9.5, "6.35mm TRS PCB jack A-1122 (on the carrier board)"),    # input on the LEFT
    ("9v", -21.0, 12.0, "DC jack A-2237 (12mm cut-out)"),
    ("line out", 0.0, 9.5, "6.35mm TRS PCB jack A-1122 (on the carrier board)"),
    ("no amp", 21.0, 9.5, "6.35mm TRS PCB jack A-1122: processed, same loudness as came in"),
    ("phones", 42.0, 9.5, "6.35mm TRS PCB jack A-1122: headphone amp"),
]

# Seed3 CARTRIDGE: the Seed3 stands on its side in a window in side C (left wall), component side OUT, and plugs
# into 2 x 20-pin female headers on a small socket board screwed inside the wall. USB-C points toward the
# footswitches (-y), BOOT/RESET face outward, so it is plugged in, pressed and swapped from outside, no soldering.
# Side C coordinates: X = across the wall height (0 = middle of the 36.1 wall), Y = same as the face.
SEED_SIDE = "E"                        # the Seed3 cartridge is in the RIGHT wall (side E): the left is for the small pots
SEED_X_SIGN = 1                        # +1 = right wall, -1 = left wall (face x of the wall it's in)
SEED_Y = 15.5                          # window centre along Y (beside the screen)
SEED_WIN = (19.0, 52.0)                # final opening (X across the wall, Y along it); Seed3 is 18 x 51, +PC added
WALL = 2.3                             # 1590XX side wall
INNER_X = -FACE_W / 2 + WALL           # inside of the left wall in face coordinates (-58.25)
SOCK_GAP = 5.0                         # A-8500 5 mm nylon spacer between the wall and the socket board
HDR_H = 8.5                            # A-1053 stackable header body height (its legs are 11 mm)
SOCK_L, SOCK_W = 70.0, 30.0            # socket board: a whole A-1192 30 x 70 double-sided board, no cutting
SOCK_OFF = 1.27                        # its centre sits this much toward the lid from the socket centre (10 rows:
                                       # sockets in the 2nd and 8th row from the face-side edge)
SOCK_COLS, SOCK_ROWS = 24, 10          # its hole grid


JMP_L = 14.0                           # a jumper wire's female end (plastic shell), long
JMP_OUT = 7.0                          # how far the glued jumper ends stick out of the wall (glued on the outside)


def seed_stack():
    """Distances outward from the OUTSIDE face of the left wall (negative = inside the box).
    Socket way 1 (built now): the jumper ends, glued in the window, sticking out JMP_OUT."""
    hdr_top = JMP_OUT
    pcb0 = hdr_top + 2.5                           # Seed3's own black pin spacer sits on the jumper ends
    return dict(block_back=-JMP_L, hdr_top=hdr_top, pcb0=pcb0, pcb1=pcb0 + 1.6, parts=pcb0 + 1.6 + 3.25)


# ---------------------------------------------------------------- drawing model
INK = (0.0, 0.0, 0.0, 1.0)             # CMYK: black
PINK = (0.0, 0.58, 0.20, 0.0)          # CMYK: the accent pink (close to the KN2310 pink caps)
def cmyk_hex(c):
    r, g, b = (round(255 * (1 - c[i]) * (1 - c[3])) for i in range(3))
    return f"#{r:02x}{g:02x}{b:02x}"


class Art:
    """Records vector items in mm (y up). Emits PDF and SVG."""
    def __init__(self):
        self.items = []     # (kind, subpaths, width, cmyk)  kind: fill | stroke

    def fill(self, subpaths, color=INK):
        self.items.append(("fill", subpaths, 0, color))

    def stroke(self, subpaths, w, color=INK):
        self.items.append(("stroke", subpaths, w, color))


def circle_path(cx, cy, r):
    k = 0.5522847498 * r
    return [("M", cx + r, cy),
            ("C", cx + r, cy + k, cx + k, cy + r, cx, cy + r),
            ("C", cx - k, cy + r, cx - r, cy + k, cx - r, cy),
            ("C", cx - r, cy - k, cx - k, cy - r, cx, cy - r),
            ("C", cx + k, cy - r, cx + r, cy - k, cx + r, cy), ("Z",)]


def rrect_path(cx, cy, w, h, r):
    x0, x1, y0, y1 = cx - w / 2, cx + w / 2, cy - h / 2, cy + h / 2
    k = 0.5522847498 * r
    return [("M", x0 + r, y0), ("L", x1 - r, y0), ("C", x1 - r + k, y0, x1, y0 + r - k, x1, y0 + r),
            ("L", x1, y1 - r), ("C", x1, y1 - r + k, x1 - r + k, y1, x1 - r, y1),
            ("L", x0 + r, y1), ("C", x0 + r - k, y1, x0, y1 - r + k, x0, y1 - r),
            ("L", x0, y0 + r), ("C", x0, y0 + r - k, x0 + r - k, y0, x0 + r, y0), ("Z",)]


def poly_path(pts, close=False):
    p = [("M",) + pts[0]] + [("L",) + q for q in pts[1:]]
    return p + [("Z",)] if close else p


class Font:
    def __init__(self, path):
        self.f = TTFont(path)
        self.upm = self.f["head"].unitsPerEm
        self.cmap = self.f.getBestCmap()
        self.gs = self.f.getGlyphSet()
        self.hmtx = self.f["hmtx"]

    def width(self, text, size, track=0.0):
        s = size / self.upm
        return sum(self.hmtx[self.cmap[ord(c)]][0] * s for c in text) + track * (len(text) - 1)

    def outline(self, text, x, y, size, anchor="middle", track=0.0):
        """Glyph outlines as subpaths (cubic), baseline at y."""
        s = size / self.upm
        w = self.width(text, size, track)
        cx = x - (w / 2 if anchor == "middle" else (w if anchor == "end" else 0))
        out = []
        for c in text:
            g = self.cmap[ord(c)]
            pen = RecordingPen()
            self.gs[g].draw(pen)
            cur = None
            for op, args in pen.value:
                T = lambda p: (cx + p[0] * s, y + p[1] * s)
                if op == "moveTo":
                    cur = T(args[0]); out.append(("M",) + cur)
                elif op == "lineTo":
                    cur = T(args[0]); out.append(("L",) + cur)
                elif op == "curveTo":
                    pts = [T(a) for a in args]
                    out.append(("C",) + pts[0] + pts[1] + pts[2]); cur = pts[2]
                elif op == "qCurveTo":
                    pts = [T(a) for a in args]
                    if args[-1] is None:      # implied on-curve closed contour (rare in TT)
                        pts = pts[:-1]
                    # split implied on-curve points
                    for i in range(len(pts) - 1):
                        c1 = pts[i]
                        end = pts[i + 1] if i == len(pts) - 2 else ((pts[i][0] + pts[i + 1][0]) / 2, (pts[i][1] + pts[i + 1][1]) / 2)
                        q0 = cur
                        a = (q0[0] + 2 / 3 * (c1[0] - q0[0]), q0[1] + 2 / 3 * (c1[1] - q0[1]))
                        b = (end[0] + 2 / 3 * (c1[0] - end[0]), end[1] + 2 / 3 * (c1[1] - end[1]))
                        out.append(("C",) + a + b + end); cur = end
                elif op in ("closePath", "endPath"):
                    out.append(("Z",))
            cx += self.hmtx[g][0] * s + track
        return out


def tick(cx, cy, r0, r1, deg):
    a = math.radians(deg)
    return [("M", cx + r0 * math.cos(a), cy + r0 * math.sin(a)), ("L", cx + r1 * math.cos(a), cy + r1 * math.sin(a))]


def logo_points(cx, cy):
    """Single-line logo: flat line -> growing wave -> one loop (a knob) -> flat line."""
    ctrl = [(-25, 0), (-19, 0)]
    for i in range(1, 13):                       # wave, amplitude grows toward the loop
        t = i / 12
        ctrl.append((-19 + 17.5 * t, (1 if i % 2 else -1) * (0.5 + 2.4 * t ** 1.2)))
    ctrl += [(0.6, 0.2), (2.6, 3.4), (3.9, 6.4), (3.1, 8.6), (1.1, 9.1), (-0.4, 7.4),
             (0.2, 4.6), (2.6, 1.7), (5.6, 0.25), (9.0, 0), (25, 0)]
    pts = []
    P = [ctrl[0]] + ctrl + [ctrl[-1]]            # Catmull-Rom through the control points
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(24):
            t = k / 24
            t2, t3 = t * t, t * t * t
            pts.append(tuple(0.5 * (2 * p1[j] + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2
                                    + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3) for j in (0, 1)))
    pts.append(ctrl[-1])
    return [(cx + x, cy + y) for x, y in pts]


# ---------------------------------------------------------------- face artwork
BOX_R = 5.0          # the 1590XX's own corner radius: every rounded corner on the print uses it


def rot90(sp, tx, ty):
    """Turn subpaths 90 degrees anticlockwise about the origin, then move them to (tx, ty)."""
    out = []
    for seg in sp:
        if seg[0] == "Z":
            out.append(seg); continue
        pts = []
        for i in range(1, len(seg), 2):
            pts += [tx - seg[i + 1], ty + seg[i]]
        out.append((seg[0],) + tuple(pts))
    return out


def diamond(cx, cy, r):
    return poly_path([(cx, cy + r), (cx + r, cy), (cx, cy - r), (cx - r, cy)], close=True)


def build_face_art():
    A = Art()
    lab = Font(FONT_LABEL)
    tag = Font(FONT_TAG)
    mono = Font(FONT_MONO)

    # border: a bold line with the box's own 5 mm corners, a hairline just inside it (concentric), and a
    # small diamond tucked into each corner
    bw, bh = ART_W - 5.0, ART_H - 5.0
    A.stroke(rrect_path(0, 0, bw, bh, BOX_R), 0.9)
    A.stroke(rrect_path(0, 0, bw - 3.0, bh - 3.0, BOX_R - 1.5), 0.22)
    for sx in (-1, 1):
        for sy in (-1, 1):
            A.fill(diamond(sx * (bw / 2 - 4.4), sy * (bh / 2 - 4.4), 0.85), PINK)

    # screen: same 5 mm corners, a fine second line 1 mm outside it
    ww, wh = LCD_WIN[0] + PC, LCD_WIN[1] + PC
    A.stroke(rrect_path(0, LCD_CY, ww + 4.0, wh + 4.0, BOX_R), 0.45)
    A.stroke(rrect_path(0, LCD_CY, ww + 6.0, wh + 6.0, BOX_R + 1.0), 0.18)

    # divider between the screen and the knobs: hairlines out from a centre diamond
    dy = -2.6
    A.stroke(poly_path([(-49.0, dy), (-3.2, dy)]), 0.22)
    A.stroke(poly_path([(3.2, dy), (49.0, dy)]), 0.22)
    A.fill(diamond(0, dy, 1.4), PINK)
    A.fill(diamond(-50.6, dy, 0.6))
    A.fill(diamond(50.6, dy, 0.6))

    # endless encoders: 20 detent dots, every 5th one a little larger
    for name, x, kind, _ in KNOBS:
        if kind == "enc":
            for i in range(20):
                a = math.radians(90 - i * 18)
                rr = 0.62 if i == 0 else (0.45 if i % 5 == 0 else 0.28)
                A.fill(circle_path(x + 12.0 * math.cos(a), KNOB_Y + 12.0 * math.sin(a), rr), PINK if i == 0 else INK)
        else:
            for i in range(11):
                deg = 240 - i * 30          # 7 o'clock -> 5 o'clock clockwise
                A.stroke(tick(x, KNOB_Y, 11.0, 12.6 if i in (0, 5, 10) else 11.9, deg), 0.35)
            A.fill(lab.outline("0", x - 9.6, KNOB_Y - 11.6, 2.4))
            A.fill(lab.outline("+40", x + 10.0, KNOB_Y - 11.6, 2.4))
        A.fill(lab.outline(name, x, KNOB_Y - 18.2, 4.3))

    # the two small pots left of the screen: 11 dots over the 300 degree turn (silent .. loudest, the top one pink)
    for name, x, y, _, _ in SMALL_POTS:
        r = SMALL_KNOB_D / 2 + 2.0
        for i in range(11):
            a = math.radians(240 - i * 30)
            rr = 0.5 if i in (0, 10) else 0.24
            A.fill(circle_path(x + r * math.cos(a), y + r * math.sin(a), rr), PINK if i == 10 else INK)
        A.fill(lab.outline(name, x, y - r - 3.2, 3.0))

    # footswitches: a ring with a hairline ring inside it, and 4 small ticks at the quarters
    for name, x in FOOTSW:
        A.stroke(circle_path(x, FS_Y, 10.0), 0.45)
        A.stroke(circle_path(x, FS_Y, 11.0), 0.18)
        for deg in (45, 135, 225, 315):
            A.stroke(tick(x, FS_Y, 11.6, 12.4, deg), 0.3)
        # "pg-a bypass": the name, then what it does in the plain mono letters, centred together
        job = FS_JOB[name]
        w1, w2, gap = lab.width(name, 4.3), mono.width(job, 2.7), 1.4
        x0 = x - (w1 + gap + w2) / 2
        A.fill(lab.outline(name, x0, FS_Y - 15.0, 4.3, anchor="start"))
        A.fill(mono.outline(job, x0 + w1 + gap, FS_Y - 15.0, 2.7, anchor="start"))

    # jack labels along the top edge (match side B holes)
    # (one row, high enough to clear the logo's loop; every audio jack is stereo trs)
    for name, x, _, _ in SIDE_B:
        A.fill(mono.outline(name, x, 61.0, 3.0))
        A.stroke(poly_path([(x - 1.2, 64.4), (x, 65.5), (x + 1.2, 64.4)]), 0.4)
        if name == "9v": # centre-negative polarity mark: minus - ( . ) - plus
            px, py = x, 57.6
            A.stroke(poly_path([(px - 4.6, py), (px - 3.6, py)]), 0.3)          # minus
            A.stroke(poly_path([(px - 3.0, py), (px - 0.25, py)]), 0.3)         # lead to the centre pin
            A.fill(circle_path(px, py, 0.45))                                   # centre pin
            arc = [(px + 1.25 * math.cos(math.radians(a)), py + 1.25 * math.sin(math.radians(a))) for a in range(40, 321, 10)]
            A.stroke(poly_path(arc), 0.3)                                       # the sleeve, open towards plus
            A.stroke(poly_path([(px + 1.25, py), (px + 3.4, py)]), 0.3)         # lead from the sleeve
            A.stroke(poly_path([(px + 3.9, py), (px + 5.1, py)]), 0.3)          # plus
            A.stroke(poly_path([(px + 4.5, py - 0.6), (px + 4.5, py + 0.6)]), 0.3)
    # Seed3 cartridge: label written up the edge on its side, level with the window ("usb-c" at the USB end)
    A.fill(rot90(mono.outline("usb-c \u00b7 seed3", 0, 0, 2.6), SEED_X_SIGN * 51.4, SEED_Y))

    # logo + tagline, the logo's flat ends finished with small diamonds
    A.stroke([("M",) + p if i == 0 else ("L",) + p for i, p in enumerate(logo_points(0, 51.0))], 0.75)
    A.fill(diamond(-26.4, 51.0, 0.75))
    A.fill(diamond(26.4, 51.0, 0.75))
    A.fill(tag.outline("pg audio \u00b7 pg-1", 0, 44.0, 3.6, track=0.15))   # brand + model, as on the screen
    return A


def check_clearances():
    """Make sure no printed item sits on a hole (cheap sanity check)."""
    holes = [(x, KNOB_Y, d / 2 + PC / 2) for _, x, _, d in KNOBS] + \
            [(x, FS_Y, FS_HOLE / 2) for _, x in FOOTSW] + [(x, y, 2.5) for x, y in SCREWS] + \
            [(x, y, d / 2 + PC / 2) for _, x, y, d, _ in SMALL_POTS]
    for (x1, y1, r1) in holes:
        for (x2, y2, r2) in holes:
            if (x1, y1) < (x2, y2) and math.hypot(x1 - x2, y1 - y2) < r1 + r2 + (2 if max(r1, r2) <= 1.0 else 8):
                raise SystemExit(f"holes too close: {(x1, y1)} {(x2, y2)}")


# ---------------------------------------------------------------- emitters
MM2PT = 72 / 25.4


def to_pdf_ops(A, ox, oy):
    """ox, oy = mm offset to move the origin to the page's lower-left."""
    f = lambda v: f"{v:.3f}"
    out = ["1 J", "1 j"]
    for kind, sp, w, col in A.items:
        out.append("%.3f %.3f %.3f %.3f k %.3f %.3f %.3f %.3f K" % (col + col))
        for seg in sp:
            if seg[0] == "Z":
                out.append("h"); continue
            pts = [((seg[i] + ox) * MM2PT, (seg[i + 1] + oy) * MM2PT) for i in range(1, len(seg), 2)]
            s = " ".join(f(a) + " " + f(b) for a, b in pts)
            out.append(s + {"M": " m", "L": " l", "C": " c"}[seg[0]])
        out.append("f" if kind == "fill" else f"{w * MM2PT:.3f} w S")
    return "\n".join(out)


def write_pdf(path, A, w_mm, h_mm, title):
    # Everything sits in ONE layer named "CMYK" (an optional-content group), as Tayda's UV guide asks.
    ops = "/OC /L0 BDC\n" + to_pdf_ops(A, w_mm / 2, h_mm / 2) + "\nEMC"
    content = zlib.compress(ops.encode())
    W, H = w_mm * MM2PT, h_mm * MM2PT
    objs = [
        b"<< /Type /Catalog /Pages 2 0 R /OCProperties << /OCGs [6 0 R] /D << /Order [6 0 R] /ON [6 0 R] >> >> >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        (f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {W:.3f} {H:.3f}] /TrimBox [0 0 {W:.3f} {H:.3f}] "
         f"/ArtBox [0 0 {W:.3f} {H:.3f}] /Contents 4 0 R /Resources << /Properties << /L0 6 0 R >> >> >>").encode(),
        f"<< /Length {len(content)} /Filter /FlateDecode >>\nstream\n".encode() + content + b"\nendstream",
        f"<< /Title ({title}) /Creator (pg_generate.py) >>".encode(),
        b"<< /Type /OCG /Name (CMYK) >>",
    ]
    buf = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offs = []
    for i, o in enumerate(objs, 1):
        offs.append(len(buf))
        buf += f"{i} 0 obj\n".encode() + o + b"\nendobj\n"
    xref = len(buf)
    buf += f"xref\n0 {len(objs) + 1}\n0000000000 65535 f \n".encode()
    for o in offs:
        buf += f"{o:010d} 00000 n \n".encode()
    buf += f"trailer\n<< /Size {len(objs) + 1} /Root 1 0 R /Info 5 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode()
    open(path, "wb").write(buf)


def svg_d(sp):
    out = []
    for seg in sp:
        if seg[0] == "Z":
            out.append("Z"); continue
        pts = [(seg[i], -seg[i + 1]) for i in range(1, len(seg), 2)]
        out.append(seg[0] + " ".join(f"{a:.3f},{b:.3f}" for a, b in pts))
    return " ".join(out)


def art_svg_group(A, color="#000"):
    g = []
    for kind, sp, w, col in A.items:
        c = color if col == INK else cmyk_hex(col)
        if kind == "fill":
            g.append(f'<path d="{svg_d(sp)}" fill="{c}"/>')
        else:
            g.append(f'<path d="{svg_d(sp)}" fill="none" stroke="{c}" stroke-width="{w}" '
                     f'stroke-linecap="round" stroke-linejoin="round"/>')
    return "\n".join(g)


def svg_doc(w, h, body, vb=None, bg=None):
    vb = vb or (-w / 2, -h / 2, w, h)
    bgr = f'<rect x="{vb[0]}" y="{vb[1]}" width="{vb[2]}" height="{vb[3]}" fill="{bg}"/>' if bg else ""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}mm" height="{h}mm" '
            f'viewBox="{vb[0]} {vb[1]} {vb[2]} {vb[3]}">\n{bgr}\n{body}\n</svg>\n')


def text(x, y, s, size=2.4, anchor="middle", color="#000", weight="normal"):
    s = s.replace("&", "&amp;").replace("<", "&lt;")
    return (f'<text x="{x:.2f}" y="{-y:.2f}" font-family="DejaVu Sans, sans-serif" font-size="{size}" '
            f'text-anchor="{anchor}" fill="{color}" font-weight="{weight}">{s}</text>')


# ---------------------------------------------------------------- outputs
def holes_table():
    rows = []
    for name, x, kind, d in KNOBS:
        part = "encoder A-6331 (M7 bushing)" if kind == "enc" else "dual pot A-1920 (M7 bushing)"
        rows.append(("A", "hole", name, x, KNOB_Y, round(d + PC, 2), "", "", part))
    for name, x in FOOTSW:
        rows.append(("A", "hole", name, x, FS_Y, round(FS_HOLE + PC, 2), "", "", "soft-touch footswitch A-1091 PBS24B4 (M12)"))
    for name, x, y, d, what in SMALL_POTS:
        rows.append(("A", "hole", name, x, y, round(d + PC, 2), "", "", "10k log dual pot A-6980 (9 mm, round shaft): " + what))
    for i, (x, y) in enumerate(SCREWS, 1):
        rows.append(("A", "hole", f"screen screw {i}", x, y, round(SCREW_HOLE + PC, 2), "", "", "M3 screw for 2.4in screen + board"))
    rows.append(("A", "rectangle", "screen window", 0.0, LCD_CY, "", round(LCD_WIN[0] + PC, 2),
                 round(LCD_WIN[1] + PC, 2), "2.4in ILI9341 A-8180 visible area"))
    for name, x, d, part in SIDE_B:
        rows.append(("B", "hole", name, x, 0.0, round(d + PC, 2), "", "", part))
    rows.append((SEED_SIDE, "rectangle", "seed3 window", 0.0, SEED_Y, "", round(SEED_WIN[0] + PC, 2), round(SEED_WIN[1] + PC, 2),
                 "Daisy Seed3 cartridge (glued jumper-end socket)"))
    return rows


def write_drill(rows):
    d = os.path.join(ROOT, "03-Drill-Template")
    with open(os.path.join(d, "tayda-drill-holes.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["side", "type", "label", "x_mm", "y_mm", "diameter_mm", "width_mm", "height_mm", "part"])
        for r in rows:
            w.writerow([r[0], r[1], r[2], f"{r[3]:.2f}", f"{r[4]:.2f}"] + list(r[5:]))

    # 1:1 printable paper template: face A with side B unfolded above it
    body = []
    fy = -SIDE_H / 2 - 4          # gap
    # Side B strip (above face), centre at y = FACE_H/2 + SIDE_H/2
    sbcy = FACE_H / 2 + SIDE_H / 2
    body.append(f'<rect x="{-FACE_W/2}" y="{-(sbcy+SIDE_H/2)}" width="{FACE_W}" height="{SIDE_H}" fill="none" stroke="#000" stroke-width="0.3"/>')
    body.append(f'<rect x="{-FACE_W/2}" y="{-FACE_H/2}" width="{FACE_W}" height="{FACE_H}" rx="4" fill="none" stroke="#000" stroke-width="0.3"/>')
    body.append(text(0, sbcy + SIDE_H / 2 + 2, "SIDE B (top edge, jacks) - centre = its own (0,0)", 2.6))
    scx = SEED_X_SIGN * (FACE_W / 2 + SIDE_H / 2)   # the side with the Seed3 window, drawn beside the face
    body.append(f'<rect x="{scx-SIDE_H/2}" y="{-FACE_H/2}" width="{SIDE_H}" height="{FACE_H}" fill="none" stroke="#000" stroke-width="0.3"/>')
    body.append(f'<text x="{scx}" y="{FACE_H/2+5}" font-family="DejaVu Sans" font-size="2.6" text-anchor="middle">SIDE {SEED_SIDE} ({"right" if SEED_X_SIGN > 0 else "left"})</text>')
    body.append(text(0, -FACE_H / 2 - 4.5, "SIDE A (face) - 1590XX 121.1 x 145.5 mm - print at 100% / actual size", 2.6))
    for cx, cy in ((0, 0), (0, sbcy), (scx, 0)):
        body.append(f'<path d="M{cx-3},{-cy} h6 M{cx},{-cy-3} v6" stroke="#888" stroke-width="0.2"/>')
    for side, typ, name, x, y, dia, ww, hh, part in rows:
        yy = y if side in ("A", SEED_SIDE) else sbcy + y
        x0 = x
        x = x + (scx if side == SEED_SIDE else 0)
        if typ == "hole":
            r = dia / 2
            body.append(f'<circle cx="{x}" cy="{-yy}" r="{r}" fill="none" stroke="#c00" stroke-width="0.3"/>')
            body.append(f'<path d="M{x-r-1.5},{-yy} h{2*r+3} M{x},{-yy-r-1.5} v{2*r+3}" stroke="#c00" stroke-width="0.15"/>')
            body.append(text(x, yy + r + 1.2, f"{name}  Ø{dia}", 1.9, color="#c00"))
            body.append(text(x, yy - r - 2.6, f"({x0:g}, {y:g})", 1.6, color="#555"))
        else:
            body.append(f'<rect x="{x-ww/2}" y="{-(yy+hh/2)}" width="{ww}" height="{hh}" fill="none" stroke="#c00" stroke-width="0.3"/>')
            body.append(text(x, yy + 1.0, f"{name} {ww} x {hh}", 2.2, color="#c00"))
            body.append(text(x, yy - 2.6, f"centre ({x0:g}, {y:g})", 1.8, color="#555"))
    # 50mm scale bar to verify print scale
    body.append(f'<path d="M{-FACE_W/2},{FACE_H/2+9} h50" stroke="#000" stroke-width="0.4"/>')
    body.append(text(-FACE_W / 2 + 25, -FACE_H / 2 - 12.8, "this bar must measure exactly 50 mm", 2.2))
    W, H = 190, FACE_H + SIDE_H + 30
    vb = (-80 if SEED_X_SIGN > 0 else -115, -(FACE_H / 2 + SIDE_H + 12), W, H)
    open(os.path.join(d, "pg1-drill-template-1to1.svg"), "w").write(svg_doc(W, H, "\n".join(body), vb, "#fff"))


def write_art(A):
    d = os.path.join(ROOT, "04-Top-Artwork")
    write_pdf(os.path.join(d, "pg1-face-uv-print.pdf"), A, ART_W, ART_H, "PG-1 face UV print 1590XX side A")
    open(os.path.join(d, "pg1-face-artwork.svg"), "w").write(svg_doc(ART_W, ART_H, art_svg_group(A)))

    # realistic preview: white box, holes, knobs, screen, switches, jacks
    p = []
    p.append('<defs><radialGradient id="kn" cx="40%" cy="35%" r="70%"><stop offset="0" stop-color="#4a4a4a"/>'
             '<stop offset="1" stop-color="#0b0b0b"/></radialGradient>'
             '<radialGradient id="pk" cx="40%" cy="35%" r="70%"><stop offset="0" stop-color="#ffb3d9"/><stop offset="1" stop-color="#e0559c"/></radialGradient>'
             '<radialGradient id="fs" cx="40%" cy="35%" r="70%"><stop offset="0" stop-color="#f4f4f4"/>'
             '<stop offset="1" stop-color="#9a9a9a"/></radialGradient>'
             '<filter id="sh" x="-20%" y="-20%" width="140%" height="140%"><feDropShadow dx="0.6" dy="1.2" stdDeviation="1.2" flood-opacity="0.35"/></filter></defs>')
    p.append(f'<rect x="{-FACE_W/2}" y="{-FACE_H/2}" width="{FACE_W}" height="{FACE_H}" rx="{BOX_R}" fill="#fbfbf9" stroke="#d9d9d4" stroke-width="0.5" filter="url(#sh)"/>')
    # jacks/USB poking from the top edge
    for name, x, dd, _ in SIDE_B:
        p.append(f'<rect x="{x-dd/2-1.5}" y="{-FACE_H/2-3.2}" width="{dd+3}" height="3.4" rx="0.8" fill="#8d8d8d"/>')
    st = seed_stack()   # the Seed3 cartridge standing out of the left wall (seen from above: its edge)
    sx0 = FACE_W / 2 + st["pcb0"] if SEED_X_SIGN > 0 else -FACE_W / 2 - st["parts"]   # the Seed3 sticking out of its wall
    p.append(f'<rect x="{sx0}" y="{-(SEED_Y+25.5)}" width="{st["parts"]-st["pcb0"]}" height="51" rx="0.6" fill="#2a2a2a"/>')
    p.append(f'<rect x="{sx0+0.3}" y="{-(SEED_Y-25.5)}" width="3.0" height="1.6" fill="#b5b5b5"/>')
    p.append(art_svg_group(A, "#111"))
    # screen
    ww, wh = LCD_WIN[0] + PC, LCD_WIN[1] + PC
    p.append(f'<rect x="{-ww/2}" y="{-(LCD_CY+wh/2)}" width="{ww}" height="{wh}" rx="0.6" fill="#0d1014"/>')
    shot = os.path.join(d, "screen-home.png")   # the real home screen, rendered by the pedal's core
    if os.path.exists(shot):
        import base64
        b64 = base64.b64encode(open(shot, "rb").read()).decode()
        p.append(f'<image x="{-ww/2}" y="{-(LCD_CY+wh/2)}" width="{ww}" height="{wh}" preserveAspectRatio="none" '
                 f'style="image-rendering:pixelated" href="data:image/png;base64,{b64}"/>')
    for i, (x, y) in enumerate(SCREWS):
        p.append(f'<circle cx="{x}" cy="{-y}" r="2.75" fill="#151515"/><circle cx="{x}" cy="{-y}" r="1.1" fill="#3a3a3a"/>')
    for name, x, kind, _ in KNOBS:
        # A-2850 knurled aluminium knob, white, 20 mm
        p.append(f'<circle cx="{x}" cy="{-KNOB_Y}" r="10" fill="#efefed" stroke="#c9c9c6" stroke-width="0.4" filter="url(#sh)"/>')
        for k in range(36):
            a = math.radians(k * 10)
            p.append(f'<path d="M{x+8.6*math.cos(a):.2f},{-KNOB_Y+8.6*math.sin(a):.2f} L{x+10*math.cos(a):.2f},{-KNOB_Y+10*math.sin(a):.2f}" stroke="#bdbdba" stroke-width="0.35"/>')
        p.append(f'<circle cx="{x}" cy="{-KNOB_Y}" r="8" fill="#f7f7f5"/>')
        p.append(f'<path d="M{x},{-KNOB_Y-7.6} v3.2" stroke="#222" stroke-width="0.7" stroke-linecap="round"/>')
    for name, x in FOOTSW:
        hexpts = " ".join(f"{x+8.2*math.cos(math.radians(30+60*i)):.2f},{-FS_Y+8.2*math.sin(math.radians(30+60*i)):.2f}" for i in range(6))
        p.append(f'<polygon points="{hexpts}" fill="url(#fs)" stroke="#888" stroke-width="0.3" filter="url(#sh)"/>')
        # the pink KN2310 aluminium cap (23 mm) on the plunger
        p.append(f'<circle cx="{x}" cy="{-FS_Y}" r="11.5" fill="url(#pk)" stroke="#b9487f" stroke-width="0.4" filter="url(#sh)"/>')
        p.append(f'<circle cx="{x}" cy="{-FS_Y}" r="9.6" fill="none" stroke="#ffd1e8" stroke-width="0.5" stroke-opacity="0.7"/>')
    for name, x, y, _, _ in SMALL_POTS: # 14 mm white ripple knobs
        p.append(f'<circle cx="{x}" cy="{-y}" r="{SMALL_KNOB_D/2}" fill="#f1f1ef" stroke="#c9c9c6" stroke-width="0.4" filter="url(#sh)"/>')
        for k in range(24):
            a = math.radians(k * 15)
            p.append(f'<path d="M{x+5.6*math.cos(a):.2f},{-y+5.6*math.sin(a):.2f} L{x+7*math.cos(a):.2f},{-y+7*math.sin(a):.2f}" stroke="#c6c6c3" stroke-width="0.35"/>')
        p.append(f'<path d="M{x},{-y-5.4} v2.4" stroke="#222" stroke-width="0.6" stroke-linecap="round"/>')
    open(os.path.join(d, "pg1-face-preview.svg"), "w").write(
        svg_doc(FACE_W + 10, FACE_H + 14, "\n".join(p), (-(FACE_W + 10) / 2, -(FACE_H + 14) / 2 - 1, FACE_W + 10, FACE_H + 14), "#e9e7e2"))


def write_interior():
    d = os.path.join(ROOT, "05-Wiring-and-Schematics")
    p = []
    p.append(f'<rect x="{-FACE_W/2}" y="{-FACE_H/2}" width="{FACE_W}" height="{FACE_H}" rx="5" fill="#fff" stroke="#000" stroke-width="0.4"/>')
    p.append(f'<rect x="{-FACE_W/2+2.6}" y="{-FACE_H/2+2.6}" width="{FACE_W-5.2}" height="{FACE_H-5.2}" rx="3" fill="none" stroke="#aaa" stroke-width="0.25" stroke-dasharray="1 1"/>')
    # the carrier board (dashed): it hangs from the 4 audio jacks' nuts along the top wall
    p.append(f'<rect x="{-FACE_W/2+4}" y="{-(FACE_H/2-2.5)}" width="{FACE_W-8}" height="{CARRIER_H}" rx="1.5" fill="#cfe9cf" fill-opacity="0.5" stroke="#1f9d3a" stroke-width="0.35" stroke-dasharray="1.5 1"/>')
    p.append(text(0, FACE_H / 2 - 2.5 - CARRIER_H + 1.6, "carrier board (pcbway): amp, buffers, protection, pin headers", 1.9, color="#1f6d2a"))
    # jacks bodies (inside, from the top wall)
    for name, x, dd, _ in SIDE_B:
        depth, wdt = (16.0, 9.5) if name == "9v" else (21.0, 13.0)
        p.append(f'<rect x="{x-wdt/2}" y="{-FACE_H/2+2.5}" width="{wdt}" height="{depth}" fill="#ffe2a8" stroke="#a66" stroke-width="0.3"/>')
        p.append(text(x, FACE_H / 2 - 2.5 - depth / 2, name, 2.2))
    # the two small pots (9 mm body) left of the screen
    for name, x, y, _, _ in SMALL_POTS:
        p.append(f'<rect x="{x-4.75}" y="{-(y+4.75)}" width="9.5" height="9.5" fill="#e5c7f0" stroke="#84a" stroke-width="0.3"/>')
        p.append(text(x, y - 7.6, name, 2.2))
    lw, lh = LCD_PCB
    p.append(f'<rect x="{LCD_CX-lw/2}" y="{-(LCD_CY+lh/2)}" width="{lw}" height="{lh}" fill="#bcd3f5" fill-opacity="0.55" stroke="#36c" stroke-width="0.4"/>')
    p.append(f'<rect x="{LCD_CX+lw/2-4.6}" y="{-(LCD_CY+lh/2-4.8)}" width="2.6" height="{33.02+2.5}" fill="#36c"/>')
    p.append(text(LCD_CX + lw / 2 - 8, LCD_CY + 15, "14-pin header", 1.9, "end", "#36c"))
    p.append(text(0, LCD_CY + 2, "2.4in screen module (top layer)", 2.3, color="#235"))
    # the Seed3 cartridge, drawn as for the left wall and mirrored when it's in the right one
    st = seed_stack()
    ox = -FACE_W / 2
    X = lambda d: ox - d
    M = lambda x, w: -(x + w) if SEED_X_SIGN > 0 else x   # a rect's left edge after mirroring
    T = lambda x: -x if SEED_X_SIGN > 0 else x
    def box(x, w, y, h, style):
        p.append(f'<rect x="{M(x, w)}" y="{y}" width="{w}" height="{h}" {style}/>')
    box(X(JMP_OUT), JMP_L, -(SEED_Y + 25.4), 50.8, 'fill="#222"')                      # the glued jumper-end block
    box(X(st["parts"]), st["parts"] - st["pcb0"], -(SEED_Y + 25.5), 51, 'fill="#333" stroke="#000" stroke-width="0.3"')
    box(X(st["parts"]) - 1.55, 3.25, -(SEED_Y - 25.5), 1.6, 'fill="#bbb"')
    box(X(st["pcb0"]), 2.5, -(SEED_Y + 25.5), 51, 'fill="#555"')                        # Seed3 pin spacer
    box(X(JMP_OUT - JMP_L), 10, -(SEED_Y + 25.4), 50.8, 'fill="none" stroke="#1f9d3a" stroke-width="0.3" stroke-dasharray="1 0.8"')
    for k_, (t_, c_) in enumerate([("Seed3", "#fff"), ("plugs into", "#fff"), ("its jumper", "#fff"), ("ends, hot-", "#fff"), ("glued into", "#fff"), ("a block", "#fff")]):
        p.append(text(T(X(JMP_OUT - JMP_L / 2)), SEED_Y + 8 - k_ * 2.5, t_, 1.7, color=c_))
    p.append(text(T(X(JMP_OUT - JMP_L - 5)), SEED_Y + 3, "wires", 1.8, color="#1f6d2a"))
    p.append(text(T(X(st["parts"]) + 1), SEED_Y - 25.5 - 3.2, "USB-C", 1.9, color="#000"))
    for x, y in SCREWS:
        p.append(f'<circle cx="{x}" cy="{-y}" r="2.6" fill="#fff" stroke="#000" stroke-width="0.4"/><circle cx="{x}" cy="{-y}" r="1.5" fill="#000"/>')
    for name, x, kind, _ in KNOBS:
        w_ = 12 if kind == "enc" else 17
        p.append(f'<rect x="{x-w_/2}" y="{-(KNOB_Y+6)}" width="{w_}" height="12" fill="#e5c7f0" stroke="#84a" stroke-width="0.3"/>')
        p.append(text(x, KNOB_Y - 9, name, 2.4))
    for name, x in FOOTSW:
        p.append(f'<circle cx="{x}" cy="{-FS_Y}" r="6.6" fill="#ddd" stroke="#555" stroke-width="0.3"/>')
        p.append(text(x, FS_Y - 10, name, 2.4))
    p.append(text(0, -FACE_H / 2 - 5, "INTERIOR TOP VIEW (looking down through the face) - 1:1", 2.6))
    p.append(text(0, -FACE_H / 2 - 9, "screen: M3x12 screw | face | 5mm spacer | screen PCB | M3 nut.   Seed3 socket: jumper ends hot-glued in the window", 2.2))
    p.append(text(0, -FACE_H / 2 - 13, "Seed3 pulls straight out of the right wall: keep the green dashed zones free for the wires", 2.2, color="#a00"))
    W, H = FACE_W + 32, FACE_H + 30
    open(os.path.join(d, "interior-layout.svg"), "w").write(svg_doc(W, H, "\n".join(p), (-W / 2, -FACE_H / 2 - 8, W, H), "#fff"))



DEPTHS = [  # item, from mm, to mm  (0 = outside of the face, 36.1 = open edge where the lid sits)
    ("face (aluminium)", 0, 3.0),
    ("5mm nylon spacer", 3.0, 8.0),
    ("screen glass + PCB", 4.1, 9.6),
    ("screen parts / M3 nut + screw tip", 9.6, 12.6),
    ("encoders + pins + solder", 3.0, 15.5),
    ("TRS jacks (top wall zone only)", 10.2, 26.0),
    ("Seed3 window in the right wall", 18.05 - (SEED_WIN[0] + PC) / 2, 18.05 + (SEED_WIN[0] + PC) / 2),
    ("Seed3 jumper-end block (in the window)", 18.05 - 8.9, 18.05 + 8.9),
    ("footswitch body + lugs (bottom zone only)", 3.0, 33.0),
    ("lid", 36.1, 39.3),
]


def write_depth():
    d = os.path.join(ROOT, "05-Wiring-and-Schematics")
    s, x0, row = 3.2, 78, 7
    W = x0 + 40 * s + 10
    H = 16 + row * len(DEPTHS) + 14
    q = [text(4, -8, "SIDE VIEW DEPTH CHECK (mm below the face surface)", 4.2, "start")]
    for mm in range(0, 41, 5):
        q.append(f'<path d="M{x0+mm*s},12 V{12+row*len(DEPTHS)}" stroke="#ddd" stroke-width="0.3"/>')
        q.append(text(x0 + mm * s, -(16 + row * len(DEPTHS) + 4), str(mm), 3))
    for i, (name, a, b) in enumerate(DEPTHS):
        y = 12 + i * row
        col = "#cfe9cf" if "Seed3" in name or "jumper" in name or "foam" in name else ("#bcd3f5" if "screen" in name or "spacer" in name else "#ffe2a8")
        if name in ("lid", "face (aluminium)"):
            col = "#bbb"
        q.append(f'<rect x="{x0+a*s}" y="{y+1}" width="{(b-a)*s}" height="{row-2}" fill="{col}" stroke="#555" stroke-width="0.3"/>')
        q.append(text(x0 - 3, -(y + row - 2.2), name, 3, "end"))
    q.append(f'<path d="M{x0+36.1*s},8 V{14+row*len(DEPTHS)}" stroke="#c00" stroke-width="0.6" stroke-dasharray="2 1"/>')
    q.append(text(x0 + 36.1 * s, -6, "lid line 36.1", 3, color="#c00"))
    open(os.path.join(d, "depth-check.svg"), "w").write(svg_doc(W, H, "\n".join(q), (0, -2, W, H), "#fff"))


# ---------------------------------------------------------------- wiring diagram
WIRE = {"audio_l": "#9a9a9a", "audio_r": "#e0b000", "enc": "#1f9d3a", "lcd": "#1f5fd1",
        "fs": "#8b4513", "pwr": "#d11f1f"}
SEED_RIGHT = {1: "D0 (unused)", 2: "D1", 3: "D2", 4: "D3", 5: "D4", 6: "D5", 7: "D6", 8: "D7", 9: "D8",
              10: "D9", 11: "D10", 12: "D11", 13: "D12", 14: "D13", 15: "D14", 16: "AUDIO IN L",
              17: "AUDIO IN R", 18: "AUDIO OUT L", 19: "AUDIO OUT R", 20: "AGND"}
SEED_LEFT = {21: "3V3 A", 22: "D15", 23: "D16", 24: "D17", 25: "D18", 26: "D19", 27: "D20", 28: "D21",
             29: "D22", 30: "D23", 31: "D24", 32: "D25", 33: "D26", 34: "D27", 35: "D28", 36: "D29",
             37: "D30", 38: "3V3 D", 39: "VIN", 40: "DGND"}
# (box title, side, [(pin label, seed pin, colour)], [ground pin labels])
WIRING = [
    ("OUT jack (TRS)", "R", [("ring = R", 19, "audio_r"), ("tip = L", 18, "audio_l")], ["sleeve"]),
    ("IN jack (TRS)", "R", [("ring = R", 17, "audio_r"), ("tip = L", 16, "audio_l")], ["sleeve"]),
    ("pg-3 encoder", "R", [("B", 15, "enc"), ("A", 14, "enc")], ["C (middle)", "push 2nd pin"]),
    ("screen (A-8180)", "R", [("4 RESET", 13, "lcd"), ("5 DC", 12, "lcd"), ("6 SDI", 11, "lcd")], ["2 GND"]),
    ("screen (cont.)", "R", [("7 SCK", 9, "lcd"), ("3 CS", 8, "lcd")], []),
    ("pg-2 encoder", "R", [("push", 7, "enc"), ("B", 6, "enc"), ("A", 5, "enc")], ["C (middle)", "push 2nd pin"]),
    ("pg-1 encoder", "R", [("push", 4, "enc"), ("B", 3, "enc"), ("A", 2, "enc")], ["C (middle)", "push 2nd pin"]),
    ("pg-3 encoder", "L", [("push", 22, "enc")], []),
    ("screen", "L", [("8 LED", 23, "lcd")], []),
    ("fs-1 footswitch", "L", [("lug", 24, "fs")], ["other lug"]),
    ("fs-2 footswitch", "L", [("lug", 25, "fs")], ["other lug"]),
    ("fs-3 footswitch", "R", [("lug", 10, "fs")], ["other lug"]),
    ("pg-4 encoder", "L", [("A", 26, "enc"), ("B", 27, "enc"), ("push", 28, "enc")], ["C (middle)", "push 2nd pin"]),
    ("screen touch", "L", [("10 T_CLK", 29, "lcd"), ("11 T_CS", 30, "lcd"), ("12 T_DIN", 31, "lcd"),
                           ("13 T_DO", 32, "lcd"), ("14 T_IRQ", 33, "lcd")], []),
    ("screen", "L", [("1 VCC", 38, "pwr")], []),
    ("9V DC jack", "L", [("+ (sleeve lug)", 39, "pwr")], ["- (center pin)"]),
]


def write_wiring_diagram():
    d = os.path.join(ROOT, "05-Wiring-and-Schematics")
    P = 8.0                                   # pin pitch in the drawing
    top = 40.0
    sx0, sx1 = 175.0, 235.0                   # Seed3 body
    ry = lambda pin: top + (20 - pin) * P     # right column, pin 20 at top
    ly = lambda pin: top + (pin - 21) * P     # left column, pin 21 at top
    o = []
    T = lambda x, y, t, sz=3.2, a="middle", c="#111", w="normal": o.append(
        f'<text x="{x:.1f}" y="{y:.1f}" font-family="DejaVu Sans, sans-serif" font-size="{sz}" text-anchor="{a}" '
        f'fill="{c}" font-weight="{w}">{t}</text>')
    T(205, 12, "PG-1 WIRING: every part goes to a Seed3 socket place (Seed3 pin numbers, seen from outside, USB-C at the bottom)", 5, w="bold")
    T(205, 20, "Coloured line = one jumper wire, its female end in that socket place.   \u23da = that point goes on a ground chain (black wire, part to part): see the 2 chains at the bottom", 3.4)
    # Seed3
    o.append(f'<rect x="{sx0}" y="{top-8}" width="{sx1-sx0}" height="{20*P+12}" rx="3" fill="#1d1d1d"/>')
    T(205, top + 70, "Daisy", 7, c="#fff", w="bold"); T(205, top + 80, "Seed3", 7, c="#fff", w="bold")
    T(205, top + 92, "(parts side, as seen", 3.2, c="#bbb"); T(205, top + 97, "from outside the box)", 3.2, c="#bbb")
    o.append(f'<rect x="{205-6}" y="{top+20*P-2}" width="12" height="7" rx="1.5" fill="#c9c9c9"/>')
    T(205, top + 20 * P + 14, "USB-C end", 3.2)
    for pin, name in SEED_RIGHT.items():
        y = ry(pin)
        o.append(f'<circle cx="{sx1-4}" cy="{y}" r="1.6" fill="#d4a017"/>')
        T(sx1 - 7, y + 1.1, f"{pin}", 2.8, "end", "#fff")
        T(sx1 + 2, y - 1.2, name, 2.4, "start", "#555")
    for pin, name in SEED_LEFT.items():
        y = ly(pin)
        o.append(f'<circle cx="{sx0+4}" cy="{y}" r="1.6" fill="#d4a017"/>')
        T(sx0 + 7, y + 1.1, f"{pin}", 2.8, "start", "#fff")
        T(sx0 - 2, y - 1.2, name, 2.4, "end", "#555")
    gnd = lambda x, y: o.append(f'<path d="M{x},{y} v2.2 M{x-2.4},{y+2.2} h4.8 M{x-1.6},{y+3.3} h3.2 M{x-0.8},{y+4.4} h1.6" stroke="#111" stroke-width="0.5" fill="none"/>')
    # ground pins on the Seed3
    AUD = "#0a7f86"
    o.append(f'<path d="M{sx1-4},{ry(20)} H{sx1+30}" stroke="{AUD}" stroke-width="1.6"/>'); gnd(sx1 + 30, ry(20))
    T(sx1 + 34, ry(20) + 1.2, "AUDIO ground chain", 2.9, "start", AUD, "bold")
    o.append(f'<path d="M{sx0+4},{ly(40)} H{sx0-30}" stroke="#111" stroke-width="1.6"/>'); gnd(sx0 - 30, ly(40))
    T(sx0 - 32, ly(40) + 9, "MAIN ground chain", 2.9, "middle", "#111", "bold")
    for title, side, pins, gnds in WIRING:
        ys = [ry(p) if side == "R" else ly(p) for _, p, _ in pins]
        chain = "audio" if "jack (TRS)" in title else "main"
        lines = [(title, True)] + [("\u23da " + g + " \u2192 " + chain, False) for g in gnds]
        mid = (min(ys) + max(ys)) / 2
        h = max(max(ys) - min(ys) + 6.4, 3.4 * len(lines) + 2.4)
        y0 = mid - h / 2
        bx, bw = (300.0, 74.0) if side == "R" else (52.0, 74.0)
        o.append(f'<rect x="{bx}" y="{y0:.1f}" width="{bw}" height="{h:.1f}" rx="1.5" fill="#f4f4f4" stroke="#333" stroke-width="0.35"/>')
        for k, (txt, bold) in enumerate(lines):
            ty = y0 + 3.6 + 3.4 * k
            if side == "R":
                T(bx + bw - 2, ty, txt, 2.9 if bold else 2.6, "end", "#111" if bold else (AUD if chain == "audio" else "#444"), "bold" if bold else "normal")
            else:
                T(bx + 2, ty, txt, 2.9 if bold else 2.6, "start", "#111" if bold else "#444", "bold" if bold else "normal")
        for (lab, pin, col), y in zip(pins, ys):
            px = bx if side == "R" else bx + bw
            seedx = sx1 - 4 if side == "R" else sx0 + 4
            o.append(f'<path d="M{seedx},{y} H{px}" stroke="{WIRE[col]}" stroke-width="1.3"/>')
            o.append(f'<circle cx="{px}" cy="{y}" r="1.1" fill="#333"/>')
            T(px + (3 if side == "R" else -3), y + 1.1, lab, 2.9, "start" if side == "R" else "end")
    # legend
    ly0 = top + 20 * P + 24
    items = [("audio_l", "left audio (tip)"), ("audio_r", "right audio (ring)"), ("enc", "encoders"),
             ("lcd", "screen"), ("fs", "footswitches"), ("pwr", "power")]
    for i, (k, lab) in enumerate(items):
        x = 40 + i * 58
        o.append(f'<path d="M{x},{ly0} h12" stroke="{WIRE[k]}" stroke-width="1.6"/>')
        T(x + 15, ly0 + 1.1, lab, 3.2, "start")
    T(205, ly0 + 10, "Encoders: 3 pins = A, C, B (C is the middle one). The 2 pins on the other side are the push switch. "
      "Screen pin 9 (SDO) is not used. Free for later: pins 34, 36, 37 (35 = exp jack later).", 3.0)
    T(205, ly0 + 15, "The Seed3 is never soldered: the jumpers' female ends, glued into a block in the wall window, are its socket (WIRING.md). "
      "Seen from inside it's a mirror image of this view: use seed-socket-board.png.", 3.0)
    # the two ground chains, step by step: solder a short black wire from each point to the next
    def chain_row(y, title, colour, steps):
        T(12, y + 1.2, title, 3.3, "start", colour, "bold")
        x = 60.0
        for k, st in enumerate(steps):
            w = 6 + 1.75 * max(len(t) for t in st.split("|"))
            last = k == len(steps) - 1
            o.append(f'<rect x="{x}" y="{y-5}" width="{w}" height="{10 if "|" in st else 8}" rx="1.5" '
                     f'fill="{"#1d1d1d" if last else "#f4f4f4"}" stroke="{colour}" stroke-width="0.6"/>')
            for j, t in enumerate(st.split("|")):
                T(x + w / 2, y - 0.4 + j * 3.6, t, 2.7, "middle", "#fff" if last else "#111")
            if not last:
                o.append(f'<path d="M{x+w+0.8},{y} h3.4 m-1.6,-1.4 l1.6,1.4 l-1.6,1.4" stroke="{colour}" stroke-width="0.7" fill="none"/>')
            x += w + 5.2
    cy = ly0 + 26
    T(205, cy - 2, "GROUND CHAINS: one black wire from each point to the next, in this order. Don't join the two chains, and don't loop back.", 3.4, w="bold")
    chain_row(cy + 9, "audio chain", AUD, ["IN jack|sleeve", "OUT jack|sleeve", "jumper in|socket 20"])
    chain_row(cy + 23, "main chain", "#111", ["9V jack|- (center)", "screen|2 GND *", "pg-4|C + push 2", "pg-3|C + push 2", "pg-2|C + push 2",
                                              "pg-1|C + push 2", "fs-3|other lug", "fs-2|other lug", "fs-1|other lug", "jumper in|socket 40"])
    T(205, cy + 34, "* the screen GND is a plug-on jumper at the screen end: cut its other end off and solder that end into the chain. "
      "(exp jack sleeve joins the audio chain only once the exp jack is used.)", 2.9)
    W, H = 410, cy + 40
    open(os.path.join(d, "wiring-diagram.svg"), "w").write(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">'
        f'<rect width="{W}" height="{H}" fill="#fff"/>' + "\n".join(o) + "</svg>\n")


def write_socket_board():
    """The socket board's BACK (the side you solder), seen from inside the box. Mirror image of the Seed3 view."""
    d = os.path.join(ROOT, "05-Wiring-and-Schematics")
    s_ = 4.0                                          # drawing scale: px per mm
    W, H = 330, 196
    cx, cy = W / 2, 92
    o = []
    T = lambda x, y, t, sz=3.2, a="middle", c="#111", w="normal": o.append(
        f'<text x="{x:.1f}" y="{y:.1f}" font-family="DejaVu Sans, sans-serif" font-size="{sz}" text-anchor="{a}" '
        f'fill="{c}" font-weight="{w}">{t}</text>')
    T(cx, 12, "SEED3 SOCKET PLACES, seen from INSIDE the box (way 1 jumper block: same places; drawn: way 2 board). 4:1", 4.6, w="bold")
    T(cx, 20, "Face (top of the pedal) is UP. USB-C end / footswitches on the LEFT. IN jack end on the RIGHT.", 3.4)
    bw, bh = SOCK_L * s_ / 1.0, SOCK_W * s_
    bw, bh = bw * 0.62, bh * 0.62                       # fit the page; positions below use the same factor
    k = s_ * 0.62
    bcy = cy + SOCK_OFF * k                             # the board's centre (the sockets' centre is cy)
    o.append(f'<rect x="{cx-bw/2}" y="{bcy-bh/2}" width="{bw}" height="{bh}" rx="2" fill="#1f8a4c" stroke="#123" stroke-width="0.6"/>')
    for i in range(SOCK_COLS):                          # every hole has its own tinned ring, nothing joins them
        for j in range(SOCK_ROWS):
            x = cx + (i - (SOCK_COLS - 1) / 2) * 2.54 * k; y = bcy + (j - (SOCK_ROWS - 1) / 2) * 2.54 * k
            o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.0" fill="none" stroke="#c9ccd0" stroke-width="1.1"/>')
    T(cx - bw / 2 - 3, bcy - bh / 2 + 4, "face-side edge", 2.8, "end", "#333")
    T(cx - bw / 2 - 3, bcy + bh / 2 - 1, "lid-side edge", 2.8, "end", "#333")
    rowA, rowB = cy - 7.62 * k, cy + 7.62 * k           # pins 1-20 toward the face (top), 21-40 toward the lid
    for pin in range(1, 41):
        pos = pin - 1 if pin <= 20 else 40 - pin        # 0 = USB-C end
        x = cx + (-24.13 + pos * 2.54) * k
        y = rowA if pin <= 20 else rowB
        name = SEED_RIGHT.get(pin) or SEED_LEFT.get(pin)
        used = any(p_ == pin for _, _, ps, _ in WIRING for _, p_, _ in ps) or pin in (20, 40)
        col = "#000" if pin == 1 else ("#d4a017" if used else "#8a7a50")
        o.append(f'<rect x="{x-1.25*k}" y="{y-1.25*k}" width="{2.5*k}" height="{2.5*k}" fill="{col}" stroke="#222" stroke-width="0.3"/>')
        ly = y - 2.6 * k if pin <= 20 else y + 3.6 * k
        T(x, ly, str(pin), 2.6, c="#fff", w="bold")
        short = name.replace("AUDIO ", "").replace(" (unused)", "")
        ty = cy - bh / 2 - 6 if pin <= 20 else cy + bh / 2 + 9
        o.append(f'<text x="{x:.1f}" y="{ty:.1f}" font-family="DejaVu Sans, sans-serif" font-size="2.5" fill="#333" '
                 f'text-anchor="{"start" if pin <= 20 else "end"}" transform="rotate(-60 {x:.1f} {ty:.1f})">{short}</text>')
    T(cx + (-24.13) * k - 26, rowA + 1, "pin 1: KEY", 2.9, "end", "#a00", "bold")
    T(cx, cy + bh / 2 + 34, "Pin 1 = D0 (not used). Snip pin 1 off the Seed3 and block place 1 (hot glue in that jumper end, or a resistor leg):", 3.1)
    T(cx, cy + bh / 2 + 39, "now the Seed3 only goes in the right way round. Backwards would put 9 V on the wrong pins!", 3.1, c="#a00", w="bold")
    T(cx, cy + bh / 2 + 47, "Way 1: the jumper for each pin goes in the place with that number. Way 2: its wire is soldered to that header leg, under heat shrink.", 3.1)
    T(cx, cy + bh / 2 + 57, "Way 2 (later): sockets in the 2nd and 8th row from the face-side edge, 2 empty columns at each end; mount it however suits you then.", 3.1)
    open(os.path.join(d, "seed-socket-board.svg"), "w").write(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">'
        f'<rect width="{W}" height="{H}" fill="#fff"/>' + "\n".join(o) + "</svg>\n")


# ---------------------------------------------------------------- logo for the pedal's start-up screen
def write_logo_header():
    """core/PgLogo.h: the same logo curve as the print (units = mm, line at y = 0) and "pg audio" as an
    8-bit coverage image (10 px per mm), so the screen's splash matches the face exactly."""
    import subprocess, tempfile
    tag = Font(FONT_TAG)
    pts = [(x, y - 51.0) for x, y in logo_points(0, 51.0)]
    # keep it light: drop points closer than 0.12 mm to the previous one
    slim = [pts[0]]
    for p in pts[1:]:
        if math.hypot(p[0] - slim[-1][0], p[1] - slim[-1][1]) >= 0.12:
            slim.append(p)
    text_base = 44.0 - 51.0
    sp = tag.outline("pg audio", 0, text_base, 3.6, track=0.15)
    xs = [sp_[i] for sp_ in sp if sp_[0] != "Z" for i in range(1, len(sp_), 2)]
    ys = [sp_[i + 1] for sp_ in sp if sp_[0] != "Z" for i in range(1, len(sp_), 2)]
    x0, x1, y0, y1 = min(xs) - 0.4, max(xs) + 0.4, min(ys) - 0.4, max(ys) + 0.4
    ppm = 10
    W, H = int(math.ceil((x1 - x0) * ppm)), int(math.ceil((y1 - y0) * ppm))
    with tempfile.TemporaryDirectory() as td:
        svg = os.path.join(td, "t.svg"); png = os.path.join(td, "t.png"); raw = os.path.join(td, "t.gray")
        open(svg, "w").write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="{x0} {-y1} {x1-x0} {y1-y0}">'
                             f'<path d="{svg_d(sp)}" fill="#fff"/></svg>')
        subprocess.run(["rsvg-convert", "-w", str(W), "-h", str(H), "-b", "none", svg, "-o", png], check=True)
        subprocess.run(["magick", png, "-alpha", "extract", "-depth", "8", f"gray:{raw}"], check=True)
        data = open(raw, "rb").read()
    assert len(data) == W * H
    out = ["// Generated by _Tools/pg_generate.py (write_logo_header) - don't edit by hand.",
           "// The pg audio logo: the exact curve printed on the face (mm, line at y = 0, y up) and the tagline",
           "// as an 8-bit coverage image (10 px per mm), for the start-up screen.",
           "#pragma once", "#include <cstdint>", "", "namespace pg", "{", "namespace logo", "{",
           f"constexpr int   kPts = {len(slim)};",
           "static const float kXY[kPts * 2] = {"]
    flat = [f"{v:.3f}f" for p in slim for v in p]
    for i in range(0, len(flat), 12):
        out.append("    " + ", ".join(flat[i:i + 12]) + ",")
    out += ["};", "constexpr float kStroke = 0.75f; // line width, mm",
            f"constexpr int   kTextW = {W}, kTextH = {H};",
            f"constexpr float kTextX0 = {x0:.3f}f, kTextY1 = {y1:.3f}f, kTextPxPerMm = {ppm}.f; // image top-left, in mm",
            f"constexpr float kTop = {max(p[1] for p in slim):.3f}f, kBottom = {y0:.3f}f; // overall height, mm",
            "static const uint8_t kText[kTextW * kTextH] = {"]
    for i in range(0, len(data), 32):
        out.append("    " + ",".join(str(b) for b in data[i:i + 32]) + ",")
    out += ["};", "} // namespace logo", "} // namespace pg", ""]
    open(os.path.join(ROOT, "06-Firmware-DaisySeed", "pg1", "core", "PgLogo.h"), "w").write("\n".join(out))



if __name__ == "__main__":
    check_clearances()
    write_depth()
    write_wiring_diagram()
    write_socket_board()
    write_logo_header()
    rows = holes_table()
    write_drill(rows)
    art = build_face_art()
    write_art(art)
    write_interior()
    for r in rows:
        print(r)
