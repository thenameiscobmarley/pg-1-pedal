#!/usr/bin/env python3
"""PG-1 wiring pictures: one page per job, every part drawn the way it looks from where you solder it, every wire
routed neatly (straight out of its pin, along its own lane, round the parts, never over them) to the exact lug / pin
it goes on, and numbered steps in words.

Same wire list as the flat drawing (pg_wiring.WIRES): this script refuses to finish if any wire is missing or drawn
twice. Lug layouts: the A-8233 / RS16211 datasheet (lugs 1-8 round the rim, commons A / B in the middle, from the
back), the encoder's row of 3 + row of 2, the dual pot's 2 rows of 3.
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

DEFS = """<defs>
<filter id="sh" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur in="SourceAlpha" stdDeviation="0.9"/>
<feOffset dx="0.8" dy="1.1"/><feComponentTransfer><feFuncA type="linear" slope="0.35"/></feComponentTransfer>
<feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge></filter>
<linearGradient id="metal" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#eef0f2"/><stop offset="0.45" stop-color="#b8bcc2"/><stop offset="1" stop-color="#8a8f96"/></linearGradient>
<linearGradient id="dark" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3a3d42"/><stop offset="1" stop-color="#1c1e21"/></linearGradient>
<linearGradient id="ivory" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#fbf8ee"/><stop offset="1" stop-color="#ddd6c2"/></linearGradient>
<linearGradient id="pcbg" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2f9a5e"/><stop offset="1" stop-color="#1f6e42"/></linearGradient>
<linearGradient id="pcbb" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2d63c8"/><stop offset="1" stop-color="#1a3f8a"/></linearGradient>
<linearGradient id="gold" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#f3d27a"/><stop offset="1" stop-color="#b98a1c"/></linearGradient>
<linearGradient id="potc" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#c9d3df"/><stop offset="1" stop-color="#7f8ea2"/></linearGradient>
<radialGradient id="chrome" cx="0.35" cy="0.35" r="0.7"><stop offset="0" stop-color="#ffffff"/><stop offset="0.5" stop-color="#c3c7cc"/><stop offset="1" stop-color="#7d838b"/></radialGradient>
</defs>"""


class Page:
    def __init__(self, title, note, lanes):
        self.o, self.pins, self.desc, self.title, self.note = [], {}, {}, title, note
        self.lanes = lanes            # (first lane y, step): every routed wire gets its own lane
        self.only = None              # optional: which wires belong on this page
        self.geo = {}                 # rotary switches: pid -> (x, y, rim radius, turn)

    def T(self, x, y, t, sz=2.4, a="start", c="#111", w="normal", rot=0, halo=True):
        tr = f' transform="rotate({rot} {x:.2f} {y:.2f})"' if rot else ""
        hl = (f' stroke="#fff" stroke-width="{sz*0.22:.2f}" paint-order="stroke"' if halo else "")
        self.o.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="DejaVu Sans, sans-serif" font-size="{sz}" '
                      f'text-anchor="{a}" fill="{c}" font-weight="{w}"{hl}{tr}>{html.escape(t)}</text>')

    def add(self, s):
        self.o.append(s)

    def pin(self, key, x, y, dx, dy, box, desc, kind="solder"):
        """a wire end: where it is, which way a wire leaves it, the part's outline (wires go round it), in words"""
        self.pins[key] = (x, y, dx, dy, box, kind)
        self.desc[key] = desc


def R(p, x, y, w, h, fill, stroke="#222", rx=1.0, sw=0.3, sh=False):
    p.add(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{rx}" fill="{fill}" stroke="{stroke}" '
          f'stroke-width="{sw}"{" filter=\"url(#sh)\"" if sh else ""}/>')


def C(p, x, y, r, fill, stroke="#222", sw=0.3, sh=False):
    p.add(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r:.2f}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"'
          f'{" filter=\"url(#sh)\"" if sh else ""}/>')


def leg(p, x0, y0, x1, y1, w=0.9):
    """a tinned leg / lug from (x0,y0) to (x1,y1)"""
    p.add(f'<path d="M{x0:.2f},{y0:.2f} L{x1:.2f},{y1:.2f}" stroke="#6d7177" stroke-width="{w+0.35}" stroke-linecap="round"/>'
          f'<path d="M{x0:.2f},{y0:.2f} L{x1:.2f},{y1:.2f}" stroke="#d9dce0" stroke-width="{w}" stroke-linecap="round"/>')


# ---------------------------------------------------------------- the parts, seen from where you solder
def encoder(p, pid, name, x, y, s=2.4):
    b = 12.0 * s
    box = (x - b / 2 - 3, y - b / 2 - 6, x + b / 2 + 3, y + b / 2 + 6)
    for sx in (-1, 1):   # the mounting tabs
        R(p, x + sx * (b / 2 + 1.2) - 1.2, y - 3, 2.4, 6, "url(#metal)", "#666", 0.4)
    R(p, x - b / 2, y - b / 2, b, b, "url(#metal)", "#555", 1.6, 0.35, True)
    R(p, x - b / 2 + 1.6, y - b / 2 + 1.6, b - 3.2, b - 3.2, "url(#dark)", "#111", 1.2)
    R(p, x - 7.5, y - 3.2, 15, 6.4, "#f4f1e8", "#999", 0.8, 0.2)
    p.T(x, y + 1.2, name, 3.0, "middle", "#111", "bold", halo=False)
    for k, (pn, d) in enumerate([("B", "LEFT pin of the row of 3"), ("C", "MIDDLE pin of the row of 3"), ("A", "RIGHT pin of the row of 3")]):
        px = x + (k - 1) * 2.5 * s
        leg(p, px, y + b / 2 - 1, px, y + b / 2 + 4.5)
        p.T(px, y + b / 2 + 8.4, pn, 2.4, "middle", "#000", "bold")
        p.pin(f"{pid}.{pn}", px, y + b / 2 + 4.5, 0, 1, box, f"{name} encoder: {d} ({pn})")
    for k, (pn, d) in enumerate([("push", "LEFT pin of the 2"), ("push 2", "RIGHT pin of the 2")]):
        px = x + (k - 0.5) * 5.0 * s
        leg(p, px, y - b / 2 + 1, px, y - b / 2 - 4.5)
        p.T(px, y - b / 2 - 6.0, "push" if k == 0 else "push 2", 2.0, "middle", "#000", "bold")
        p.pin(f"{pid}.{pn}", px, y - b / 2 - 4.5, 0, -1, box, f"{name} encoder: {d} ({pn})")
    p.T(x, y + b / 2 + 12.0, "back of the encoder", 1.8, "middle", "#666")


def footswitch(p, pid, name, x, y, s=2.2, up=False):
    """from the back; up=True draws it turned so its 2 lugs face up (it's round: it fits any way)"""
    r = 6.6 * s
    box = (x - r - 2, y - r - 8, x + r + 2, y + r + 8)
    C(p, x, y, r, "url(#metal)", "#555", 0.35, True)
    C(p, x, y, r - 2.2, "url(#dark)", "#111")
    sg = -1 if up else 1
    p.T(x, y + (5.0 if up else -3.0), name, 3.0, "middle", "#fff", "bold", halo=False)
    p.T(x, y + (8.2 if up else 0.4), "footswitch (back)", 1.7, "middle", "#bbb", halo=False)
    for k, (pn, d) in enumerate([("lug 1", "either lug"), ("lug 2", "the other lug")]):
        px = x + (k - 0.5) * 4.4 * s
        y0 = y + 3.0 if not up else y - 3.0 - (r - 1.0)
        R(p, px - 1.4, y0, 2.8, r - 1.0, "url(#metal)", "#666", 0.4)
        C(p, px, y + sg * (r - 1.2), 0.6, "#555", "none")
        p.pin(f"{pid}.{pn}", px, y + sg * (r + 1.2), 0, sg, box, f"{name} footswitch: {d}")


def pot(p, pid, name, x, y, s=2.8):
    b = 9.5 * s
    box = (x - b / 2 - 2, y - b / 2 - 2, x + b / 2 + 20, y + b / 2 + 14)
    R(p, x - b / 2, y - b / 2, b, b, "url(#potc)", "#445", 1.2, 0.35, True)
    R(p, x - b / 2 + 1.4, y - b / 2 + 1.4, b - 2.8, b * 0.5, "url(#metal)", "#667", 0.8, 0.25)
    p.T(x, y + 3.0, name, 2.6, "middle", "#111", "bold", halo=False)
    p.T(x, y + 6.2, "back of the pot", 1.7, "middle", "#223", halo=False)
    for row, (g, ln, rname) in enumerate([("l", 5.5, "row nearer the PANEL"), ("r", 10.5, "row nearer YOU")]):
        for k, n in enumerate(["3", "2", "1"]):
            px = x + (k - 1) * 2.5 * s + (1.0 if row else -1.0)
            leg(p, px, y + b / 2 - 1, px, y + b / 2 + ln, 0.8)
            pos = ["LEFT", "MIDDLE", "RIGHT"][k]
            p.pin(f"{pid}.{n} {g}", px, y + b / 2 + ln, 0, 1, box, f"{name}: pin {n} {g} ({pos} pin of the {rname})")
    p.T(x + b / 2 + 2.5, y + b / 2 + 5.0, "row l (nearer the panel): 3 2 1", 1.8, "start", "#333")
    p.T(x + b / 2 + 2.5, y + b / 2 + 10.0, "row r (nearer you): 3 2 1", 1.8, "start", "#333")


SW_ANG = {"1": -68, "2": -22, "3": 22, "4": 68, "5": 112, "6": 158, "7": 202, "8": 248}
SW_NUM = {"A1": "1", "A2": "2", "A3": "3", "A4": "4", "B1": "5", "B2": "6", "B3": "7", "B4": "8"}


def rswitch(p, pid, name, x, y, s=3.0, used=(), turn=0):
    r = 8.0 * s
    box = (x - r - 4, y - r - 4, x + r + 4, y + r + 4)
    p.geo[pid] = (x, y, r, turn)
    C(p, x, y, r + 1.4, "url(#ivory)", "#8a8470", 0.4, True)
    for k in range(24):   # the ribs moulded round the rim
        a = math.radians(k * 15 + 7)
        p.add(f'<path d="M{x+(r+0.2)*math.cos(a):.2f},{y-(r+0.2)*math.sin(a):.2f} L{x+(r+1.3)*math.cos(a):.2f},{y-(r+1.3)*math.sin(a):.2f}" stroke="#b9b29c" stroke-width="0.35"/>')
    C(p, x, y, 2.9 * s, "#efe9d8", "#a49d86", 0.3)
    p.T(x, y + r + 6.0, name, 2.9, "middle", "#000", "bold")
    p.T(x, y + r + 9.2, "back of the switch: lug numbers 1-8, A and B are moulded on it" + ("; drawn turned (it fits any way round)" if turn else ""), 1.7, "middle", "#555")
    for full, n in SW_NUM.items():
        a = math.radians(SW_ANG[n] + turn)
        px, py = x + r * math.cos(a), y - r * math.sin(a)
        on = f"{pid}.{full}" in used
        leg(p, x + (r - 2.6) * math.cos(a), y - (r - 2.6) * math.sin(a), px, py, 1.5)
        tx, ty = x + (r - 5.0) * math.cos(a), y - (r - 5.0) * math.sin(a) + 1.0
        p.T(tx, ty, n, 2.6, "middle", "#000" if on else "#b5ae98", "bold", halo=False)
        pole = "pole A" if full[0] == "A" else "pole B"
        p.pin(f"{pid}.{full}", px, py, math.cos(a), -math.sin(a), box, f"{name}: lug {n} ({pole}, position {full[1]})")
    for full, base, t in (("A com", 0, "A"), ("B com", 180, "B")):
        a = math.radians(base + turn)
        px, py = x + 2.4 * s * math.cos(a), y - 2.4 * s * math.sin(a)
        C(p, px, py, 1.1, "#d9dce0", "#6d7177", 0.35)
        on = f"{pid}.{full}" in used
        p.T(x + (2.4 * s - 3.4) * math.cos(a), y - (2.4 * s - 3.4) * math.sin(a) + 1.0, t, 2.6, "middle", "#000" if on else "#b5ae98", "bold", halo=False)
        p.pin(f"{pid}.{full}", px, py, math.cos(a), -math.sin(a), box, f"{name}: common {t} (the inner lug by the moulded '{t}')")


def dcjack(p, pid, x, y, s=2.6):
    r = 6.0 * s
    box = (x - r - 4, y - r - 4, x + r + 4, y + r + 4)
    C(p, x, y, r + 1.5, "url(#metal)", "#555", 0.35, True)
    C(p, x, y, r, "url(#dark)", "#111")
    p.T(x, y + r + 6.0, "9V DC jack (back)", 2.9, "middle", "#000", "bold")
    lugs = {"- centre": (0, -1, "the CENTRE-PIN lug, the middle one: - (centre negative)"),
            "+ sleeve": (1, 0, "the SLEEVE lug, on the side: +"),
            "switch": (-1, 0, "the switch lug: stays empty")}
    for pn, (dx, dy, d) in lugs.items():
        px, py = x + dx * (r - 1.5), y + dy * (r - 1.5)
        R(p, px - (2.2 if dx else 1.4), py - (1.4 if dx else 2.2), 4.4 if dx else 2.8, 2.8 if dx else 4.4, "url(#metal)", "#666", 0.5)
        C(p, px, py, 0.6, "#555", "none")
        lab = {"- centre": "- centre pin", "+ sleeve": "+ sleeve", "switch": "switch (empty)"}[pn]
        p.T(x + dx * (r * 0.42), y + dy * (r * 0.42) + (1.0 if dy == 0 else 2.0), lab, 1.9, "middle", "#fff", "bold", halo=False)
        p.pin(f"{pid}.{pn}", px + dx * 2.2, py + dy * 2.2, dx, dy, box, f"9V jack: {d}")


def header(p, pid, title, names, x, y, s=2.2, up=False):
    """a carrier-board header: board edge with the names printed on it, black spacer bar, gold right-angle pins.
    up=True: the pins point up (the board below them)"""
    pitch = 2.54 * s
    w = pitch * len(names) + 8
    by = y + 4 if up else y - 14
    box = (x - 4, min(by, y - 8), x - 4 + w, max(by + 14, y + 8))
    R(p, x - 4, by, w, 10, "url(#pcbg)", "#14502f", 1.0, 0.3, True)
    p.T(x - 4 + w / 2, by + (8.2 if up else 3.0), f"carrier board: {title}", 1.9, "middle", "#fff", "bold", halo=False)
    R(p, x - 1.5, (y - 1.2) if up else (y - 4.2), pitch * len(names) + 3, 5.4, "#1a1a1a", "#000", 0.4)
    for i, n in enumerate(names):
        px = x + pitch * (i + 0.5)
        y0, y1 = ((y + 0.8, y - 7.0) if up else (y - 0.8, y + 7.0))
        p.add(f'<rect x="{px-0.55:.2f}" y="{min(y0,y1):.2f}" width="1.1" height="{abs(y1-y0):.2f}" fill="url(#gold)" stroke="#7a5a10" stroke-width="0.15"/>')
        p.T(px, by + (4.6 if up else 7.4), n, 1.65, "middle", "#fff", "bold", halo=False)
        p.pin(f"{pid}.{n}", px, y1, 0, -1 if up else 1, box, f"carrier '{title}' header: '{n}' ({ordinal(i, len(names))})", "plug")


def ordinal(i, n):
    if i == 0:
        return "1st from the left"
    if i == n - 1:
        return "last on the right"
    return f"{i+1}{'nd' if i == 1 else 'rd' if i == 2 else 'th'} from the left"


def seed(p, x, y, used, s=1.8):
    """the Seed3 socket (the glued block of jumper ends in the right wall) from inside, USB-C end on your LEFT:
    places 1-20 the top row, 40-21 the bottom row; each place is a jumper's female end"""
    pitch = 2.54 * s
    box = (x - 8, y - 9, x + pitch * 20 + 4, y + pitch * 2 + 9)
    R(p, x - 4, y - 3, pitch * 20 + 8, pitch * 2 + 11, "#2a2a2a", "#000", 1.6, 0.35, True)
    p.T(x + pitch * 10, y + pitch * 2 + 5.2, "Seed3 socket: the jumper-end block in the right wall, from INSIDE (USB-C end on your left)",
        1.8, "middle", "#eee", "bold", halo=False)
    for i in range(20):
        for row, n in ((0, i + 1), (1, 40 - i)):
            px, py = x + pitch * (i + 0.5), y + row * pitch + pitch / 2 + 1.0
            on = f"seed.{n}" in used
            R(p, px - pitch / 2 + 0.35, py - pitch / 2 + 0.35, pitch - 0.7, pitch - 0.7, "#111" if not on else "#000",
              "#555" if not on else "#d4a017", 0.3, 0.25 if not on else 0.45)
            if not on:
                p.T(px, py + 0.6, str(n), 1.5, "middle", "#8a8a8a", halo=False)
            else:
                p.pin(f"seed.{n}", px, py, 0, -1, box, f"Seed3 socket place {n} ({'top' if row == 0 else 'bottom'} row)", "seed")
    p.T(x - 5.5, y + pitch + 1.5, "USB-C", 1.8, "end", "#444")


def screen(p, x, y, s=2.0):
    names = ["1 VCC", "2 GND", "3 CS", "4 RST", "5 DC", "6 SDI", "7 SCK", "8 LED", "9 SDO", "10 TCLK", "11 TCS", "12 TDIN", "13 TDO", "14 TIRQ"]
    printed = ["VCC", "GND", "CS", "RESET", "DC", "SDI(MOSI)", "SCK", "LED", "SDO(MISO)", "T_CLK", "T_CS", "T_DIN", "T_DO", "T_IRQ"]
    pitch = 2.54 * s
    hgt = pitch * 14 + 10
    box = (x - 46, y - 5, x + 10, y - 5 + hgt)
    R(p, x - 46, y - 5, 46, hgt, "url(#pcbb)", "#0d2a66", 1.6, 0.35, True)
    R(p, x - 40, y + 4, 22, hgt - 18, "#0f1b33", "#000", 0.8)
    p.T(x - 29, y + hgt / 2 - 5, "screen module, back", 2.4, "middle", "#cfe0ff", "bold", -90, halo=False)
    R(p, x - 0.8, y + 0.6, 3.6, pitch * 14 - 1.2, "#1a1a1a", "#000", 0.4)
    for i, (n, pr) in enumerate(zip(names, printed)):
        py = y + pitch * (i + 0.5)
        p.add(f'<rect x="{x:.2f}" y="{py-0.55:.2f}" width="7.5" height="1.1" fill="url(#gold)" stroke="#7a5a10" stroke-width="0.15"/>')
        p.T(x - 2.0, py + 0.7, pr, 1.75, "end", "#fff", "bold", halo=False)
        p.pin(f"lcd.{n}", x + 7.5, py, 1, 0, box, f"screen: the pin printed '{pr}' (pin {n.split()[0]})", "plug")


def pcf(p, x, y):
    """Comimark PCF8574 board, parts side up: the 4 input pins at the left end, P0-P7 along the bottom edge, the
    4 pass-through pins at the right end"""
    w, h = 62.0, 26.0
    box = (x - 9, y - 4, x + w + 9, y + h + 9)
    R(p, x, y, w, h, "url(#pcbb)", "#0d2a66", 1.6, 0.35, True)
    R(p, x + 26, y + 6, 10, 9, "#111", "#000", 0.6)
    p.T(x + 31, y + 4.4, "PCF8574", 1.9, "middle", "#fff", "bold", halo=False)
    p.T(x + w / 2, y - 1.8, "PCF8574 board (Amazon), parts side up", 1.9, "middle", "#333")
    pitch = 4.0
    for i, n in enumerate(["VCC", "GND", "SDA", "SCL"]):
        py = y + 4 + pitch * (i + 0.5)
        p.add(f'<rect x="{x-7:.2f}" y="{py-0.55:.2f}" width="7.5" height="1.1" fill="url(#gold)"/>')
        p.T(x + 1.8, py + 0.7, n, 1.75, "start", "#fff", "bold", halo=False)
        p.pin(f"pcf.{n}", x - 7, py, -1, 0, box, f"PCF8574: '{n}' on the INPUT pins (left end)", "plug")
        R(p, x + w - 0.5, py - 1.1, 5.5, 2.2, "#111", "#000", 0.3)
        p.T(x + w - 1.8, py + 0.7, n, 1.75, "end", "#fff", "bold", halo=False)
        p.pin(f"pcf.{n} ", x + w + 5, py, 1, 0, box, f"PCF8574: '{n}' on the PASS-THROUGH pins (right end)", "plug")
    R(p, x + 9, y + h - 0.6, pitch * 8 + 2, 2.4, "#1a1a1a", "#000", 0.3)
    for i in range(8):
        px = x + 10 + pitch * (i + 0.5)
        p.add(f'<rect x="{px-0.55:.2f}" y="{y+h:.2f}" width="1.1" height="7" fill="url(#gold)"/>')
        p.T(px, y + h - 2.0, f"P{i}", 1.7, "middle", "#fff", "bold", halo=False)
        p.pin(f"pcf.P{i}", px, y + h + 7, 0, 1, box, f"PCF8574: 'P{i}' on the row of 8", "plug")
    p.T(x + 10 + pitch * 8.5, y + h + 4.5, "INT: empty", 1.6, "start", "#555")


def bay(p, x, y):
    box = (x - 4, y - 4, x + 30, y + 14)
    R(p, x - 2, y, 26, 11, "#2a2a2a", "#000", 1.0, 0.35, True)
    p.T(x + 11, y + 16, "expansion bay (left wall): 4 glued jumper ends", 1.8, "middle", "#000", "bold")
    for i, n in enumerate(["3v3", "gnd", "scl", "sda"]):
        px = x + 2 + 5.5 * i + 1.5
        R(p, px - 2.1, y + 2, 4.2, 7, "#000", "#d4a017", 0.3, 0.4)
        p.T(px, y + 6.3, n, 1.6, "middle", "#fff", "bold", halo=False)
        p.pin(f"bay.{n}", px, y + 2, 0, -1, box, f"expansion bay: the '{n}' jumper end", "seed")


def light(p, pid, name, x, y, s=2.6):
    box = (x - 10, y - 22, x + 10, y + 14)
    C(p, x, y, 3.4 * s, "url(#chrome)", "#666", 0.35, True)
    C(p, x, y, 1.7 * s, "#f3f6f3", "#9aa", 0.3)
    p.T(x, y + 3.4 * s + 4.6, name, 2.4, "middle", "#000", "bold")
    p.T(x, y + 3.4 * s + 7.6, "back of its chrome bezel", 1.7, "middle", "#555")
    for k, pn in enumerate(["leg 1", "leg 2"]):
        px = x + (k - 0.5) * 2.54 * s
        ln = 13 + 2.5 * k
        leg(p, px, y, px, y - ln, 0.7)
        p.pin(f"{pid}.{pn}", px, y - ln, 0, -1, box, f"{name}: the {'LEFT' if k == 0 else 'RIGHT'} leg (either way round)")


def resistor(p, k, x, y):
    box = (x - 3, y - 10, x + 3, y + 10)
    leg(p, x, y - 9.5, x, y + 9.5, 0.6)
    R(p, x - 1.7, y - 4.8, 3.4, 9.6, "#e7d3a8", "#8a7350", 1.6, 0.3, True)
    for j, c in enumerate(["#f08c00", "#f08c00", "#6b3a12", "#c9a227"]):   # 330 ohm: orange orange brown gold
        p.add(f'<rect x="{x-1.7:.2f}" y="{y-3.4+j*1.9:.2f}" width="3.4" height="0.9" fill="{c}"/>')
    p.pin(f"rr.a{k}", x, y - 9.5, 0, -1, box, f"330 ohm resistor {k}: its TOP leg")
    p.pin(f"rr.b{k}", x, y + 9.5, 0, 1, box, f"330 ohm resistor {k}: its BOTTOM leg")


# ---------------------------------------------------------------- the pages
def pages():
    P = []

    def new(title, note, lanes):
        p = Page(title, note, lanes)
        P.append(p)
        return p

    p = new("Power: 9V jack -> power switch -> carrier board -> Seed3",
            "+ (red) goes into BOTH commons, A and B. Lugs 2 3 4 and 6 7 8 are all ON: link them with bare offcuts (the "
            "short silver links, bent round the OUTSIDE so they never touch lug 1 or 5) and run ONE red wire from them to "
            "'dc +'. Lugs 1 and 5 = OFF: leave them empty.", (114, 3.2))
    dcjack(p, "dc", 42, 64)
    rswitch(p, "pw", "power switch", 126, 66, used={"pw.A com", "pw.B com", "pw.A2", "pw.A3", "pw.A4", "pw.B2", "pw.B3", "pw.B4"})
    header(p, "j10", "seed3 + 9v", ["dc +", "dc -", "vin", "gnd", "scl", "sda", "sck", "fs", "tx", "rx"], 18, 158, up=True)
    seed(p, 100, 170, {"seed.39", "seed.40"})

    p = new("Carrier board -> Seed3: the 6 signal jumpers",
            "Plug these onto the board's header BEFORE the board goes in the box (the names are printed on the board). "
            "Their other ends go into the Seed3 socket places shown.", (88, 3.4))
    header(p, "j10", "seed3 + 9v", ["dc +", "dc -", "vin", "gnd", "scl", "sda", "sck", "fs", "tx", "rx"], 50, 62)
    seed(p, 60, 150, {f"seed.{n}" for n in (12, 13, 32, 33, 34, 35)})

    p = new("The gain pots -> their headers on the carrier board",
            "Short wires soldered on each pot, a female jumper end on the header. The pot's row nearer the PANEL is 'l', "
            "the row nearer YOU is 'r'. The header names are printed on the board.", (104, 2.6))
    pot(p, "pl", "Input Gain", 38, 58)
    header(p, "j20", "in gain knob", ["3 l", "2 l", "1 l", "3 r", "2 r", "1 r"], 20, 160, up=True)
    pot(p, "ph", "Output Gain", 128, 58)
    header(p, "j19", "out gain knob", ["3 l", "2 l", "1 l", "3 r", "2 r", "1 r"], 110, 160, up=True)

    p = new("Screen -> Seed3",
            "Read the names printed beside the screen's pins. 'SDO(MISO)' and 'T_IRQ' stay empty; 'GND' is on the "
            "ground-chain page.", (128, 2.6))
    screen(p, 58, 34)
    seed(p, 92, 172, {f"seed.{n}" for n in (38, 8, 1, 37, 11, 9, 23, 29, 30, 31, 36)})

    p = new("The 4 encoders -> Seed3",
            "On the back of each encoder: a row of 3 pins and a row of 2. C and 'push 2' are on the ground-chain page. "
            "If a knob turns the wrong way, flip 'knobs' in the config tab: no rewiring.", (108, 2.6))
    for k, x in enumerate([28, 74, 120, 166]):
        encoder(p, f"e{k+1}", f"pg-{k+1}", x, 62)
    p.only = lambda w: w[1].startswith("seed.")
    seed(p, 52, 176, {f"seed.{n}" for n in (2, 3, 4, 5, 6, 7, 14, 15, 22, 26, 27, 28)})

    p = new("The 3 footswitches -> Seed3",
            "Either lug of each footswitch goes to its Seed3 place; the other lug is on the ground-chain page.", (110, 3.4))
    for k, x in enumerate([40, 100, 160]):
        footswitch(p, f"f{'abc'[k]}", f"pg-{'abc'[k]}", x, 66)
    p.only = lambda w: w[1].startswith("seed.")
    seed(p, 52, 160, {"seed.24", "seed.25", "seed.10"})

    p = new("The ground chain (black wire, part to part)",
            "One black wire from each lug to the next, in step order. The last one (from the screen's GND) is SPLICED onto "
            "the jumper that runs from the board's 'gnd' to Seed3 place 40, about 3 cm from the socket: strip 5 mm, wrap, "
            "solder, heat shrink (a socket place only holds one jumper end).", (92, 2.4))
    for k, x in enumerate([24, 66, 108, 150]):
        encoder(p, f"e{k+1}", f"pg-{k+1}", x, 52, 2.0)
    for k, x in enumerate([22, 60, 98]):
        footswitch(p, f"f{'abc'[k]}", f"pg-{'abc'[k]}", x, 152, 1.8, up=True)
    screen(p, 186, 128, 1.15)
    seed(p, 118, 186, {"seed.40"}, 1.2)

    p = new("PCF8574 board -> carrier board and the expansion bay",
            "The 4 input pins go to the board's 'expansion' header; the 4 pass-through pins go to the bay in the left wall. "
            "The PCF8574 board sits on the inside of the lid (velcro).", (100, 3.0))
    pcf(p, 66, 40)
    header(p, "j21", "expansion", ["3v3", "gnd", "scl", "sda"], 30, 160, up=True)
    bay(p, 130, 156)

    p = new("Page switch -> PCF8574 board",
            "Pole A only: lugs 1 2 3 4 and common A. Pole B (5 6 7 8 and B) stays empty. Common A goes to GND; that pin "
            "already has the carrier's jumper on it, so solder this one on the PCF board's UNDERSIDE, on GND's joint.", (100, 3.2))
    pcf(p, 60, 36)
    rswitch(p, "ps", "page switch", 92, 150, used={"ps.A com", "ps.A1", "ps.A2", "ps.A3", "ps.A4"}, turn=90)

    p = new("Level lights + 330 ohm resistors -> PCF8574 board",
            "Each light's 2 legs go to 2 P pins, either way round (red with signal? swap that light's 2 wires). P4-P7 each "
            "also get a 330 ohm to VCC, soldered on the PCF board's UNDERSIDE; the 4 resistors' bottom legs are joined "
            "and go to a VCC pin.", (90, 2.8))
    pcf(p, 50, 34)
    light(p, "li", "in light", 40, 160)
    light(p, "lo", "out light", 90, 160)
    for k in range(4):
        resistor(p, k + 1, 136 + 10 * k, 150)
    return P


# ---------------------------------------------------------------- neat wires
def approach(pin, lane, k):
    """the points from a pin out to its lane: straight out of the pin, round its own part if the lane is behind it"""
    x, y, dx, dy, box, kind = pin
    st = 3.0
    if dy:
        if (lane - y) * dy > 0:
            return [(x, y), (x, lane)]
        side = box[2] + 1.5 + 1.1 * k if x >= (box[0] + box[2]) / 2 else box[0] - 1.5 - 1.1 * k
        edge = box[1] - 1.5 - 0.8 * k if dy < 0 else box[3] + 1.5 + 0.8 * k
        return [(x, y), (x, edge), (side, edge), (side, lane)]
    x1 = x + dx * (st + 1.4 * k)
    return [(x, y), (x1, y), (x1, lane)]


def rounded(pts, r=1.6):
    pts = [p for i, p in enumerate(pts) if i == 0 or math.dist(p, pts[i - 1]) > 1e-6]
    d = f"M{pts[0][0]:.2f},{pts[0][1]:.2f}"
    for i in range(1, len(pts) - 1):
        (x0, y0), (x1, y1), (x2, y2) = pts[i - 1], pts[i], pts[i + 1]
        l0, l1 = max(math.dist((x0, y0), (x1, y1)), 1e-9), max(math.dist((x1, y1), (x2, y2)), 1e-9)
        a = min(r, l0 / 2, l1 / 2)
        ux, uy, vx, vy = (x1 - x0) / l0, (y1 - y0) / l0, (x2 - x1) / l1, (y2 - y1) / l1
        d += f" L{x1-ux*a:.2f},{y1-uy*a:.2f} Q{x1:.2f},{y1:.2f} {x1+vx*a:.2f},{y1+vy*a:.2f}"
    d += f" L{pts[-1][0]:.2f},{pts[-1][1]:.2f}"
    return d


def wire_svg(d, c, w=1.25):
    return (f'<path d="{d}" fill="none" stroke="#000" stroke-opacity="0.55" stroke-width="{w+0.55}" stroke-linecap="round" stroke-linejoin="round"/>'
            f'<path d="{d}" fill="none" stroke="{c}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"/>'
            f'<path d="{d}" fill="none" stroke="#fff" stroke-opacity="0.35" stroke-width="{w*0.28:.2f}" stroke-linecap="round" '
            f'stroke-linejoin="round" transform="translate(-0.18,-0.18)"/>')


def end_svg(pin, c, label=None):
    x, y, dx, dy, box, kind = pin
    if kind == "plug":   # a jumper's black female end pushed onto the pin
        if dx:
            return f'<rect x="{x - (0 if dx > 0 else 6.5):.2f}" y="{y-1.3:.2f}" width="6.5" height="2.6" rx="0.4" fill="#141414" stroke="{c}" stroke-width="0.35"/>'
        return f'<rect x="{x-1.3:.2f}" y="{y - 6.5 if dy < 0 else y:.2f}" width="2.6" height="6.5" rx="0.4" fill="#141414" stroke="{c}" stroke-width="0.35"/>'
    if kind == "seed":   # its place in the socket block, with the place number kept readable
        return (f'<rect x="{x-2.2:.2f}" y="{y-1.7:.2f}" width="4.4" height="3.4" rx="0.7" fill="#fff" stroke="{c}" stroke-width="0.5"/>'
                f'<text x="{x:.2f}" y="{y+0.8:.2f}" font-family="DejaVu Sans" font-size="2.2" text-anchor="middle" font-weight="bold" fill="#000">{label or ""}</text>')
    return f'<circle cx="{x:.2f}" cy="{y:.2f}" r="1.05" fill="#e4e6e9" stroke="#80858c" stroke-width="0.3"/>'   # a solder joint


def render(P):
    files, placed = [], {}
    for n, p in enumerate(P, 1):
        ws = [w for w in WIRES if w[0] in p.pins and w[1] in p.pins and w not in placed and (p.only is None or p.only(w))]
        for w in ws:
            placed[w] = n
        under, over, ends, steps = [], [], [], []
        same = lambda w: w[3].startswith("join") and w[0].split(".")[0] == w[1].split(".")[0]
        routed = [w for w in ws if not same(w)]
        routed.sort(key=lambda w: p.pins[w[0]][0] + p.pins[w[1]][0])
        lane_of = {w: p.lanes[0] + p.lanes[1] * i for i, w in enumerate(routed)}
        for k, w in enumerate(ws, 1):
            a, b, col, what = w
            c = COL[col]
            if w in lane_of:
                L = lane_of[w]
                ki = routed.index(w) % 6
                pa, pb = approach(p.pins[a], L, ki), approach(p.pins[b], L, ki)
                under.append(wire_svg(rounded(pa + pb[::-1]), c))
                mx, my = (pa[-1][0] + pb[-1][0]) / 2, L   # the step number sits on the sideways run
            else:   # a bare offcut between 2 lugs of one part
                (x0, y0), (x1, y1) = p.pins[a][:2], p.pins[b][:2]
                d = f"M{x0:.2f},{y0:.2f} L{x1:.2f},{y1:.2f}"
                pid = a.split(".")[0]
                if pid in p.geo and "com" not in a and "com" not in b:   # round the rim, outside the lugs
                    gx, gy, gr, _ = p.geo[pid]
                    a0 = math.atan2(gy - y0, x0 - gx)
                    a1 = math.atan2(gy - y1, x1 - gx)
                    while a1 - a0 > math.pi:
                        a1 -= 2 * math.pi
                    while a0 - a1 > math.pi:
                        a1 += 2 * math.pi
                    rr = gr + 3.2
                    pts = [(x0, y0)] + [(gx + rr * math.cos(a0 + (a1 - a0) * t / 12), gy - rr * math.sin(a0 + (a1 - a0) * t / 12)) for t in range(13)] + [(x1, y1)]
                    d = "M" + " L".join(f"{u:.2f},{v:.2f}" for u, v in pts)
                    mid = a0 + (a1 - a0) / 2
                    x0 = x1 = gx + (rr + 3.0) * math.cos(mid)   # the step number just outside the link
                    y0 = y1 = gy - (rr + 3.0) * math.sin(mid)
                under.append(f'<path d="{d}" fill="none" stroke="#6d7177" stroke-width="1.1" stroke-linecap="round" stroke-linejoin="round"/>'
                             f'<path d="{d}" fill="none" stroke="#e8eaed" stroke-width="0.7" stroke-linecap="round" stroke-linejoin="round"/>')
                mx, my = (x0 + x1) / 2, (y0 + y1) / 2
            for e in (a, b):
                ends.append(end_svg(p.pins[e], c, e.split(".")[1] if e.startswith("seed.") else None))
            over.append(f'<circle cx="{mx:.2f}" cy="{my:.2f}" r="2.2" fill="#fff" stroke="{c}" stroke-width="0.55"/>'
                        f'<text x="{mx:.2f}" y="{my+0.85:.2f}" font-family="DejaVu Sans" font-size="2.3" text-anchor="middle" '
                        f'font-weight="bold" fill="#000">{k}</text>')
            how = "" if what in ("jumper", "ground chain", "short wire, soldered on the pot") else what
            steps.append((k, c, f"{p.desc[a]}  ->  {p.desc[b]}", how))
        body = p.o
        p.o = []
        p.T(8, 10, f"PG-1 wiring  {n}/{len(P)}:  {p.title}", 4.6, w="bold", halo=False)
        line, yy = "", 16.0
        for word in p.note.split():
            if len(line) + len(word) > 140:
                p.T(8, yy, line, 2.3, c="#333", halo=False)
                yy += 3.1
                line = ""
            line += word + " "
        p.T(8, yy, line, 2.3, c="#333", halo=False)
        head = p.o
        p.o = []
        sx, sy = 204.0, 31.0
        p.add(f'<rect x="{sx-3:.2f}" y="22" width="{W-sx-2:.2f}" height="{H-28:.2f}" rx="2" fill="#fff" stroke="#ddd" stroke-width="0.3"/>')
        p.T(sx, 27.5, "Steps (the number is on the wire)", 2.8, w="bold", halo=False)
        for k, c, t, how in steps:
            first = True
            for txt, size, colr in ((t, 1.9, "#111"), (how, 1.65, "#666")):
                while txt:
                    cut = txt[:56]
                    if len(txt) > 56 and " " in cut:
                        cut = cut[:cut.rfind(" ")]
                    if first:
                        p.add(f'<circle cx="{sx+1.7:.2f}" cy="{sy-0.75:.2f}" r="1.7" fill="{c}"/>'
                              f'<text x="{sx+1.7:.2f}" y="{sy:.2f}" font-family="DejaVu Sans" font-size="2.0" text-anchor="middle" '
                              f'font-weight="bold" fill="#fff">{k}</text>')
                    p.T(sx + 4.6, sy, cut, size, c=colr, halo=False)
                    txt = txt[len(cut):].lstrip()
                    sy += 2.55
                    first = False
            sy += 1.0
        side = p.o
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">' + DEFS +
               f'<rect width="{W}" height="{H}" fill="#f6f4ef"/>' + "\n".join(head + body + under + ends + over + side) + "</svg>\n")
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
