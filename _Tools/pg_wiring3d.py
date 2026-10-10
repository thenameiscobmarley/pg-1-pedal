#!/usr/bin/env python3
"""PG-1 wiring in 3D: the open box seen from above (lid off, face down on the table, the jacks' wall away from you),
every part where it really sits, and the wires arching from lug to lug, one page per job.

Same wire list as the flat drawing (pg_wiring.WIRES). Positions: face millimetres from pg_generate.py / make_board.py
(X right / Y up as you look at the FRONT of the pedal, h = depth below the face's outer surface). Seen from inside the
picture is mirrored left-right: the in jack (front left) is on your RIGHT. -> wiring-3d.pdf (+ one svg per page).
"""
import html
import math
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pg_wiring import WIRES, PARTS, COL, UNUSED, OUT   # noqa: E402

TITLE = {p[0]: p[1] for p in PARTS}
PINS = {p[0]: p[3] for p in PARTS}

# ---------------------------------------------------------------- the box and where things are (face mm, h = depth)
BOX_X, BOX_Y, BOX_D, WALL, FACE_T = 60.55, 72.75, 36.1, 2.3, 3.0
IN_X, IN_Y = BOX_X - WALL, BOX_Y - WALL
ENC_X, ENC_Y = [-40.5, -13.5, 13.5, 40.5], -18.5
FS_X, FS_Y = [-36.0, 0.0, 36.0], -50.0
POT = {"pl": -48.0, "ph": 48.0}
POT_Y = 52.0
SW = {"pw": (-14.0, 56.0), "ps": (14.0, 56.0)}
LIGHT = {"li": -48.0, "lo": 48.0}
LIGHT_Y = 37.0
LCD_C, LCD_PCB = (4.85, 21.39), (77.18, 42.72)
DC_Y, BAY_Y, SEED_Y = -34.0, 17.0, 15.5
BOARD_H = 27.0                                       # carrier board (F side toward the face), 1.6 thick
BOARD = [(-45.0, 70.2), (45.0, 70.2), (45.0, 57.0), (55.5, 57.0), (55.5, 43.0), (30.0, 43.0), (30.0, 12.0),
         (-34.0, 12.0), (-34.0, 43.0), (-55.5, 43.0), (-55.5, 57.0), (-45.0, 57.0)]
HDR = {"j10": (3.5, 14.0), "j21": (-7.4, 14.0), "j20": (-53.5, 44.2), "j19": (40.8, 44.2)}   # first pin; +x, 2.54
PCF_C, PCF_H = (-2.0, -4.0), 33.0                    # the PCF8574 board on the lid (velcro), seen through the lid


def pin3d():
    """every lug / pin end a wire is soldered or plugged to: 'part.pin' -> (X, Y, h)"""
    P = {}
    # 9 V jack in the left wall: its 3 lugs at the inner end
    P["dc.- centre"], P["dc.+ sleeve"], P["dc.switch"] = (-44.0, DC_Y, 18.0), (-44.0, DC_Y + 3.5, 15.5), (-44.0, DC_Y - 3.5, 15.5)
    # the 2 rotary switches: commons near the middle, the outer lugs round the edge (pole A on the left half)
    for sid, (x, y) in SW.items():
        ring = {"A com": (180, 2.6), "A1": (125, 6.6), "A2": (155, 6.6), "A3": (205, 6.6), "A4": (235, 6.6),
                "B com": (0, 2.6), "B1": (305, 6.6), "B2": (335, 6.6), "B3": (25, 6.6), "B4": (55, 6.6)}
        for name, (a, r) in ring.items():
            if name in PINS[sid]:
                P[f"{sid}.{name}"] = (x + r * math.cos(math.radians(a)), y + r * math.sin(math.radians(a)), 17.0)
    # carrier headers: right-angle pins pointing toward the screen, on the board's face side
    for hid, (x0, y0) in HDR.items():
        for i, name in enumerate(PINS[hid]):
            P[f"{hid}.{name}"] = (x0 + 2.54 * i, y0 - 6.0, BOARD_H - 2.5)
    # gain pots: pins point toward the screen; front gang (l) nearer the face, back gang (r) deeper; pin 1 on the left
    for pid, x in POT.items():
        for name in PINS[pid]:
            n, g = name.split()
            P[f"{pid}.{name}"] = (x + 2.5 * (int(n) - 2), POT_Y - 7.5, 9.0 if g == "l" else 15.0)
    for lid, x in LIGHT.items():   # level lights: 2 legs (the out one bent toward the top wall)
        bend = 4.0 if lid == "lo" else 0.0
        P[f"{lid}.leg 1"], P[f"{lid}.leg 2"] = (x - 1.27, LIGHT_Y + bend, 10.0), (x + 1.27, LIGHT_Y + bend, 10.0)
    # screen: 14-pin header down its right end, pin 1 at the top
    hx = LCD_C[0] + LCD_PCB[0] / 2 - 2.5
    for i, name in enumerate(PINS["lcd"]):
        P[f"lcd.{name}"] = (hx, LCD_C[1] + (6.5 - i) * 2.54, 14.0)
    for k, x in enumerate(ENC_X):   # encoders: A C B on the footswitch side, the 2 push pins on the screen side
        e = f"e{k+1}"
        P[f"{e}.A"], P[f"{e}.C"], P[f"{e}.B"] = (x - 2.5, ENC_Y - 7.5, 16.0), (x, ENC_Y - 7.5, 16.0), (x + 2.5, ENC_Y - 7.5, 16.0)
        P[f"{e}.push"], P[f"{e}.push 2"] = (x - 2.5, ENC_Y + 7.0, 16.0), (x + 2.5, ENC_Y + 7.0, 16.0)
    for k, x in enumerate(FS_X):
        f = "abc"[k]
        P[f"f{f}.lug 1"], P[f"f{f}.lug 2"] = (x - 3.0, FS_Y, 32.0), (x + 3.0, FS_Y, 32.0)
    # Seed3 socket block in the right wall: wires leave the jumper ends' inner side. Places 1-20 the row nearer the
    # face (1 at the USB-C end, toward the footswitches), 21-40 the deeper row (40 at the USB-C end)
    for n in range(1, 41):
        y = -8.9 + ((n - 1) if n <= 20 else (40 - n)) * 2.54
        P[f"seed.{n}"] = (IN_X - 13.0, y, 16.8 if n <= 20 else 19.3)
    for i, name in enumerate(PINS["bay"]):
        P[f"bay.{name}"] = (-IN_X + 9.0, BAY_Y - 3.81 + 2.54 * i, 18.0)
    # PCF8574 board on the lid: input header at its left end, pass-through at the right, P0-P7 along its front edge
    cx, cy = PCF_C
    for i, name in enumerate(["VCC", "GND", "SDA", "SCL"]):
        P[f"pcf.{name}"] = (cx + 24.0, cy + 3.81 - 2.54 * i, PCF_H - 2.0)
        P[f"pcf.{name} "] = (cx - 24.0, cy + 3.81 - 2.54 * i, PCF_H - 2.0)
    for i in range(8):
        P[f"pcf.P{i}"] = (cx + 8.9 - 2.54 * i, cy - 9.0, PCF_H - 2.0)
    for i in range(4):   # the 4 resistors hang just in front of P4-P7
        x = cx + 8.9 - 2.54 * (4 + i)
        P[f"rr.a{i+1}"], P[f"rr.b{i+1}"] = (x, cy - 12.0, PCF_H - 3.0), (x, cy - 19.0, PCF_H - 3.0)
    return P


# ---------------------------------------------------------------- solids (prisms): (name, polygon XY, h0, h1, colour)
def circle(x, y, r, n=20):
    return [(x + r * math.cos(2 * math.pi * k / n), y + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def solids():
    S = []
    cx, cy = LCD_C
    S.append(("lcd", rect(cx - LCD_PCB[0] / 2, cy - LCD_PCB[1] / 2, cx + LCD_PCB[0] / 2, cy + LCD_PCB[1] / 2), 4.1, 9.6, "#2f4fa8"))
    for k, x in enumerate(ENC_X):
        S.append((f"e{k+1}", rect(x - 6, ENC_Y - 6, x + 6, ENC_Y + 6), FACE_T, 13.0, "#9aa0a6"))
    for k, x in enumerate(FS_X):
        S.append((f"f{'abc'[k]}", circle(x, FS_Y, 6.6), FACE_T, 30.0, "#555a60"))
    for pid, x in POT.items():
        S.append((pid, rect(x - 4.75, POT_Y - 4.75, x + 4.75, POT_Y + 4.75), FACE_T, 17.0, "#7d8fa3"))
    for sid, (x, y) in SW.items():
        S.append((sid, circle(x, y, 8.0), FACE_T, 13.0, "#e9e9e6"))
    for lid, x in LIGHT.items():
        S.append((lid, circle(x, LIGHT_Y, 2.85, 12), FACE_T, 8.0, "#c8c8c8"))
    S.append(("dc", rect(-IN_X, DC_Y - 6.0, -44.0, DC_Y + 6.0), 12.0, 24.0, "#2a2a2a"))
    S.append(("bay", rect(-IN_X, BAY_Y - 5.75, -IN_X + 8.0, BAY_Y + 5.75), 16.5, 19.5, "#2a2a2a"))
    S.append(("seed", rect(IN_X - 13.0, SEED_Y - 26.0, IN_X, SEED_Y + 26.0), 9.2, 27.0, "#1d1d1d"))
    for x in (-34.0, 34.0):
        S.append(("jack", rect(x - 9.1, 52.0, x + 9.1, IN_Y), 10.2, BOARD_H, "#202020"))
    S.append(("board", BOARD, BOARD_H, BOARD_H + 1.6, "#2e8b57"))
    S.append(("pcf", rect(PCF_C[0] - 26, PCF_C[1] - 10, PCF_C[0] + 26, PCF_C[1] + 8), PCF_H, PCF_H + 1.6, "#2456b5"))
    return S


# ---------------------------------------------------------------- the camera
YAW, ELEV = math.radians(-16.0), math.radians(56.0)


def proj(p):
    """(X, Y, h) -> (sx, sy_up, depth): seen from inside, so X is mirrored; higher h = nearer you"""
    X, Y, h = p
    u, w = -X, Y
    u1 = u * math.cos(YAW) - w * math.sin(YAW)
    w1 = u * math.sin(YAW) + w * math.cos(YAW)
    return u1, w1 * math.sin(ELEV) + h * math.cos(ELEV), w1 * math.cos(ELEV) - h * math.sin(ELEV)


def shade(hexc, f):
    r, g, b = (int(hexc[i:i + 2], 16) for i in (1, 3, 5))
    return "#%02x%02x%02x" % tuple(max(0, min(255, int(c * f))) for c in (r, g, b))


GROUPS = [   # page title, the parts that matter, which wires
    ("1. Where every part sits", None, lambda w: False),
    ("2. Power: 9 V jack -> power switch -> board -> Seed3", {"dc", "pw", "board", "seed"},
     lambda w: w[0].split(".")[0] in ("dc", "pw") or w[0] in ("j10.vin", "j10.gnd")),
    ("3. Board <-> Seed3, and the gain pots <-> the board", {"board", "seed", "pl", "ph"},
     lambda w: (w[0].startswith("j10.") and w[0] not in ("j10.vin", "j10.gnd")) or w[0].split(".")[0] in ("pl", "ph")),
    ("4. Screen -> Seed3", {"lcd", "seed"}, lambda w: w[0].startswith("lcd.") and w[0] != "lcd.2 GND"),
    ("5. Knobs and footswitches -> Seed3", {"e1", "e2", "e3", "e4", "fa", "fb", "fc", "seed"},
     lambda w: w[0].split(".")[0] in ("e1", "e2", "e3", "e4", "fa", "fb", "fc") and w[1].startswith("seed.")),
    ("6. The ground chain (black wire, part to part)", {"e1", "e2", "e3", "e4", "fa", "fb", "fc", "lcd", "seed"},
     lambda w: "ground chain" in w[3] or w[0] == "lcd.2 GND"),
    ("7. PCF8574 board (on the lid): page switch, level lights, board, bay", {"pcf", "ps", "li", "lo", "bay", "board"},
     lambda w: w[0].split(".")[0] in ("pcf", "ps", "li", "lo", "rr") or w[1].startswith("bay.")),
]


def words(e):
    pid, pin = e.split(".", 1)
    if pid == "seed":
        return f"Seed3 socket place {pin}"
    return f"{TITLE[pid]}: {pin.strip()}"


def page(n, title, focus, pick, P):
    W, H = 297.0, 210.0
    S = solids()
    # fit: project the box corners into the left 195 x 180 mm
    corners = [proj((sx * BOX_X, sy * BOX_Y, h)) for sx in (-1, 1) for sy in (-1, 1) for h in (0.0, BOX_D + 4)]
    minx, maxx = min(c[0] for c in corners), max(c[0] for c in corners)
    miny, maxy = min(c[1] for c in corners), max(c[1] for c in corners)
    sc = min(190.0 / (maxx - minx), 178.0 / (maxy - miny))
    ox, oy = 6.0 - minx * sc, 22.0 + maxy * sc
    xy = lambda p: (ox + proj(p)[0] * sc, oy - proj(p)[1] * sc)
    o = []
    T = lambda x, y, t, sz=2.4, a="start", c="#111", w="normal": o.append(
        f'<text x="{x:.2f}" y="{y:.2f}" font-family="DejaVu Sans, sans-serif" font-size="{sz}" text-anchor="{a}" '
        f'fill="{c}" font-weight="{w}" stroke="#fff" stroke-width="{sz*0.22:.2f}" paint-order="stroke">{html.escape(t)}</text>')
    poly = lambda pts, fill, op=1.0, stroke="#333", sw=0.2: o.append(
        '<polygon points="' + " ".join(f"{a:.2f},{b:.2f}" for a, b in pts) + f'" fill="{fill}" fill-opacity="{op}" '
        f'stroke="{stroke}" stroke-width="{sw}" stroke-linejoin="round"/>')
    T(8, 10, f"PG-1 wiring in 3D  -  {title}", 4.6, w="bold")
    T(8, 15.5, "Open box from above: lid off, face down on the table, the jacks' wall away from you. Seen from inside, "
      "so left and right are swapped: the in jack is on your RIGHT.", 2.5, c="#444")
    # the floor (the face's back) and the 3 far walls
    flo = [(-IN_X, -IN_Y), (IN_X, -IN_Y), (IN_X, IN_Y), (-IN_X, IN_Y)]
    poly([xy((x, y, FACE_T)) for x, y in flo], "#eceae4", 1.0, "#999", 0.3)
    for wall in ([(-IN_X, IN_Y), (IN_X, IN_Y)], [(-IN_X, -IN_Y), (-IN_X, IN_Y)], [(IN_X, -IN_Y), (IN_X, IN_Y)]):
        (x0, y0), (x1, y1) = wall
        poly([xy((x0, y0, FACE_T)), xy((x1, y1, FACE_T)), xy((x1, y1, BOX_D)), xy((x0, y0, BOX_D))], "#dcd9d0", 0.55, "#888", 0.3)
    # the Seed3 itself, outside the right wall
    poly([xy((BOX_X + 0.3, SEED_Y - 25.5, 9.0)), xy((BOX_X + 0.3, SEED_Y + 25.5, 9.0)), xy((BOX_X + 0.3, SEED_Y + 25.5, 27.0)),
          xy((BOX_X + 0.3, SEED_Y - 25.5, 27.0))], "#3a3a3a", 0.9, "#111", 0.3)
    # solids, far to near (painter's order by their centre's depth)
    faces = []
    for name, pg, h0, h1, col in S:
        dim = focus is not None and name not in focus and not (name == "jack" and "board" in (focus or ()))
        op = 0.22 if dim else (0.55 if name in ("board", "pcf") else 1.0)
        cxm = sum(p[0] for p in pg) / len(pg); cym = sum(p[1] for p in pg) / len(pg)
        d0 = proj((cxm, cym, (h0 + h1) / 2))[2]
        m = len(pg)
        side = []
        for i in range(m):
            a, b = pg[i], pg[(i + 1) % m]
            q = [xy((a[0], a[1], h0)), xy((b[0], b[1], h0)), xy((b[0], b[1], h1)), xy((a[0], a[1], h1))]
            area = sum(q[k][0] * q[(k + 1) % 4][1] - q[(k + 1) % 4][0] * q[k][1] for k in range(4))
            if area < 0:   # facing you
                side.append((q, shade(col, 0.72 + 0.18 * math.cos(i))))
        top = [xy((x, y, h1)) for x, y in pg]
        faces.append((d0, side, top, col, op))
    for d0, side, top, col, op in sorted(faces, key=lambda f: -f[0]):
        for q, c in side:
            poly(q, c, op, "#222", 0.15)
        poly(top, shade(col, 1.12), op, "#222", 0.2)
    # the near wall: just its outline, so nothing hides
    poly([xy((-IN_X, -IN_Y, FACE_T)), xy((IN_X, -IN_Y, FACE_T)), xy((IN_X, -IN_Y, BOX_D)), xy((-IN_X, -IN_Y, BOX_D))], "none", 1, "#888", 0.3)
    # part names (page 1: all of them; then the ones in focus)
    names = {"lcd": "screen (header on its right end)", "e1": "pg-1", "e2": "pg-2", "e3": "pg-3", "e4": "pg-4", "fa": "pg-a",
             "fb": "pg-b", "fc": "pg-c", "pl": "Input Gain pot", "ph": "Output Gain pot", "pw": "power switch",
             "ps": "page switch", "li": "in light", "lo": "out light", "dc": "9V jack (left wall)",
             "bay": "expansion bay (left wall)", "seed": "Seed3 socket (right wall)", "board": "carrier board",
             "pcf": "PCF8574 board (on the lid)"}
    for name, pg, h0, h1, col in S:
        if name not in names or (focus is not None and name not in focus):
            continue
        cxm = sum(p[0] for p in pg) / len(pg); cym = sum(p[1] for p in pg) / len(pg)
        x, y = xy((cxm, cym, h1 + 1.0))
        if focus is not None:   # under the part, clear of its pin names
            x, y = xy((cxm, cym, h0))
            y += 4.0
        T(x, y, names[name], 2.6 if focus is None else 2.2, "middle", "#000", "bold")
    if focus is None:
        T(8, 200, "Not drawn: the 4 encoder knobs, gain knobs, chicken heads and footswitch caps (they're outside, on the face).",
          2.4, c="#444")
    # wires: arching up toward you between their two ends; each end labelled with its pin, a list on the right
    ws = [w for w in WIRES if pick(w)]
    labels = []
    def free(x, y, wdt, hgt):
        return all(x + wdt < a or x > a + aw or y < b - bh or y - hgt > b for a, b, aw, bh in labels)
    for a, b, col, what in ws:
        pa, pb = P[a], P[b]
        dist = math.dist(pa, pb)
        lift = 4.0 + 0.10 * dist
        c0 = (pa[0], pa[1], pa[2] + lift); c1 = (pb[0], pb[1], pb[2] + lift)
        if what.startswith("join") and dist < 9:
            c0 = (pa[0], pa[1], pa[2] + 1.5); c1 = (pb[0], pb[1], pb[2] + 1.5)
        (x0, y0), (x1, y1), (x2, y2), (x3, y3) = xy(pa), xy(c0), xy(c1), xy(pb)
        d = f"M{x0:.2f},{y0:.2f} C{x1:.2f},{y1:.2f} {x2:.2f},{y2:.2f} {x3:.2f},{y3:.2f}"
        o.append(f'<path d="{d}" fill="none" stroke="#fff" stroke-width="1.15" stroke-linecap="round"/>')
        o.append(f'<path d="{d}" fill="none" stroke="{COL[col]}" stroke-width="0.6" stroke-linecap="round"/>')
    for a, b, col, what in ws:
        for e in (a, b):
            x, y = xy(P[e])
            o.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="0.55" fill="{COL[col]}" stroke="#fff" stroke-width="0.15"/>')
    # pin names at the ends (once per pin), pushed clear of each other with a leader
    done = set()
    for a, b, col, what in ws:
        for e in (a, b):
            if e in done:
                continue
            done.add(e)
            pid, pin = e.split(".", 1)
            lab = (f"{pin}" if pid == "seed" else pin.strip())
            x, y = xy(P[e])
            wdt, hgt = 0.95 * len(lab) + 0.6, 1.9
            for k in range(60):
                ang = math.radians(-35 + 47 * k)
                r = 2.2 + 0.35 * k
                tx, ty = x + r * math.cos(ang), y + r * math.sin(ang)
                if free(tx, ty, wdt, hgt):
                    break
            labels.append((tx, ty, wdt, hgt))
            o.append(f'<path d="M{x:.2f},{y:.2f} L{tx + (0 if tx > x else wdt):.2f},{ty - 0.6:.2f}" stroke="#777" stroke-width="0.12"/>')
            T(tx, ty, lab, 1.7, c="#000")
    # the list, in words
    if ws:
        lx, ly0 = 203.0, 26.0
        T(lx, ly0 - 4, "Each wire, from -> to:", 2.8, w="bold")
        yy = ly0
        for a, b, col, what in ws:
            o.append(f'<path d="M{lx},{yy - 0.8} h4" stroke="{COL[col]}" stroke-width="1.2"/>')
            T(lx + 5.5, yy, f"{words(a)}  ->  {words(b)}", 1.85)
            yy += 2.55
            if what and what not in ("jumper", "ground chain", "short wire, soldered on the pot") and not what.startswith("join"):
                T(lx + 5.5, yy, what[:95], 1.55, c="#666")
                yy += 2.3
    else:   # page 1: what's on which wall
        lx = 203.0
        for i, t in enumerate(["Top wall (away from you): the in / out jacks;", "the carrier board hangs from their nuts,",
                               "flat, 27 mm down from the face.", "", "Left wall (your right): 9V jack, expansion bay.",
                               "Right wall (your left): the Seed3 socket;", "the Seed3 plugs in from outside.", "",
                               "PCF8574 board: velcro on the lid, drawn", "where it sits when the lid is on.", "",
                               "Pages 2-7: each job's wires, with the pin", "names at both ends and a list in words.",
                               "Flat version with every lug: wiring-diagram.pdf"]):
            T(lx, 30 + i * 4.2, t, 2.5)
    # the Seed3 socket, in words, when it's on this page
    if focus and "seed" in focus and ws:
        used = {}
        for a, b, col, what in ws:
            for e, other in ((a, b), (b, a)):
                if e.startswith("seed."):
                    used[int(e.split(".")[1])] = (other, col)
        bx, by = 203.0, yy + 7
        T(bx, by - 3, "Seed3 socket places on this page (from inside):", 2.4, w="bold")
        for k, (pl, (other, col)) in enumerate(sorted(used.items())):
            T(bx + (k % 2) * 46, by + 2.6 * (k // 2) + 1, f"{pl}: {words(other).split(': ', 1)[-1] if ': ' in words(other) else words(other)} ({TITLE[other.split('.')[0]]})"[:44], 1.7, c=COL[col])
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">'
           f'<rect width="{W}" height="{H}" fill="#fff"/>' + "\n".join(o) + "</svg>\n")
    path = os.path.join(OUT, f"wiring-3d-{n}.svg")
    open(path, "w").write(svg)
    return path


def main():
    P = pin3d()
    missing = sorted({e for w in WIRES for e in w[:2]} - set(P))
    if missing:
        sys.exit(f"no 3D position for: {missing}")
    files = [page(i + 1, t, f, pick, P) for i, (t, f, pick) in enumerate(GROUPS)]
    drawn = {w for w in WIRES for g in GROUPS[1:] if g[2](w)}
    left = [w for w in WIRES if w not in drawn]
    if left:
        sys.exit(f"wires on no page: {left}")
    subprocess.run(["rsvg-convert", "-f", "pdf", "-o", os.path.join(OUT, "wiring-3d.pdf")] + files, check=True)
    print(f"wiring 3d: {len(files)} pages, every one of the {len(WIRES)} wires on a page")


if __name__ == "__main__":
    main()
