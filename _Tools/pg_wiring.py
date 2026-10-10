#!/usr/bin/env python3
"""PG-1 full wiring drawing: every part, every lug, every wire (point to point).

One list (WIRES) is the truth: the drawing (wiring-diagram.svg/.png/.pdf) and the table (WIRE-LIST.md) both come
from it. Parts are drawn with their pins in their real order along the top edge; every wire leaves its pin, runs up
to its own lane (never shared, so no two wires ever run on top of each other), across, and down to the other pin.
Each wire carries a number at both ends: look it up in WIRE-LIST.md.
"""
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "PG-Pedal-1", "05-Wiring-and-Schematics")

# ---------------------------------------------------------------- parts: (id, title, subtitle, [pin names], width)
# pins are in the part's real order (left to right as you look at its lugs / header from the wiring side)
SEED = [str(i) for i in range(1, 41)]
SEED_NAMES = {1: "D0", 2: "D1", 3: "D2", 4: "D3", 5: "D4", 6: "D5", 7: "D6", 8: "D7", 9: "D8", 10: "D9", 11: "D10",
              12: "D11", 13: "D12", 14: "D13", 15: "D14", 16: "key", 17: "-", 18: "-", 19: "-", 20: "AGND", 21: "3V3A",
              22: "D15", 23: "D16", 24: "D17", 25: "D18", 26: "D19", 27: "D20", 28: "D21", 29: "D22", 30: "D23",
              31: "D24", 32: "D25", 33: "D26", 34: "D27", 35: "D28", 36: "D29", 37: "D30", 38: "3V3D", 39: "VIN",
              40: "DGND"}

PARTS = [
    ("dc", "9V DC jack", "A-2237, left wall", ["- centre", "+ sleeve", "switch"]),
    ("pw", "power switch", "A-8233, pole A | pole B", ["A com", "A1", "A2", "A3", "A4", "B com", "B1", "B2", "B3", "B4"]),
    ("j10", "carrier J10", "seed3 + 9v", ["dc +", "dc -", "vin", "gnd", "scl", "sda", "sck", "fs", "tx", "rx"]),
    ("j20", "carrier pg-line", "header (in gain)", ["3 l", "2 l", "1 l", "3 r", "2 r", "1 r"]),
    ("pl", "Input Gain pot", "A-8618 dual 10k", ["1 l", "2 l", "3 l", "1 r", "2 r", "3 r"]),
    ("j19", "carrier pg-hp", "header (out gain)", ["3 l", "2 l", "1 l", "3 r", "2 r", "1 r"]),
    ("ph", "Output Gain pot", "A-8618 dual 10k", ["1 l", "2 l", "3 l", "1 r", "2 r", "3 r"]),
    ("j21", "carrier expansion", "header", ["3v3", "gnd", "scl", "sda"]),
    ("pcf", "PCF8574 board", "in hdr | P header | pass-through",
     ["VCC", "GND", "SDA", "SCL", "P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "VCC ", "GND ", "SDA ", "SCL "]),
    ("rr", "4 x 330 ohm", "A-2325", ["a1", "b1", "a2", "b2", "a3", "b3", "a4", "b4"]),
    ("li", "in level light", "A-1076 in bezel", ["leg 1", "leg 2"]),
    ("lo", "out level light", "A-1076 in bezel", ["leg 1", "leg 2"]),
    ("ps", "page switch", "A-8233, pole A (B empty)", ["A com", "A1", "A2", "A3", "A4"]),
    ("bay", "expansion bay", "4 glued ends, left wall", ["3v3", "gnd", "scl", "sda"]),
    ("lcd", "screen A-8180", "14-pin header", ["1 VCC", "2 GND", "3 CS", "4 RST", "5 DC", "6 SDI", "7 SCK", "8 LED",
                                                "9 SDO", "10 TCLK", "11 TCS", "12 TDIN", "13 TDO", "14 TIRQ"]),
    ("e1", "pg-1 encoder", "A-6331", ["A", "C", "B", "push", "push 2"]),
    ("e2", "pg-2 encoder", "A-6331", ["A", "C", "B", "push", "push 2"]),
    ("e3", "pg-3 encoder", "A-6331", ["A", "C", "B", "push", "push 2"]),
    ("e4", "pg-4 encoder", "A-6331", ["A", "C", "B", "push", "push 2"]),
    ("fa", "pg-a footswitch", "A-1091", ["lug 1", "lug 2"]),
    ("fb", "pg-b footswitch", "A-1091", ["lug 1", "lug 2"]),
    ("fc", "pg-c footswitch", "A-1091", ["lug 1", "lug 2"]),
]

COL = {"pwr": "#d11f1f", "gnd": "#111111", "i2c": "#1f5fd1", "sai": "#7a3db8", "pot": "#e07b00", "sw": "#8b4513",
       "enc": "#1f9d3a", "lcd": "#1a8fb0", "led": "#e0409a"}

# ---------------------------------------------------------------- every wire: (from, to, colour, what / how)
# "part.pin"; "seed.N" = the Seed3 socket place N
WIRES = [
    # power: 9 V jack -> power switch -> carrier
    ("dc.+ sleeve", "pw.A com", "pwr", "9 V + into the switch (solder)"),
    ("pw.A com", "pw.B com", "pwr", "join both commons (short bare offcut)"),
    ("pw.A2", "j10.dc +", "pwr", "ON lugs -> dc + (join A2-A4 and B2-B4 with bare offcuts first)"),
    ("pw.A2", "pw.A3", "pwr", "join (bare offcut)"),
    ("pw.A3", "pw.A4", "pwr", "join (bare offcut)"),
    ("pw.A4", "pw.B2", "pwr", "join (bare offcut)"),
    ("pw.B2", "pw.B3", "pwr", "join (bare offcut)"),
    ("pw.B3", "pw.B4", "pwr", "join (bare offcut)"),
    ("dc.- centre", "j10.dc -", "gnd", "9 V - straight to the board (female end on the header)"),
    # carrier -> Seed3
    ("j10.vin", "seed.39", "pwr", "jumper: Seed3 power (9 V, protected)"),
    ("j10.gnd", "seed.40", "gnd", "jumper: Seed3 ground (the ground chain is spliced onto this wire)"),
    ("j10.scl", "seed.12", "i2c", "jumper: I2C clock"),
    ("j10.sda", "seed.13", "i2c", "jumper: I2C data"),
    ("j10.sck", "seed.35", "sai", "jumper: audio bit clock"),
    ("j10.fs", "seed.34", "sai", "jumper: audio frame clock"),
    ("j10.tx", "seed.33", "sai", "jumper: audio Seed3 -> board"),
    ("j10.rx", "seed.32", "sai", "jumper: audio board -> Seed3"),
    # gain pots -> their headers (short soldered wires, female end on the header)
    *[(f"pl.{p}", f"j20.{p}", "pot", "short wire, soldered on the pot") for p in ["1 l", "2 l", "3 l", "1 r", "2 r", "3 r"]],
    *[(f"ph.{p}", f"j19.{p}", "pot", "short wire, soldered on the pot") for p in ["1 l", "2 l", "3 l", "1 r", "2 r", "3 r"]],
    # PCF8574: in from the carrier, pass-through to the bay
    ("pcf.VCC", "j21.3v3", "pwr", "jumper (3.3 V)"),
    ("pcf.GND", "j21.gnd", "gnd", "jumper"),
    ("pcf.SDA", "j21.sda", "i2c", "jumper"),
    ("pcf.SCL", "j21.scl", "i2c", "jumper"),
    ("pcf.VCC ", "bay.3v3", "pwr", "jumper, its far end glued in the bay"),
    ("pcf.GND ", "bay.gnd", "gnd", "jumper, its far end glued in the bay"),
    ("pcf.SDA ", "bay.sda", "i2c", "jumper, its far end glued in the bay"),
    ("pcf.SCL ", "bay.scl", "i2c", "jumper, its far end glued in the bay"),
    # page switch -> PCF8574
    ("ps.A1", "pcf.P0", "sw", "position 1 (find with beep mode)"),
    ("ps.A2", "pcf.P1", "sw", "position 2"),
    ("ps.A3", "pcf.P2", "sw", "position 3"),
    ("ps.A4", "pcf.P3", "sw", "position 4"),
    ("ps.A com", "pcf.GND", "gnd", "pole A common: solder it on the UNDERSIDE of the PCF board, on the GND pin's solder joint"),
    # level lights: each LED's 2 legs between 2 P pins; a 330 ohm pull-up from VCC to each P pin
    ("li.leg 1", "pcf.P4", "led", "in light (swap its 2 legs, or set kSwapLightColours, if the colours are backwards)"),
    ("li.leg 2", "pcf.P5", "led", "in light"),
    ("lo.leg 1", "pcf.P6", "led", "out light"),
    ("lo.leg 2", "pcf.P7", "led", "out light"),
    ("rr.a1", "pcf.P4", "led", "330 ohm pull-up: solder its leg on the PCF board's underside, on P4's joint"),
    ("rr.a2", "pcf.P5", "led", "330 ohm pull-up"),
    ("rr.a3", "pcf.P6", "led", "330 ohm pull-up"),
    ("rr.a4", "pcf.P7", "led", "330 ohm pull-up"),
    ("rr.b1", "pcf.VCC ", "pwr", "the 4 resistors' other ends joined -> underside of a VCC pin's joint"),
    ("rr.b1", "rr.b2", "pwr", "join"),
    ("rr.b2", "rr.b3", "pwr", "join"),
    ("rr.b3", "rr.b4", "pwr", "join"),
    # screen
    ("lcd.1 VCC", "seed.38", "pwr", "jumper: 3.3 V"),
    ("lcd.3 CS", "seed.8", "lcd", "jumper"),
    ("lcd.4 RST", "seed.1", "lcd", "jumper"),
    ("lcd.5 DC", "seed.37", "lcd", "jumper"),
    ("lcd.6 SDI", "seed.11", "lcd", "jumper"),
    ("lcd.7 SCK", "seed.9", "lcd", "jumper"),
    ("lcd.8 LED", "seed.23", "lcd", "jumper: backlight"),
    ("lcd.10 TCLK", "seed.29", "lcd", "jumper: touch"),
    ("lcd.11 TCS", "seed.30", "lcd", "jumper: touch"),
    ("lcd.12 TDIN", "seed.31", "lcd", "jumper: touch"),
    ("lcd.13 TDO", "seed.36", "lcd", "jumper: touch"),
    # encoders + footswitches
    ("e1.A", "seed.2", "enc", "jumper"), ("e1.B", "seed.3", "enc", "jumper"), ("e1.push", "seed.4", "enc", "jumper"),
    ("e2.A", "seed.5", "enc", "jumper"), ("e2.B", "seed.6", "enc", "jumper"), ("e2.push", "seed.7", "enc", "jumper"),
    ("e3.A", "seed.14", "enc", "jumper"), ("e3.B", "seed.15", "enc", "jumper"), ("e3.push", "seed.22", "enc", "jumper"),
    ("e4.A", "seed.26", "enc", "jumper"), ("e4.B", "seed.27", "enc", "jumper"), ("e4.push", "seed.28", "enc", "jumper"),
    ("fa.lug 1", "seed.24", "sw", "jumper"), ("fb.lug 1", "seed.25", "sw", "jumper"), ("fc.lug 1", "seed.10", "sw", "jumper"),
    # the ground chain: one black wire part to part, its end spliced onto wire j10.gnd -> seed.40 near the socket
    ("e1.C", "e1.push 2", "gnd", "ground chain"),
    ("e1.push 2", "e2.C", "gnd", "ground chain"),
    ("e2.C", "e2.push 2", "gnd", "ground chain"),
    ("e2.push 2", "e3.C", "gnd", "ground chain"),
    ("e3.C", "e3.push 2", "gnd", "ground chain"),
    ("e3.push 2", "e4.C", "gnd", "ground chain"),
    ("e4.C", "e4.push 2", "gnd", "ground chain"),
    ("e4.push 2", "fa.lug 2", "gnd", "ground chain"),
    ("fa.lug 2", "fb.lug 2", "gnd", "ground chain"),
    ("fb.lug 2", "fc.lug 2", "gnd", "ground chain"),
    ("fc.lug 2", "lcd.2 GND", "gnd", "ground chain (screen GND: cut a jumper, solder its cut end in the chain)"),
    ("lcd.2 GND", "seed.40", "gnd", "chain's end: SPLICED onto the j10.gnd -> seed.40 jumper, 3 cm from the socket "
                                    "(strip 5 mm, wrap, solder, heat shrink). A socket place holds one jumper only"),
]

UNUSED = {"dc.switch": "not used", "pw.A1": "OFF position: empty", "pw.B1": "OFF position: empty",
          "lcd.9 SDO": "empty", "lcd.14 TIRQ": "empty"}


LOWER = {"lcd", "e1", "e2", "e3", "e4", "fa", "fb", "fc"}   # below the Seed3 strip; the rest sit above it


def main():
    P = 3.2                     # pin pitch (mm)
    PAD = 5.0                   # part side padding
    GAP = 4.0                   # between parts
    PH = 30.0                   # part box height
    top_parts = [p for p in PARTS if p[0] not in LOWER]
    low_parts = [p for p in PARTS if p[0] in LOWER]
    place = {}
    def row(parts):
        x = 10.0
        for pid, title, sub, pins in parts:
            w = max(2 * PAD + P * (len(pins) - 1), 27.0)
            place[pid] = (x, w)
            x += w + GAP
        return x
    width = max(row(top_parts), row(low_parts)) + 10.0
    for parts in (top_parts, low_parts):   # centre each row
        x1 = place[parts[-1][0]][0] + place[parts[-1][0]][1]
        d = (width - 10.0 - x1 - 10.0) / 2
        for p in parts:
            place[p[0]] = (place[p[0]][0] + d, place[p[0]][1])
    is_low = lambda e: e.startswith("seed.") is False and e.split(".")[0] in LOWER
    up_w = [w for w in WIRES if not (is_low(w[0]) or is_low(w[1]))]
    lo_w = [w for w in WIRES if is_low(w[0]) or is_low(w[1])]
    lane_step = 2.0
    TAGROOM = 14.0   # room under / over the pins for the stacked number tags
    top_y = 24.0                                          # upper parts' box top
    up_pin_y = top_y + PH                                 # their pins on the bottom edge
    up_lane0 = up_pin_y + TAGROOM
    seed_y = up_lane0 + lane_step * (len(up_w) + 1) + TAGROOM   # the Seed3 strip
    lo_lane0 = seed_y + 16 + TAGROOM
    lo_y = lo_lane0 + lane_step * (len(lo_w) + 1) + TAGROOM     # lower parts' box top (pins on it)
    height = lo_y + PH + 34
    pin_xy = {}
    for pid, title, sub, pins in PARTS:
        x0, w = place[pid]
        off = (w - P * (len(pins) - 1)) / 2
        for i, pn in enumerate(pins):
            pin_xy[f"{pid}.{pn}"] = (x0 + off + i * P, lo_y if pid in LOWER else up_pin_y)
    sp = 5.2
    sx0 = (width - sp * 39) / 2
    seed_x = {i + 1: sx0 + i * sp for i in range(40)}
    o = []
    T = lambda x, y, t, sz=2.4, a="middle", c="#111", w="normal", rot=None: o.append(
        f'<text x="{x:.2f}" y="{y:.2f}" font-family="DejaVu Sans, sans-serif" font-size="{sz}" text-anchor="{a}" '
        f'fill="{c}" font-weight="{w}"' + (f' transform="rotate({rot} {x:.2f} {y:.2f})"' if rot else "") + f'>{t}</text>')
    T(width / 2, 9, "PG-1 COMPLETE WIRING: every part, every lug, every wire", 5.5, w="bold")
    T(width / 2, 15.5, "Each wire has its own lane and its number at both ends (same number = same wire: WIRE-LIST.md says what "
      "it is and how). Black = ground. Grey lugs = leave empty. Drawn from the wiring side of each part.", 2.7)
    o.append(f'<rect x="{sx0-6}" y="{seed_y}" width="{sp*39+12}" height="16" rx="2" fill="#1d1d1d"/>')
    T(sx0 - 8, seed_y + 7, "Seed3 socket", 3.2, "end", w="bold")
    T(sx0 - 8, seed_y + 11, "(place = Seed3 pin number)", 2.2, "end", "#555")
    used = {w[0] for w in WIRES} | {w[1] for w in WIRES}
    for i in range(1, 41):
        on = f"seed.{i}" in used
        for yy in (seed_y, seed_y + 16):
            o.append(f'<circle cx="{seed_x[i]:.2f}" cy="{yy:.2f}" r="1.1" fill="{"#d4a017" if on else "#777"}"/>')
        T(seed_x[i], seed_y + 7, str(i), 2.3, c="#fff", w="bold")
        T(seed_x[i], seed_y + 11.5, SEED_NAMES[i], 1.6, c="#bbb")
    for pid, title, sub, pins in PARTS:
        x0, w = place[pid]
        low = pid in LOWER
        by = lo_y if low else top_y
        o.append(f'<rect x="{x0:.2f}" y="{by:.2f}" width="{w:.2f}" height="{PH}" rx="2" fill="#f2f2f0" stroke="#333" stroke-width="0.35"/>')
        for pn in pins:
            key = f"{pid}.{pn}"
            px, py = pin_xy[key]
            o.append(f'<rect x="{px-1.0:.2f}" y="{py-1.0:.2f}" width="2.0" height="2.0" fill="{"#b0b0b0" if key in UNUSED else "#c9a227"}" stroke="#555" stroke-width="0.2"/>')
            if low:
                T(px + 0.65, py + 2.6, pn.strip(), 1.8, "end", "#999" if key in UNUSED else "#222", rot=-90)
            else:
                T(px + 0.65, py - 2.6, pn.strip(), 1.8, "start", "#999" if key in UNUSED else "#222", rot=-90)
        ty = by + (PH - 8 if low else 5)
        T(x0 + w / 2, ty, title, 2.5, w="bold")
        T(x0 + w / 2, ty + 3.6, sub, 1.8, c="#555")
        cuts = {"pw": [5], "pcf": [4, 12]}.get(pid, [])
        for c in cuts:
            a_ = pin_xy[f"{pid}.{pins[c-1]}"][0]; b_ = pin_xy[f"{pid}.{pins[c]}"][0]
            o.append(f'<path d="M{(a_+b_)/2:.2f},{by+10 if not low else by+1} v18" stroke="#999" stroke-width="0.3" stroke-dasharray="1 0.7"/>')
    rows = []
    tags = {}
    numbered = up_w + lo_w
    for n, (a, b, col, what) in enumerate(numbered, 1):
        low = (a, b, col, what) in lo_w
        k = (lo_w.index((a, b, col, what)) + 1) if low else (up_w.index((a, b, col, what)) + 1)
        ly = (lo_lane0 if low else up_lane0) + lane_step * k
        def end(e):
            if e.startswith("seed."):
                i = int(e.split(".")[1]); return (seed_x[i], seed_y + 16 if low else seed_y)
            return pin_xy[e]
        (ax, ay), (bx, by_) = end(a), end(b)
        c = COL[col]
        o.append(f'<path d="M{ax:.2f},{ay:.2f} V{ly:.2f} H{bx:.2f} V{by_:.2f}" fill="none" stroke="{c}" stroke-width="0.5" stroke-linejoin="round"/>')
        for (px, py) in ((ax, ay), (bx, by_)):
            o.append(f'<circle cx="{px:.2f}" cy="{py:.2f}" r="0.65" fill="{c}"/>')
            # tag on the lane side of the pin; neighbouring pins alternate 2 heights, more wires on one pin stack further
            seat = (round(px, 1), round(py, 1))
            k = (0 if py in (seed_y, seed_y + 16) else int(round(px / P)) % 2) + 2 * tags.get(seat, 0)
            tags[seat] = tags.get(seat, 0) + 1
            ty = py + 3.6 + 3.0 * k if ly > py else py - 1.6 - 3.0 * k
            o.append(f'<rect x="{px-2.0:.2f}" y="{ty-2.0:.2f}" width="4.0" height="2.6" rx="0.6" fill="#fff" stroke="{c}" stroke-width="0.25"/>')
            T(px, ty, str(n), 1.8, c=c, w="bold")
        rows.append((n, a, b, col, what))
    # legend
    lx, ly = 10.0, height - 26
    for i, (k, lab) in enumerate([("pwr", "power (9 V / 3.3 V)"), ("gnd", "ground"), ("i2c", "I2C"), ("sai", "digital audio"),
                                  ("pot", "gain pots"), ("sw", "switches"), ("enc", "encoders"), ("lcd", "screen"),
                                  ("led", "level lights")]):
        o.append(f'<path d="M{lx+i*42:.2f},{ly} h10" stroke="{COL[k]}" stroke-width="1.2"/>')
        T(lx + i * 42 + 12, ly + 1, lab, 2.6, "start")
    notes = ["Not drawn (on the carrier board already): the in / out jacks, the leveller's 2 light pairs OC1 / OC2 (BUILD-GUIDE 6c). "
             "fx loop header (2 x 4): leave it EMPTY.",
             "Seed3 socket places 16-21 are not used (16 = the key: snip the Seed3's pin 16, block place 16). "
             "Grey lugs: leave empty. Wires marked 'join' are bare offcuts between neighbouring lugs (see WIRE-LIST.md).",
             "The jack's + is its SLEEVE lug (centre-negative). Find the pot's pin 1 / 3 and the switches' lugs with the multimeter "
             "(BUILD-GUIDE 5, 6b, 6b2) before soldering."]
    for i, t in enumerate(notes):
        T(10, ly + 8 + i * 4.4, t, 2.6, "start")
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.0f}mm" height="{height:.0f}mm" viewBox="0 0 {width:.1f} {height:.1f}">'
           f'<rect width="{width:.1f}" height="{height:.1f}" fill="#fff"/>' + "\n".join(o) + "</svg>\n")
    os.makedirs(OUT, exist_ok=True)
    sp_ = os.path.join(OUT, "wiring-diagram.svg")
    open(sp_, "w").write(svg)
    subprocess.run(["rsvg-convert", "-z", "6", "-o", os.path.join(OUT, "wiring-diagram.png"), sp_], check=True)
    subprocess.run(["rsvg-convert", "-f", "pdf", "-o", os.path.join(OUT, "wiring-diagram.pdf"), sp_], check=True)
    # the table
    name = {p[0]: p[1] for p in PARTS}
    fmt = lambda e: (f"Seed3 socket {e.split('.')[1]}" if e.startswith("seed.") else f"{name[e.split('.')[0]]}: **{e.split('.', 1)[1].strip()}**")
    md = ["# PG-1 wire list", "",
          "Generated by `_Tools/pg_wiring.py` with `wiring-diagram.png` (the same numbers). Tick each one off as you go.", "",
          "| # | from | to | colour | how |", "|---|---|---|---|---|"]
    for n, a, b, col, what in rows:
        md.append(f"| {n} | {fmt(a)} | {fmt(b)} | {col} | {what} |")
    md += ["", "Leave empty: " + ", ".join(f"{fmt(k)} ({v})" for k, v in UNUSED.items()) + ".", ""]
    open(os.path.join(OUT, "WIRE-LIST.md"), "w").write("\n".join(md))
    print(f"wiring: {len(WIRES)} wires, {len(PARTS)} parts, {width:.0f} x {height:.0f} mm")


if __name__ == "__main__":
    main()
