#!/usr/bin/env python3
"""PG-1 wiring pictures: one page per job, every part drawn the way it looks from where you solder it (from
inside the box, lid off), every wire running to the exact lug / pin it goes on, a numbered step list in words.

Same wire list as the flat drawing (pg_wiring.WIRES): this script refuses to finish if any wire is missing from the
pages or drawn twice. Part pin layouts: the A-8233 / RS16211 datasheet (lugs 1-8 round the rim, commons A / B in the
middle, seen from the back), the encoder / pot / jack / footswitch pin rows as they come out of the part's back.
-> 05-Wiring-and-Schematics/wiring-pictures.pdf (+ wiring-pic-N.svg)
"""
import html
import math
import os
import subprocess
import sys

sys.dont_write_bytecode = True   # (no __pycache__: it would carry this machine's paths into the export)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pg_wiring import WIRES, COL, OUT   # noqa: E402

W, H = 297.0, 210.0
SOLDER = "#c9ccd1"


class Page:
    def __init__(self, title, note):
        self.o, self.pins, self.desc, self.title, self.note = [], {}, {}, title, note
        self.only = None   # optional: which wires belong on this page

    def T(self, x, y, t, sz=2.6, a="start", c="#111", w="normal", rot=0):
        tr = f' transform="rotate({rot} {x:.2f} {y:.2f})"' if rot else ""
        light = c.lower() in ("#fff", "#ffffff", "#ccc", "#cde", "#cccccc", "#888")
        halo = "#000" if light else "#fff"   # light text on a dark part: a dark outline, not a white blob
        self.o.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="DejaVu Sans, sans-serif" font-size="{sz}" '
                      f'text-anchor="{a}" fill="{c}" font-weight="{w}" stroke="{halo}" stroke-opacity="{0.5 if light else 1}" stroke-width="{sz*0.18 if light else sz*0.25:.2f}" '
                      f'paint-order="stroke"{tr}>{html.escape(t)}</text>')

    def add(self, s):
        self.o.append(s)

    def pin(self, key, x, y, dx, dy, desc):
        """a wire end: where, which way a wire leaves it, and in words which pin it is"""
        self.pins[key] = (x, y, dx, dy)
        self.desc[key] = desc


def rect(p, x, y, w, h, fill, stroke="#333", rx=1.0, sw=0.35, extra=""):
    p.add(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{extra}/>')


def circ(p, x, y, r, fill, stroke="#333", sw=0.35):
    p.add(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r:.2f}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')


def lug(p, x, y, w=1.6, h=3.2, ang=0):
    """a tinned solder lug (a small flat tab with a hole)"""
    p.add(f'<g transform="rotate({ang} {x:.2f} {y:.2f})"><rect x="{x-w/2:.2f}" y="{y-h/2:.2f}" width="{w}" height="{h}" rx="0.3" '
          f'fill="{SOLDER}" stroke="#666" stroke-width="0.25"/><circle cx="{x:.2f}" cy="{y-h*0.15:.2f}" r="{w*0.22:.2f}" fill="#777"/></g>')


# ---------------------------------------------------------------- parts, as seen from where you solder
def encoder(p, pid, name, x, y, s=3.2):
    """A-6331 from the back: the 3-pin row (B C A from the back) at the bottom, the 2 push pins at the top"""
    b = 12 * s
    rect(p, x - b / 2, y - b / 2, b, b, "#b9bec5", "#555", 2.0)
    rect(p, x - b / 2 + 2, y - b / 2 + 2, b - 4, b - 4, "#30343a", "#222", 1.5)
    p.T(x, y + 1, name, 3.2, "middle", "#fff", "bold")
    p.T(x, y + 5, "back of the encoder", 1.9, "middle", "#ccc")
    for k, (pn, d) in enumerate([("B", "LEFT pin of the row of 3"), ("C", "MIDDLE pin of the row of 3"), ("A", "RIGHT pin of the row of 3")]):
        px = x + (k - 1) * 2.5 * s
        lug(p, px, y + b / 2 + 2.2, 1.4, 4.4)
        p.T(px, y + b / 2 + 7.6, pn, 2.4, "middle", "#000", "bold")
        p.pin(f"{pid}.{pn}", px, y + b / 2 + 4.2, 0, 1, f"{name}: {pn} ({d})")
    for k, (pn, d) in enumerate([("push", "LEFT pin of the 2"), ("push 2", "RIGHT pin of the 2")]):
        px = x + (k - 0.5) * 5.0 * s
        lug(p, px, y - b / 2 - 2.2, 1.4, 4.4)
        p.T(px, y - b / 2 - 5.4, pn, 2.2, "middle", "#000", "bold")
        p.pin(f"{pid}.{pn}", px, y - b / 2 - 4.2, 0, -1, f"{name}: {pn} ({d})")


def footswitch(p, pid, name, x, y, s=2.6):
    circ(p, x, y, 6.5 * s, "#4a4f55", "#222")
    circ(p, x, y, 4.6 * s, "#2b2e33", "#111")
    p.T(x, y + 1, name, 3.0, "middle", "#fff", "bold")
    p.T(x, y + 4.6, "back of the footswitch", 1.8, "middle", "#ccc")
    for k, (pn, d) in enumerate([("lug 1", "either lug"), ("lug 2", "the other lug")]):
        px = x + (k - 0.5) * 4.0 * s
        lug(p, px, y - 4.8 * s, 2.0, 4.6)
        p.pin(f"{pid}.{pn}", px, y - 4.8 * s - 2.0, 0, -1, f"{name}: {d}")


def pot(p, pid, name, x, y, s=3.4):
    """A-8618 dual pot from the back, pins pointing down: 2 rows of 3 (the row nearer the panel = gang l)"""
    b = 9.5 * s
    rect(p, x - b / 2, y - b / 2, b, b, "#9aa9bb", "#445", 1.2)
    rect(p, x - b / 2 + 1.5, y - b / 2 + 1.5, b - 3, b * 0.45, "#7d8ea3", "#445", 0.8)
    p.T(x, y - 1, name, 2.7, "middle", "#000", "bold")
    p.T(x, y + 3, "back of the pot", 1.8, "middle", "#223")
    for row, (g, dy, rname) in enumerate([("l", 3.0, "the row nearer the PANEL"), ("r", 8.5, "the row nearer YOU")]):
        for k, n in enumerate(["3", "2", "1"]):
            px = x + (k - 1) * 2.5 * s + (0.8 if row else 0)
            py = y + b / 2 + dy
            lug(p, px, py, 1.3, 3.6)
            pos = ["LEFT", "MIDDLE", "RIGHT"][k]
            p.pin(f"{pid}.{n} {g}", px, py + 1.8, 0, 1, f"{name}: pin {n} {g} ({pos} pin of {rname})")
        p.T(x + b / 2 + 2, y + b / 2 + dy + 1, f"gang {g}: 3 2 1", 1.9, "start", "#334")


def rswitch(p, pid, name, x, y, s=3.2, used=None, turn=0):
    """A-8233 (RS16211) from the back: lugs 1-8 round the rim (numbers moulded on it), commons A / B in the middle"""
    r = 8.0 * s
    circ(p, x, y, r + 1.0, "#f1f1ee", "#555", 0.5)
    circ(p, x, y, 2.75 * s, "#dcdcd6", "#777", 0.3)
    p.T(x, y + r + 5.5, name, 3.0, "middle", "#000", "bold")
    p.T(x, y + r + 9.0, "back of the switch (its lugs toward you; numbers moulded on it)" + (", drawn turned: it can sit at any angle" if turn else ""), 1.8, "middle", "#555")
    ang = {k: v + turn for k, v in {"1": -68, "2": -22, "3": 22, "4": 68, "5": 112, "6": 158, "7": 202, "8": 248}.items()}
    names = {"A1": "1", "A2": "2", "A3": "3", "A4": "4", "B1": "5", "B2": "6", "B3": "7", "B4": "8"}
    for full, n in names.items():
        a = math.radians(ang[n])
        px, py = x + r * math.cos(a), y - r * math.sin(a)
        lug(p, px, py, 1.5, 3.0, -ang[n] + 90)
        tx, ty = x + (r + 4.0) * math.cos(a), y - (r + 4.0) * math.sin(a) + 1.0
        p.T(tx, ty, n, 2.6, "middle", "#000" if not used or f"{pid}.{full}" in used else "#aaa", "bold")
        pole = "pole A" if full[0] == "A" else "pole B"
        p.pin(f"{pid}.{full}", px, py, math.cos(a), -math.sin(a), f"{name}: lug {n} ({pole}, position {full[1]})")
    for full, base, txt in (("A com", 0, "A"), ("B com", 180, "B")):
        a = math.radians(base + turn)
        px, py = x + 2.6 * s * math.cos(a), y - 2.6 * s * math.sin(a)
        lug(p, px, py, 1.5, 3.0, -(base + turn) + 90)
        p.T(px + 3.4 * math.cos(a), py - 3.4 * math.sin(a) + 1.0, txt, 2.6, "middle", "#000", "bold")
        p.pin(f"{pid}.{full}", px, py, math.cos(a), -math.sin(a), f"{name}: common {txt} (the inner lug marked {txt})")


def dcjack(p, pid, x, y, s=3.0):
    circ(p, x, y, 6.0 * s, "#2c2c2c", "#111")
    circ(p, x, y, 3.0 * s, "#444", "#111")
    p.T(x, y + 6.0 * s + 5.0, "9V DC jack (back)", 3.0, "middle", "#000", "bold")
    for pn, (dx, dy, d) in {"- centre": (0, -1, "the CENTRE-PIN lug (its - : centre negative)"),
                            "+ sleeve": (1, 0.35, "the SLEEVE lug (+)"),
                            "switch": (-1, 0.35, "the switch lug: leave empty")}.items():
        px, py = x + dx * 4.6 * s, y + dy * 4.6 * s
        lug(p, px, py, 1.8, 4.2, 90 if dx else 0)
        p.T(px + dx * 7, py + (-6 if dy < 0 else 1), pn.split()[-1] if pn != "switch" else "switch (empty)", 2.3,
            "middle", "#000", "bold")
        p.pin(f"{pid}.{pn}", px + dx * 2.0, py + (-2.0 if dy < 0 else 0), dx, dy if dx == 0 else 0, f"9V jack: {d}")


def header(p, pid, title, names, x, y, s=2.2, flip=False):
    """a carrier-board header: a strip of green board edge with right-angle pins and the names printed beside them
    (flip: drawn with the pins pointing up, the board below them)"""
    pitch = 2.54 * s
    w = pitch * len(names) + 6
    by = y if flip else y - 9
    rect(p, x - 3, by, w, 9, "#2e8b57", "#1d5c39", 0.8)
    p.T(x - 3 + w / 2, by + (7.6 if flip else 3.5), f"carrier board: {title}", 2.0, "middle", "#fff", "bold")
    for i, n in enumerate(names):
        px = x + pitch * (i + 0.5)
        py0 = y - 6 if flip else y - 1
        p.add(f'<rect x="{px-0.5:.2f}" y="{py0:.2f}" width="1.0" height="7" fill="#d4a017" stroke="#8a6a00" stroke-width="0.2"/>')
        p.T(px, by + (3.4 if flip else 7.6), n, 1.7, "middle", "#fff", "bold")
        p.pin(f"{pid}.{n}", px, y - 6.0 if flip else y + 6.0, 0, -1 if flip else 1,
              f"board '{title}' header: pin '{n}' ({ordinal(i, len(names))})")


def ordinal(i, n):
    if i == 0:
        return "1st from the left"
    if i == n - 1:
        return "last on the right"
    return f"{i+1}{'nd' if i == 1 else 'rd' if i == 2 else 'th'} from the left"


def seed(p, x, y, labels, s=1.75):
    """the Seed3 socket block from inside the box, USB-C end on your left: places 1-20 top row, 40-21 bottom row"""
    pitch = 2.54 * s
    rect(p, x - 3, y - 10, pitch * 20 + 6, pitch * 2 + 14, "#1d1d1d", "#000", 1.5)
    p.T(x + pitch * 10, y - 5.2, "Seed3 socket (jumper ends in the right wall), seen from INSIDE, USB-C end on your LEFT", 2.0,
        "middle", "#fff", "bold")
    for i in range(20):
        for row, n in ((0, i + 1), (1, 40 - i)):
            px, py = x + pitch * (i + 0.5), y + row * pitch + pitch / 2
            used = f"seed.{n}" in labels
            rect(p, px - pitch / 2 + 0.3, py - pitch / 2 + 0.3, pitch - 0.6, pitch - 0.6, "#3a3a3a" if not used else "#111",
                 "#d4a017" if used else "#555", 0.3, 0.3 if not used else 0.5)
            p.T(px, py + 0.7, str(n), 1.6, "middle", "#fff" if used else "#888", "bold")
            if used:
                p.pin(f"seed.{n}", px, py, 0, -1,   # the jumper sticks out of its place toward you: leave upward
                      f"Seed3 socket place {n} ({'top' if row == 0 else 'bottom'} row)")
    p.T(x - 4, y + pitch, "USB-C", 1.9, "end", "#555")


def screen(p, x, y, s=2.0):
    """the screen's 14-pin header, as you see it from inside (the names are printed on the module next to the pins)"""
    names = ["1 VCC", "2 GND", "3 CS", "4 RST", "5 DC", "6 SDI", "7 SCK", "8 LED", "9 SDO", "10 TCLK", "11 TCS", "12 TDIN", "13 TDO", "14 TIRQ"]
    printed = ["VCC", "GND", "CS", "RESET", "DC", "SDI(MOSI)", "SCK", "LED", "SDO(MISO)", "T_CLK", "T_CS", "T_DIN", "T_DO", "T_IRQ"]
    pitch = 2.54 * s
    rect(p, x - 40, y - 4, 44, pitch * 14 + 8, "#1f4fa0", "#123", 1.5)
    p.T(x - 20, y + pitch * 7, "screen module", 3.0, "middle", "#fff", "bold", -90)
    p.T(x - 14, y + pitch * 7, "(back)", 2.0, "middle", "#cde", "normal", -90)
    for i, (n, pr) in enumerate(zip(names, printed)):
        py = y + pitch * (i + 0.5)
        p.add(f'<rect x="{x-1:.2f}" y="{py-0.5:.2f}" width="7" height="1.0" fill="#d4a017" stroke="#8a6a00" stroke-width="0.2"/>')
        p.T(x - 2, py + 0.7, pr, 1.8, "end", "#fff", "bold")
        p.pin(f"lcd.{n}", x + 6.0, py, 1, 0, f"screen: pin {n.split()[0]} '{pr}' (printed on the module)")


def pcf(p, x, y, s=2.2):
    """Comimark PCF8574 board: input pins at the left end, the P0-P7 row along the bottom, pass-through at the right"""
    w, h = 54 * s * 0.55, 20 * s * 0.55
    rect(p, x, y, w, h + 6, "#1e4fb8", "#0d2a66", 1.5)
    rect(p, x + w * 0.42, y + 6, 10, 9, "#111", "#000", 0.5)
    p.T(x + w * 0.42 + 5, y + 4.5, "PCF8574", 2.0, "middle", "#fff", "bold")
    p.T(x + w / 2, y - 2.0, "PCF8574 board (Amazon), parts side up", 2.0, "middle", "#333")
    pitch = 2.54 * 1.6
    for i, n in enumerate(["VCC", "GND", "SDA", "SCL"]):
        py = y + 4 + pitch * (i + 0.5)
        p.add(f'<rect x="{x-6:.2f}" y="{py-0.5:.2f}" width="7" height="1.0" fill="#d4a017"/>')
        p.T(x + 2, py + 0.7, n, 1.8, "start", "#fff", "bold")
        p.pin(f"pcf.{n}", x - 6, py, -1, 0, f"PCF8574: '{n}' on the INPUT pins (left end)")
        py2 = py
        p.add(f'<rect x="{x+w-1:.2f}" y="{py2-0.8:.2f}" width="5" height="1.6" fill="#111"/>')
        p.T(x + w - 2, py2 + 0.7, n, 1.8, "end", "#fff", "bold")
        p.pin(f"pcf.{n} ", x + w + 4, py2, 1, 0, f"PCF8574: '{n}' on the PASS-THROUGH pins (right end)")
    for i in range(8):
        px = x + w * 0.18 + pitch * i
        p.add(f'<rect x="{px-0.5:.2f}" y="{y+h+5:.2f}" width="1.0" height="7" fill="#d4a017"/>')
        p.T(px, y + h + 4.2, f"P{i}", 1.7, "middle", "#fff", "bold")
        p.pin(f"pcf.P{i}", px, y + h + 12, 0, 1, f"PCF8574: 'P{i}' on the row of 8")
    p.T(x + w * 0.18 + pitch * 8.2, y + h + 9, "(INT: empty)", 1.7, "start", "#555")


def bay(p, x, y, s=2.4):
    rect(p, x - 2, y - 2, 4 * 2.54 * s + 4, 12, "#2a2a2a", "#000", 1.0)
    p.T(x + 2 * 2.54 * s, y + 14, "expansion bay (4 jumper ends glued in the left wall)", 2.0, "middle", "#000", "bold")
    for i, n in enumerate(["3v3", "gnd", "scl", "sda"]):
        px = x + 2.54 * s * (i + 0.5)
        rect(p, px - 2, y + 1, 4, 7, "#111", "#d4a017", 0.3, 0.3)
        p.T(px, y + 5.5, n, 1.7, "middle", "#fff", "bold")
        p.pin(f"bay.{n}", px, y - 2, 0, -1, f"expansion bay: the '{n}' place")


def light(p, pid, name, x, y, s=3.2):
    circ(p, x, y, 3.4 * s, "#d8d8d8", "#888")
    circ(p, x, y, 1.6 * s, "#e8f6ec", "#9a9", 0.3)
    p.T(x, y + 3.4 * s + 4, name, 2.6, "middle", "#000", "bold")
    p.T(x, y + 3.4 * s + 7.2, "back of the bezel: the LED's 2 legs", 1.8, "middle", "#555")
    for k, pn in enumerate(["leg 1", "leg 2"]):
        px = x + (k - 0.5) * 2.54 * s
        ln = 14 + 3 * k
        p.add(f'<rect x="{px-0.4:.2f}" y="{y-ln:.2f}" width="0.8" height="{ln}" fill="{SOLDER}" stroke="#777" stroke-width="0.2"/>')
        p.pin(f"{pid}.{pn}", px, y - ln, 0, -1, f"{name}: {'the LEFT leg' if k == 0 else 'the RIGHT leg'} (either way round)")


def resistor(p, pid, k, x, y):
    p.add(f'<path d="M{x:.2f},{y-9:.2f} V{y+9:.2f}" stroke="{SOLDER}" stroke-width="0.7"/>')
    rect(p, x - 1.6, y - 4.5, 3.2, 9, "#e9d7b4", "#8a7350", 1.2, 0.3)
    for j, c in enumerate(["#f08c00", "#f08c00", "#7a4a1a", "#c9a227"]):   # 330 ohm: orange orange brown gold
        p.add(f'<rect x="{x-1.6:.2f}" y="{y-3.2+j*1.8:.2f}" width="3.2" height="0.8" fill="{c}"/>')
    p.pin(f"rr.a{k}", x, y - 9, 0, -1, f"330 ohm resistor {k}: the TOP leg")
    p.pin(f"rr.b{k}", x, y + 9, 0, 1, f"330 ohm resistor {k}: the BOTTOM leg")


# ---------------------------------------------------------------- the pages
def pages():
    P = []

    def new(title, note):
        p = Page(title, note); P.append(p); return p

    p = new("Power: 9V jack -> power switch -> carrier board -> Seed3",
            "Black = -, red = +. The power switch: + goes into BOTH commons (A and B); lugs 2 3 4 and 6 7 8 are all ON, so "
            "join them with bare offcuts and take ONE wire from them to 'dc +'. Lugs 1 and 5 = OFF: leave them empty.")
    dcjack(p, "dc", 40, 70)
    rswitch(p, "pw", "power switch", 108, 72, used={"pw.A com", "pw.B com", "pw.A2", "pw.A3", "pw.A4", "pw.B2", "pw.B3", "pw.B4"})
    header(p, "j10", "seed3 + 9v", ["dc +", "dc -", "vin", "gnd", "scl", "sda", "sck", "fs", "tx", "rx"], 30, 128)
    seed(p, 92, 170, {"seed.39", "seed.40"})

    p = new("Carrier board -> Seed3: the 6 signal jumpers",
            "Plug the jumpers onto the board's header BEFORE the board goes in the box (the names are printed on the "
            "board's face side). The other ends go in the Seed3 socket places shown.")
    header(p, "j10", "seed3 + 9v", ["dc +", "dc -", "vin", "gnd", "scl", "sda", "sck", "fs", "tx", "rx"], 40, 60)
    seed(p, 70, 150, {f"seed.{n}" for n in (12, 13, 32, 33, 34, 35)})

    p = new("The gain pots -> their headers on the carrier board",
            "Each pot has 2 rows of 3 pins: the row nearer the PANEL is gang 'l', the row nearer YOU is gang 'r'. Short "
            "wires soldered on the pot, female jumper ends on the header (header names are printed on the board).")
    pot(p, "pl", "Input Gain pot", 48, 52)
    header(p, "j20", "in gain knob", ["3 l", "2 l", "1 l", "3 r", "2 r", "1 r"], 26, 140, flip=True)
    pot(p, "ph", "Output Gain pot", 148, 52)
    header(p, "j19", "out gain knob", ["3 l", "2 l", "1 l", "3 r", "2 r", "1 r"], 126, 140, flip=True)

    p = new("Screen -> Seed3",
            "Read the names printed beside the screen's pins. 'SDO(MISO)' and 'T_IRQ' stay empty. 'GND' is on the "
            "ground-chain page.")
    screen(p, 60, 40)
    seed(p, 92, 160, {f"seed.{n}" for n in (38, 8, 1, 37, 11, 9, 23, 29, 30, 31, 36)})

    p = new("The 4 encoders -> Seed3",
            "Back of each encoder: the row of 3 pins at the bottom, the 2 push pins at the top. C and 'push 2' are on the "
            "ground-chain page. If a knob turns the wrong way, flip 'knobs' in the config tab (no rewiring).")
    for k, x in enumerate([30, 78, 126, 174]):
        encoder(p, f"e{k+1}", f"pg-{k+1}", x, 50)
    p.only = lambda w: w[1].startswith("seed.")
    seed(p, 80, 165, {f"seed.{n}" for n in (2, 3, 4, 5, 6, 7, 14, 15, 22, 26, 27, 28)})

    p = new("The 3 footswitches -> Seed3",
            "Either lug of each footswitch to its Seed3 place; the other lug is on the ground-chain page.")
    for k, x in enumerate([40, 100, 160]):
        footswitch(p, f"f{'abc'[k]}", f"pg-{'abc'[k]}", x, 60)
    p.only = lambda w: w[1].startswith("seed.")
    seed(p, 70, 160, {"seed.24", "seed.25", "seed.10"})

    p = new("The ground chain (one black wire, part to part)",
            "Solder a short black wire from each lug to the next, in the order of the steps. The last one (from the screen's "
            "GND) is SPLICED onto the jumper from the board's 'gnd' to Seed3 place 40, about 3 cm from the socket: strip 5 mm "
            "of that jumper, wrap, solder, heat shrink (a socket place holds only one jumper end).")
    for k, x in enumerate([24, 66, 108, 150]):
        encoder(p, f"e{k+1}", f"pg-{k+1}", x, 48, 2.6)
    for k, x in enumerate([28, 76, 124]):
        footswitch(p, f"f{'abc'[k]}", f"pg-{'abc'[k]}", x, 118, 2.2)
    screen(p, 192, 112, 1.25)
    seed(p, 40, 182, {"seed.40"}, 1.5)

    p = new("PCF8574 board -> carrier board and the expansion bay",
            "The 4 input pins go to the board's 'expansion' header; its 4 pass-through pins go to the bay in the left wall. "
            "The PCF8574 board is stuck to the inside of the lid with velcro.")
    pcf(p, 70, 40)
    header(p, "j21", "expansion", ["3v3", "gnd", "scl", "sda"], 22, 140, flip=True)
    bay(p, 140, 140)

    p = new("Page switch -> PCF8574 board",
            "Pole A only (lugs 1 2 3 4 and common A); pole B (5 6 7 8, B) stays empty. Common A goes to GND: the GND pin "
            "already has the carrier's jumper on it, so solder this wire on the PCF board's UNDERSIDE, on GND's joint.")
    rswitch(p, "ps", "page switch", 95, 140, used={"ps.A com", "ps.A1", "ps.A2", "ps.A3", "ps.A4"}, turn=90)
    pcf(p, 55, 35)

    p = new("Level lights + 330 ohm resistors -> PCF8574 board",
            "Each light's 2 legs go to 2 P pins (either way round: if a light shows red with signal, swap its 2 wires). "
            "Each P4-P7 also gets a 330 ohm to VCC, soldered on the PCF board's UNDERSIDE; the 4 resistors' other ends are "
            "joined and go to a VCC pin.")
    pcf(p, 55, 35)
    light(p, "li", "in light (under Input Gain)", 35, 150)
    light(p, "lo", "out light (under Output Gain)", 95, 150)
    for k in range(4):
        resistor(p, "rr", k + 1, 145 + 9 * k, 120)
    return P


def bez(a, b, bend=14.0):
    (x0, y0, dx0, dy0), (x1, y1, dx1, dy1) = a, b
    d = max(bend, 0.3 * math.dist((x0, y0), (x1, y1)))
    return (f"M{x0:.2f},{y0:.2f} C{x0+dx0*d:.2f},{y0+dy0*d:.2f} {x1+dx1*d:.2f},{y1+dy1*d:.2f} {x1:.2f},{y1:.2f}",
            (x0 + 3 * (x0 + dx0 * d) + 3 * (x1 + dx1 * d) + x1) / 8, (y0 + 3 * (y0 + dy0 * d) + 3 * (y1 + dy1 * d) + y1) / 8)


def render(P):
    files, placed = [], {}
    for n, p in enumerate(P, 1):
        ws = [w for w in WIRES if w[0] in p.pins and w[1] in p.pins and w not in placed and (p.only is None or p.only(w))]
        for w in ws:
            placed[w] = n
        body, tags = [], []
        steps = []
        for k, (a, b, col, what) in enumerate(ws, 1):
            d, mx, my = bez(p.pins[a], p.pins[b], 6.0 if what.startswith("join") else 14.0)
            c = COL[col]
            body.append(f'<path d="{d}" fill="none" stroke="#000" stroke-opacity="0.35" stroke-width="1.7" stroke-linecap="round"/>')
            body.append(f'<path d="{d}" fill="none" stroke="{c}" stroke-width="1.2" stroke-linecap="round"/>')
            for e in (a, b):
                x, y = p.pins[e][:2]
                if e.startswith("seed."):   # the socket place's number stays readable on top of the wire's end
                    t = e.split(".")[1]
                    tags.append(f'<rect x="{x-2.0:.2f}" y="{y-1.5:.2f}" width="4.0" height="3.0" rx="0.6" fill="#fff" stroke="{c}" stroke-width="0.5"/>'
                                f'<text x="{x:.2f}" y="{y+0.75:.2f}" font-family="DejaVu Sans" font-size="2.1" text-anchor="middle" font-weight="bold" fill="#000">{t}</text>')
                else:
                    body.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="0.9" fill="{SOLDER}" stroke="#555" stroke-width="0.2"/>')
            body.append(f'<circle cx="{mx:.2f}" cy="{my:.2f}" r="2.1" fill="#fff" stroke="{c}" stroke-width="0.5"/>'
                        f'<text x="{mx:.2f}" y="{my+0.8:.2f}" font-family="DejaVu Sans" font-size="2.2" text-anchor="middle" '
                        f'font-weight="bold" fill="{c}">{k}</text>')
            how = "" if what in ("jumper", "ground chain", "short wire, soldered on the pot") else f"  ({what})"
            steps.append((k, c, f"{p.desc[a]}  ->  {p.desc[b]}", how))
        p.o = p.o + body + tags
        p.T(8, 9, f"PG-1 wiring, page {n} of {len(P)}: {p.title}", 4.4, w="bold")
        # the note, wrapped
        line, yy = "", 15.0
        for word in p.note.split():
            if len(line) + len(word) > 150:
                p.T(8, yy, line, 2.3, c="#333"); yy += 3.2; line = ""
            line += word + " "
        p.T(8, yy, line, 2.3, c="#333")
        # the steps, wrapped, on the right
        sx, sy = 206.0, 30.0
        p.T(sx, sy - 4, "Steps (the number is on the wire):", 2.7, w="bold")
        for k, c, t, how in steps:
            first = True
            txt = t + how
            while txt:
                cut = txt[:58]
                if len(txt) > 58 and " " in cut:
                    cut = cut[:cut.rfind(" ")]
                if first:
                    p.add(f'<circle cx="{sx+1.6:.2f}" cy="{sy-0.8:.2f}" r="1.6" fill="{c}"/>'
                          f'<text x="{sx+1.6:.2f}" y="{sy:.2f}" font-family="DejaVu Sans" font-size="1.9" text-anchor="middle" '
                          f'font-weight="bold" fill="#fff">{k}</text>')
                p.T(sx + 4.2, sy, cut, 1.75, c="#111" if first else "#444")
                txt = txt[len(cut):].lstrip()
                sy += 2.5
                first = False
            sy += 0.9
        p.add(f'<path d="M202,24 V{H-8}" stroke="#ccc" stroke-width="0.3"/>')
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">'
               f'<rect width="{W}" height="{H}" fill="#fbfaf7"/>' + "\n".join(p.o) + "</svg>\n")
        path = os.path.join(OUT, f"wiring-pic-{n}.svg")
        open(path, "w").write(svg)
        files.append(path)
    missing = [w for w in WIRES if w not in placed]
    if missing:
        sys.exit(f"wires on no page: {missing}")
    subprocess.run(["rsvg-convert", "-f", "pdf", "-o", os.path.join(OUT, "wiring-pictures.pdf")] + files, check=True)
    print(f"wiring pictures: {len(files)} pages, all {len(WIRES)} wires drawn once")


if __name__ == "__main__":
    render(pages())
