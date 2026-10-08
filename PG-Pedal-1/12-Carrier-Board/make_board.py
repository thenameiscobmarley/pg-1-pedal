#!/usr/bin/env python3
"""Builds the carrier board from carrier.py and writes the PCBWay files.

    python3 make_board.py            # place, autoroute, pour, DRC, export (needs KiCad 10 + Java + Freerouting)
    python3 make_board.py --seed 7   # another placement try

1. the jacks and headers go where the enclosure needs them (face coordinates: x right, y up, 0 = face centre, mm)
2. every other part goes on the BOTTOM (lid side; the jack side is full of jack bodies), placed by simulated
   annealing: short wires, no overlaps, decoupling caps next to their chips
3. Freerouting routes it (Specctra DSN -> SES), an AGND pour goes on both layers, kicad-cli runs the DRC
4. pcbway/: Gerbers + drill (zipped), BOM, pick-and-place (CPL), and pictures of both sides
"""
import argparse, csv, math, os, random, shutil, subprocess, sys, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pcbnew as K   # noqa: E402
from carrier import parts, nets, ISOLATED   # noqa: E402

BUILD, OUT = os.path.join(HERE, "build"), os.path.join(HERE, "pcbway")
FPLIB, LOCAL = "/usr/share/kicad/footprints", os.path.join(HERE, "PG1.pretty")
JAVA = shutil.which("java") or "/usr/lib/jvm/default/bin/java"
FREEROUTING = os.path.expanduser("~/.local/opt/freerouting/freerouting.jar")
NAME = "pg1-carrier"

# ---------------------------------------------------------------- the enclosure (same numbers as _Tools/pg_generate.py)
FACE_H, WALL = 145.5, 2.3
WALL_IN = FACE_H / 2 - WALL            # inside of the top wall: 70.45
TOP = WALL_IN - 0.25                   # board edge just short of the wall
# board = a strip under the jacks + a tongue over the screen's top (clear of the pots on the left, the screen's
# 14-pin header and the Seed3 block on the right)
STRIP = (-55.5, 43.0, 55.5, TOP)       # x0, y0, x1, y1
TONGUE = (-34.0, 12.0, 30.0, TOP)
# the top corners are notched round the box's lid-screw posts (Hammond 1590XX: centred at +-51 / +-63, up to ~5.5 mm
# round): nothing of the board is above y 57 outside +-45
POST_X, POST_Y = 45.0, 57.0
OUTLINE = [(-POST_X, TOP), (POST_X, TOP), (POST_X, POST_Y), (55.5, POST_Y), (55.5, 43.0), (30.0, 43.0), (30.0, 12.0),
           (-34.0, 12.0), (-34.0, 43.0), (-55.5, 43.0), (-55.5, POST_Y), (-POST_X, POST_Y)]
# TWO WORLDS: the ISOLATED side (both jacks, codec, input buffer; ground IGND) is the strip under the jacks; the PEDAL
# side (9 V, regulators, knob reader; ground GND) is the tongue below it. Between them runs a strip with no copper at
# all (BARRIER), crossed only by the three barrier parts (isolated power, I2S isolator, I2C isolator).
BARRIER = (29.05, 29.95)               # y range of the no-copper strip
DOMAIN_RECTS = {"iso": [(-POST_X, STRIP[1], POST_X, TOP), (STRIP[0], STRIP[1], STRIP[2], POST_Y),
                        (TONGUE[0], BARRIER[1] + 0.4, TONGUE[2], TOP)],
                "ped": [(TONGUE[0], TONGUE[1], TONGUE[2], BARRIER[0] - 0.4)]}
BARRIER_PARTS = {"U4": -1.5, "U5": 8.5, "U3": 21.0}   # U3: its body may sit either side of its pins (5.6 mm)   # x of each, centred on the barrier
# On the isolated side: INPUT on the left (as you look at the pedal's face), OUTPUT on the right, the codec between them.
DIVIDE = 0.0
# every part belongs to one job, and the parts of a job sit together in a printed box (title on top);
# arrows between the boxes show the signal's path. (name, x range it may use, where it starts, parts)
GROUPS = [   # (name, the area(s) its parts stay in: x0, y0, x1, y1 incl. its title, where it starts, parts)
    # isolated side: the strip between the jack bodies + the tongue above the barrier. input | codec | output
    ("input", [(-23.4, 46.6, -9.6, 69.9), (-33.5, 30.6, -9.6, 46.0)], (-20.0, 42.0),
     "R30 R31 R32 R33 R34 R35 R36 R37 C30 C31 C32 C33 R40 R41 R42 R43 R44 R45 R46 R47 C40 C41 C42 C43 U8 C26 U11 C27 D61"),
    ("codec", [(-8.4, 48.6, 5.4, 69.9)], (0.0, 58.0), "U9 C18 C19 C20 C21 C22 C23 C24 C25 R8 R9 R10 R11 R12 R13 R14"),
    # the DSP-set analog effect (a quad digital pot: level + filter per side, and its buffer) between the codec and the output stage
    ("analog fx", [(8.4, 36.4, 29.8, 47.1)], (19.5, 41.5), "U13 U15 C74 C75 C76 C78"),
    ("output", [(6.0, 47.7, 23.4, 69.9)], (15.0, 58.0),
     "C50 C51 C52 R50 R51 R52 C60 C61 C62 R60 R61 R62 U12 C28 U10 C70 C71 C72 R70 C73 D62"),
    ("iso power", [(-9.0, 35.8, 8.0, 46.0)], (0.0, 41.0), "U7 C13 C14 C15 R5 C16 R6 R7 C17"),
    # the barrier parts (fixed) and their caps + the I2C pull-ups, both sides of it (its box is drawn round all of them)
    ("isolation", [(-12.0, 30.35, 29.5, 33.2), (9.6, 30.35, 29.5, 35.0), (-6.0, 22.6, 29.5, 28.65)], (6.0, 29.5),
     "U3 U4 U5 C7 C8 C9 C10 C11 C12 R3 R4"),
    # pedal side (the tongue below the barrier)
    ("power", [(-33.5, 12.2, -9.0, 28.4), (-33.5, 21.0, -2.8, 27.2)], (-21.0, 21.0), "F1 D1 D2 C1 U1 C2 U2 C3 C5"),
]
GROUP_OF = {r: g for g, _, _, rs in GROUPS for r in rs.split()}
# the signal's path, drawn as arrows: (from, to, both ways); a name is a group, a J-number a jack or header
FLOW = [("J1", "input", False), ("input", "codec", False), ("codec", "analog fx", False), ("analog fx", "output", False), ("output", "J2", False),
        ("codec", "isolation", True), ("isolation", "J10", True), ("output", "J19", True), ("input", "J20", True),
        ("J10", "power", False), ("J21", "power", True)]
BOX_M, BOX_TITLE = 0.75, 1.5            # printed box: this far round the parts, plus room for its title on top
FONT = "IBM Plex Mono"                 # the pedal's own small-word font (installed to ~/.local/share/fonts)
# Neutrik NMJ6HFD2 (official drawing ST-NMJ6HFD2, 26.02.2021): body front on the inside of the wall, pins 4.35 / 10.7 /
# 17.05 mm in from it, rows 16.23 apart (8.115 each side of the axis), axis 8.29 mm above the board, 24 mm long behind
# the panel, panel hole 11.2 mm. KiCad's footprint has its origin on the T pin and the front toward +x: turned 90 deg.
JACK_T = (-8.115, 17.05)               # T pin from (jack axis, inside of the wall)
JACKS = {"J1": -34.0, "J2": 34.0}   # clear of the corner posts (see POST_X)
# the panel DC jack (Tayda A-2237, ~12 mm threaded body, ~18 mm deep with its lugs) hangs over the board at x -13:
# its body is only ~2.4 mm above the board, so under it only flat parts (0402 / 0603, <= 0.6 mm) may sit
DC_ZONE = (-13.0 - 7.0, WALL_IN - 19.0, -13.0 + 7.0, TOP)
FLAT = ("R_0402", "C_0402", "R_0603", "C_0603")
# headers: (first pin x, row y, side). Right-angle, pointing off the lower edge (the jumpers lie flat).
HEADERS = {"J10": (3.5, 14.0, "F"),                                # pedal side, tongue edge
           "J21": (-7.4, 14.0, "F"),    # pedal side, left of J10: to the expansion bay in the left wall (jumper wires)
           "J20": (-53.5, 44.2, "F"),   # isolated side, under the in jack, next to the pots: pg-line (the sensitive one)
           "J19": (40.8, 44.2, "F")}    # isolated side, under the out jack: pg-hp
HEADER_PINS = {   # printed beside each pin
    "J10": ["dc +", "dc -", "vin", "gnd", "scl", "sda", "sck", "fs", "tx", "rx"],
    "J19": ["1 l", "2 l", "3 l", "1 r", "2 r", "3 r"],
    "J20": ["1 l", "2 l", "3 l", "1 r", "2 r", "3 r"],
    "J21": ["3v3", "gnd", "scl", "sda"],
}
HEADER_TITLE = {"J10": "seed3 + 9v (IN)", "J19": "pg-hp", "J20": "pg-line", "J21": "expansion"}
JACK_NAME = {"J1": "in (IN)", "J2": "out (OUT)"}
JACK_NAME_X = {"J1": -36.6, "J2": 51.2}   # moved off the pot headers that sit under the jacks

TRACK, CLEAR, VIA, DRILL = 0.2, 0.15, 0.6, 0.3  # 0.2 mm tracks: these currents are all < 0.15 A
HALO = {"U9": 1.1}                     # extra room round fine-pitch chips: the tracks must get out of every pin
GAP = 0.3                             # extra room around each part's courtyard (space for tracks and vias)
EDGE = 0.2                             # courtyard to board edge / the barrier (the job areas keep the boxes in)
NET_W = {"IGND": 0.12, "GND": 0.12, "ISO3V3": 0.5, "+3V3": 0.5, "+5V": 0.6, "+9V": 0.6, "IBIAS": 0.7}
STICK = {"C26": "U8", "C24": "U9", "C23": "U9", "C19": "U9", "C20": "U9", "C21": "U9", "C22": "U9", "C18": "U9",
         "C4": "U6", "C9": "U4", "C10": "U4", "C11": "U5", "C12": "U5", "C13": "U7", "C14": "U7", "C15": "U7",
         "C2": "U1", "C3": "U2", "C7": "U3", "C8": "U3", "C70": "U10", "C71": "U10", "C72": "U10",
         "C75": "U13", "C78": "U15"}   # keep these right at their chip


def kpt(x, y):
    return K.VECTOR2I(K.FromMM(x + 100.0), K.FromMM(100.0 - y))


def face(v):
    return K.ToMM(v.x) - 100.0, 100.0 - K.ToMM(v.y)


def load(fpid):
    lib, name = fpid.split(":")
    fp = K.FootprintLoad(LOCAL if lib == "PG1" else f"{FPLIB}/{lib}.pretty", name)
    if fp is None:
        sys.exit(f"footprint not found: {fpid}")
    return fp


# ---------------------------------------------------------------- the board, the footprints, the nets
def make_board():
    b = K.BOARD()
    b.SetCopperLayerCount(2)
    ds = b.GetDesignSettings()
    nc = ds.m_NetSettings.GetDefaultNetclass()
    nc.SetTrackWidth(K.FromMM(TRACK)), nc.SetClearance(K.FromMM(CLEAR))
    nc.SetViaDiameter(K.FromMM(VIA)), nc.SetViaDrill(K.FromMM(DRILL))
    ds.m_CopperEdgeClearance = K.FromMM(0.3)
    ds.SetAuxOrigin(kpt(STRIP[0], TONGUE[1]))   # the board's lower-left corner = 0,0 in the placement file
    ds.m_TrackMinWidth = K.FromMM(0.127)   # JLCPCB 2-layer standard: 5 mil
    for i, (x, y) in enumerate(OUTLINE):
        s = K.PCB_SHAPE(b)
        s.SetShape(K.SHAPE_T_SEGMENT), s.SetLayer(K.Edge_Cuts), s.SetWidth(K.FromMM(0.1))
        s.SetStart(kpt(x, y)), s.SetEnd(kpt(*OUTLINE[(i + 1) % len(OUTLINE)]))
        b.Add(s)
    netinfo = {}
    for n in nets():
        ni = K.NETINFO_ITEM(b, n)
        b.Add(ni)
        netinfo[n] = ni
    fps, where = {}, {c: n for n, cs in nets().items() for c in cs}
    for ref, value, fpid, mpn, pins, note in parts:
        fp = load(fpid)
        fp.SetReference(ref), fp.SetValue(value)
        fp.Reference().SetVisible(False), fp.Value().SetVisible(False)
        b.Add(fp)
        for pad in fp.Pads():
            pin = pad.GetNumber()
            net = where.get((ref, pin))
            if net:
                pad.SetNet(netinfo[net])
        fps[ref] = fp
    return b, fps


def put(fp, x, y, rot, side):
    fp.SetPosition(kpt(0, 0))
    if side == "B" and not fp.IsFlipped():
        fp.Flip(fp.GetPosition(), K.FLIP_DIRECTION_LEFT_RIGHT)
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(kpt(x, y))


def geometry(fp, side):
    """courtyard box and pad offsets (face coords, relative to the footprint position) for rotations 0/90/180/270"""
    out = {}
    for rot in (0, 90, 180, 270):
        put(fp, 0, 0, rot, side)
        fp.BuildCourtyardCaches()
        cy = fp.GetCourtyard(K.B_CrtYd if side == "B" else K.F_CrtYd)
        bb = cy.BBox() if cy.OutlineCount() else fp.GetBoundingBox(False)
        x0, y1 = face(bb.GetOrigin())
        x1, y0 = face(bb.GetEnd())
        out[rot] = ((x0, y0, x1, y1), {p.GetNumber(): face(p.GetPosition()) for p in fp.Pads()})
    return out


def fixed_rot(fp, side, want):
    """the rotation that makes a header's pins point the way we want ('down' = -y off the lower edge)"""
    for rot in (0, 90, 180, 270):
        (x0, y0, x1, y1), pads = geometry(fp, side)[rot]
        px = [p[0] for p in pads.values()]
        py = [p[1] for p in pads.values()]
        along_x = max(px) - min(px) > max(py) - min(py) or len(pads) == 1
        if want == "down" and along_x and (y0 + y1) / 2 < min(py) - 0.5:
            return rot
        if want == "up" and along_x and first_left:
            return rot
    sys.exit(f"no rotation for {fp.GetReference()}")


# ---------------------------------------------------------------- placement (simulated annealing)
class Placer:
    CELL = 4.0

    def __init__(self, geo, fixed_pads, obstacles, netlist, rng):
        self.geo, self.rng = geo, rng
        self.side = {r: "iso" if any(n in ISOLATED for n, cs in netlist.items() if any(c[0] == r for c in cs)) else "ped"
                     for r in geo}
        self.areas = {r: rects for g, rects, _, rs in GROUPS for r in rs.split()}
        self.tall = set()   # parts too tall to sit under the DC jack (filled in by main)
        self.members = {g: [r for r in rs.split() if r in geo] for g, _, _, rs in GROUPS}
        self.target = {g: t for g, _, t, _ in GROUPS}
        self.gb, self.fov, self.extra = {}, {}, {}
        self.fixed_boxes = []   # header names, jack names: the job boxes keep off them
        self.ow = 2.0           # weight of box overlap (the job areas already keep the boxes apart)
        self.refs = list(geo)
        self.pos, self.rot = {}, {}
        self.grid = {}
        self.fixed_pads = fixed_pads
        self.nets = {n: cs for n, cs in netlist.items() if any(r in geo for r, _ in cs)}
        self.of = {r: [n for n, cs in self.nets.items() if any(c[0] == r for c in cs)] for r in self.refs}
        for i, ob in enumerate(obstacles):
            self._add(("ob", i), ob)
        self.box = {("ob", i): ob for i, ob in enumerate(obstacles)}

    # --- geometry
    def boxat(self, r, x, y, rot):
        b, g = self.geo[r][rot][0], GAP + HALO.get(r, 0.0)
        return (x + b[0] - g, y + b[1] - g, x + b[2] + g, y + b[3] + g)

    def inside(self, b, r):
        g = GROUP_OF[r]
        m = BOX_M + 0.15 - GAP if g != "isolation" else -GAP   # the job's printed box must fit in its area too
        t = BOX_TITLE if g != "isolation" else 0.0
        if not any(b[0] >= x0 + m and b[1] >= y0 + m and b[2] <= x1 - m and b[3] <= y1 - m - t
                   for x0, y0, x1, y1 in self.areas[r]):
            return False
        return any(b[0] >= x0 + EDGE - GAP and b[1] >= y0 + EDGE - GAP and b[2] <= x1 - EDGE + GAP and b[3] <= y1 - EDGE + GAP
                   for x0, y0, x1, y1 in DOMAIN_RECTS[self.side[r]])

    def cells(self, b):
        c = self.CELL
        return [(i, j) for i in range(math.floor(b[0] / c), math.floor(b[2] / c) + 1)
                for j in range(math.floor(b[1] / c), math.floor(b[3] / c) + 1)]

    def _add(self, key, b):
        for c in self.cells(b):
            self.grid.setdefault(c, set()).add(key)

    def _rm(self, key, b):
        for c in self.cells(b):
            self.grid[c].discard(key)

    def free(self, b, skip=(), r=None):
        if not self.inside(b, r):
            return False
        if r is not None and r in self.tall and b[0] < DC_ZONE[2] and DC_ZONE[0] < b[2] and b[1] < DC_ZONE[3] and DC_ZONE[1] < b[3]:
            return False
        seen = set()
        for c in self.cells(b):
            for k in self.grid.get(c, ()):
                if k in skip or k in seen:
                    continue
                seen.add(k)
                o = self.box[k]
                if b[0] < o[2] and o[0] < b[2] and b[1] < o[3] and o[1] < b[3]:
                    return False
        return True

    def set(self, r, x, y, rot):
        if r in self.pos:
            self._rm(r, self.box[r])
        self.pos[r], self.rot[r] = (x, y), rot
        self.box[r] = self.boxat(r, x, y, rot)
        self._add(r, self.box[r])
        self.gb.pop(GROUP_OF.get(r), None)
        self.fov.pop(GROUP_OF.get(r), None)

    # --- cost
    def pin(self, ref, pin):
        if ref in self.pos:
            x, y = self.pos[ref]
            dx, dy = self.geo[ref][self.rot[ref]][1][pin]
            return x + dx, y + dy
        return self.fixed_pads.get((ref, pin))

    def net_cost(self, n):
        pts = [p for p in (self.pin(r, q) for r, q in self.nets[n]) if p]
        if len(pts) < 2:
            return 0.0
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        return NET_W.get(n, 1.0) * (max(xs) - min(xs) + max(ys) - min(ys))

    def stick_cost(self, r):
        """a decoupling cap: its distance to the chip's pin it serves (the non-ground net they share)"""
        other = STICK.get(r)
        if not other or r not in self.pos:
            return 0.0
        x, y = self.pos[r]
        pins = [self.pin(o, q) for n in self.of[r] if n not in ("GND", "IGND")
                for o, q in self.nets[n] if o == other]
        pins = [p for p in pins if p]
        if not pins:
            return 0.0
        return 2.0 * min(abs(x - u) + abs(y - v) for u, v in pins)

    def part_cost(self, rs):
        ns = set(n for r in rs for n in self.of[r])
        st = set(rs) | {k for k, v in STICK.items() if v in rs}
        return sum(self.net_cost(n) for n in ns) + sum(self.stick_cost(s) for s in st) + self.group_cost()

    def total(self):
        return sum(self.net_cost(n) for n in self.nets) + sum(self.stick_cost(s) for s in STICK) + self.group_cost()

    # --- the job boxes: small, apart, and next to the box they feed
    def gbox(self, g):
        if g not in self.gb:
            bs = [self.box[r] for r in self.members[g] if r in self.box] + self.extra.get(g, [])
            if not bs:
                return None
            self.gb[g] = (min(b[0] for b in bs) - BOX_M, min(b[1] for b in bs) - BOX_M,
                          max(b[2] for b in bs) + BOX_M, max(b[3] for b in bs) + BOX_M + BOX_TITLE)
        return self.gb[g]

    @staticmethod
    def _ov(a, b):
        w, h = min(a[2], b[2]) - max(a[0], b[0]) + 0.6, min(a[3], b[3]) - max(a[1], b[1]) + 0.6
        return w * h if w > 0 and h > 0 else 0.0

    def overlap(self):
        gs = [(g, self.gbox(g)) for g in self.members]
        gs = [(g, b) for g, b in gs if b]
        tot = 0.0
        for i in range(len(gs)):
            g, a = gs[i]
            if g not in self.fov:   # against the fixed names: only redone when this box moved
                self.fov[g] = sum(self._ov(a, f) for f in self.fixed_boxes)
            tot += self.fov[g]
            for j in range(i + 1, len(gs)):
                tot += self._ov(a, gs[j][1])
        return tot

    def group_cost(self):
        c = 0.0
        for g in self.members:
            b = self.gbox(g)
            if b:
                c += 0.4 * ((b[2] - b[0]) + (b[3] - b[1]))
        for u, v, _ in FLOW:
            if u in self.members and v in self.members:
                a, b = self.gbox(u), self.gbox(v)
                if a and b:
                    c += 0.15 * (abs((a[0] + a[2]) - (b[0] + b[2])) + abs((a[1] + a[3]) - (b[1] + b[3]))) / 2
        return c + self.ow * self.overlap()

    # --- first placement: each part as close as it fits to what it's wired to
    def seed(self, order):
        xs = [x / 4 for x in range(int(STRIP[0] * 4), int(STRIP[2] * 4) + 1)]
        ys = [y / 4 for y in range(int(TONGUE[1] * 4), int(STRIP[3] * 4) + 1)]
        for r in order:
            anchors = [self.pin(rr, q) for n in self.of[r] if NET_W.get(n, 1) >= 1 or r in STICK
                       for rr, q in self.nets[n] if rr != r]
            anchors = [self.target[GROUP_OF[r]]] * 3 + [a for a in anchors if a][:3]
            if r in STICK and STICK[r] in self.pos:
                anchors = [self.pos[STICK[r]]]
            tx = sum(a[0] for a in anchors) / len(anchors)
            ty = sum(a[1] for a in anchors) / len(anchors)
            for x, y in sorted(((x, y) for x in xs for y in ys), key=lambda p: (p[0] - tx) ** 2 + (p[1] - ty) ** 2):
                rot = next((q for q in (0, 90, 180, 270) if self.free(self.boxat(r, x, y, q), r=r)), None)
                if rot is not None:
                    self.set(r, x, y, rot)
                    break
            else:
                sys.exit(f"{r} doesn't fit on the board ({GROUP_OF[r]}: " + ", ".join(f"{q} {tuple(round(v, 1) for v in self.box[q])}" for q in self.members[GROUP_OF[r]] if q in self.pos) + ") obstacles: " + str([tuple(round(v, 1) for v in o) for k, o in self.box.items() if k[0] == "ob" and o[0] < self.areas[r][0][2] and o[2] > self.areas[r][0][0] and o[1] < self.areas[r][0][3] and o[3] > self.areas[r][0][1]]))

    def anneal(self, moves=120000):
        rng = self.rng
        cost = self.total()
        t0, t1 = 4.0, 0.02
        sizes = {r: round((self.geo[r][0][0][2] - self.geo[r][0][0][0]) * (self.geo[r][0][0][3] - self.geo[r][0][0][1]), 0)
                 for r in self.refs}
        for k in range(moves):
            t = t0 * (t1 / t0) ** (k / moves)
            if k % 2000 == 0:
                self.ow = 2.0   # the areas keep the boxes apart; this only tidies the edges
                self.gb.clear(), self.fov.clear()
            r = rng.choice(self.refs)
            x, y = self.pos[r]
            rot = self.rot[r]
            if rng.random() < 0.2:   # swap with a same-size part
                s = rng.choice(self.refs)
                if s == r or sizes[s] != sizes[r] or GROUP_OF[s] != GROUP_OF[r]:
                    continue
                before = self.part_cost([r, s])
                (u, v), rs = self.pos[s], self.rot[s]
                b1, b2 = self.boxat(r, u, v, rs), self.boxat(s, x, y, rot)
                ov = b1[0] < b2[2] and b2[0] < b1[2] and b1[1] < b2[3] and b2[1] < b1[3]
                if ov or not self.free(b1, (r, s), r) or not self.free(b2, (r, s), s):
                    continue
                self.set(r, u, v, rs), self.set(s, x, y, rot)
                d = self.part_cost([r, s]) - before
                if d > 0 and rng.random() > math.exp(-d / t):
                    self.set(r, x, y, rot), self.set(s, u, v, rs)
                else:
                    cost += d
                continue
            if rng.random() < 0.12:   # move a whole job box
                g = GROUP_OF[r]
                ms = self.members[g]
                step = 0.5 + 10.0 * t / t0
                dx, dy = round(rng.gauss(0, step) * 4) / 4, round(rng.gauss(0, step) * 4) / 4
                old = {m: (self.pos[m], self.rot[m]) for m in ms}
                nb = {m: self.boxat(m, old[m][0][0] + dx, old[m][0][1] + dy, old[m][1]) for m in ms}
                if not all(self.free(nb[m], tuple(ms), m) for m in ms):
                    continue
                before = self.part_cost(ms)
                for m in ms:
                    self.set(m, old[m][0][0] + dx, old[m][0][1] + dy, old[m][1])
                d = self.part_cost(ms) - before
                if d > 0 and rng.random() > math.exp(-d / t):
                    for m in ms:
                        self.set(m, old[m][0][0], old[m][0][1], old[m][1])
                else:
                    cost += d
                continue
            if rng.random() < 0.15:
                nx, ny, nr = x, y, rng.choice((0, 90, 180, 270))
            else:
                step = 0.5 + 12.0 * t / t0
                nx = round((x + rng.gauss(0, step)) * 4) / 4
                ny = round((y + rng.gauss(0, step)) * 4) / 4
                nr = rot
            b = self.boxat(r, nx, ny, nr)
            if not self.free(b, (r,), r):
                continue
            before = self.part_cost([r])
            self.set(r, nx, ny, nr)
            d = self.part_cost([r]) - before
            if d > 0 and rng.random() > math.exp(-d / t):
                self.set(r, x, y, rot)
            else:
                cost += d
        return self.total()


# ---------------------------------------------------------------- silk
def fbox(bb):
    """a KiCad box -> (x0, y0, x1, y1) in face coordinates"""
    (ax, ay), (bx, by) = face(bb.GetOrigin()), face(bb.GetEnd())
    return min(ax, bx), min(ay, by), max(ax, bx), max(ay, by)


def text(b, s, x, y, size=1.0, layer=K.B_SilkS, rot=0, just=0):
    t = K.PCB_TEXT(b)
    t.SetText(s), t.SetLayer(layer), t.SetPosition(kpt(x, y))
    t.SetTextSize(K.VECTOR2I(K.FromMM(size), K.FromMM(size))), t.SetTextThickness(K.FromMM(max(0.16, size * 0.15)))   # JLCPCB prints 0.153 mm lines at the least
    t.SetTextAngleDegrees(rot)
    if just:
        t.SetHorizJustify(K.GR_TEXT_H_ALIGN_LEFT if just < 0 else K.GR_TEXT_H_ALIGN_RIGHT)
    if layer == K.B_SilkS:
        t.SetMirrored(True)
    b.Add(t)
    return t


def diamond(b, x, y, r, layer, filled=False):
    s = K.PCB_SHAPE(b)
    s.SetShape(K.SHAPE_T_POLY), s.SetLayer(layer), s.SetWidth(K.FromMM(0.15)), s.SetFilled(filled)
    s.SetPolyPoints([kpt(x, y + r), kpt(x + r, y), kpt(x, y - r), kpt(x - r, y)])
    b.Add(s)


def line(b, x0, y0, x1, y1, layer=K.F_SilkS, w=0.15):
    s = K.PCB_SHAPE(b)
    s.SetShape(K.SHAPE_T_SEGMENT), s.SetLayer(layer), s.SetWidth(K.FromMM(w))
    s.SetStart(kpt(x0, y0)), s.SetEnd(kpt(x1, y1))
    b.Add(s)


def logo_points(cx, cy, k=1.0, mirror=False):
    """the pg logo (same curve as the face print, _Tools/pg_generate.py): flat line, growing wave, one loop, flat line"""
    ctrl = [(-25, 0), (-19, 0)]
    for i in range(1, 13):
        t = i / 12
        ctrl.append((-19 + 17.5 * t, (1 if i % 2 else -1) * (0.5 + 2.4 * t ** 1.2)))
    ctrl += [(0.6, 0.2), (2.6, 3.4), (3.9, 6.4), (3.1, 8.6), (1.1, 9.1), (-0.4, 7.4),
             (0.2, 4.6), (2.6, 1.7), (5.6, 0.25), (9.0, 0), (25, 0)]
    pts, P = [], [ctrl[0]] + ctrl + [ctrl[-1]]
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for n in range(8):
            t = n / 8
            pts.append(tuple(0.5 * (2 * p1[j] + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t * t
                                    + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t ** 3) for j in (0, 1)))
    pts.append(ctrl[-1])
    return [(cx + (-x if mirror else x) * k, cy + y * k) for x, y in pts]


def artwork(b):
    """the fixed printing on the parts side: each jack's name with (IN) or (OUT), small IN / OUT at the top of the
    divider, and which side of the barrier is which. The pg logo goes in a corner of the other side. Returns the texts
    (the parts keep off them) and the jack names' boxes (the arrows start / end there)."""
    F = K.F_SilkS
    # each jack's name up the board's side edge, beside it (its pins and the pot headers keep the rest busy)
    out = [text(b, JACK_NAME[r], STRIP[0] + 1.6 if x < 0 else STRIP[2] - 1.6, 51.5, 0.9, F, 90) for r, x in JACKS.items()]
    labels = {r: fbox(t.GetBoundingBox()) for r, t in zip(JACKS, out)}
    out += [text(b, "IN", DIVIDE - 2.2, TOP - 1.3, 1.0, F),
            text(b, "OUT", DIVIDE + 2.6, TOP - 1.3, 1.0, F),
            text(b, "isolated", TONGUE[0] + 3.4, BARRIER[1] + 1.0, 0.8, F),
            text(b, "pedal", TONGUE[0] + 3.0, BARRIER[0] - 1.0, 0.8, F)]
    pts = logo_points(-18.0, 16.5, 0.46, mirror=True)   # on the back of the pedal side; read from the lid side: mirrored
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        line(b, x0, y0, x1, y1, K.B_SilkS, 0.3)
    return out, labels


def barrier_and_divider(b, boxes):
    """dashes along the barrier, and the IN | OUT row of diamonds up the isolated side (both step round the boxes)"""
    F = K.F_SilkS
    words = [fbox(t.GetBoundingBox()) for t in b.Drawings() if isinstance(t, K.PCB_TEXT) and t.GetLayer() == F]
    keep_off = words + list(boxes.values())
    pads = []
    for fp in b.GetFootprints():
        for p in fp.Pads():
            px, py = face(p.GetPosition())
            pads.append((px - 1.2, py - 1.2, px + 1.2, py + 1.2))
    hit = lambda x, y, m: any(w[0] - m < x < w[2] + m and w[1] - m < y < w[3] + m for w in keep_off + pads)
    y, x = sum(BARRIER) / 2, TONGUE[0] + 1.0
    while x < TONGUE[2] - 1.0:   # the barrier: short dashes
        if not hit(x, y, 0.2) and not hit(x + 0.8, y, 0.2):
            line(b, x, y, x + 0.8, y, F, 0.15)
        x += 1.6
    y = STRIP[1] + 0.8
    while y < TOP - 2.4:   # the IN | OUT divider
        if not hit(DIVIDE, y, 0.5):
            diamond(b, DIVIDE, y, 0.35, F)
        y += 1.8


def route_arrow(blocked, src, dst, cell=0.5):
    """a right-angled path from box src to box dst on a 0.5 mm grid, round everything in blocked (cells), fewest
    turns first; returns its corner points"""
    import heapq
    def cells_of(r):
        return {(i, j) for i in range(math.floor(r[0] / cell), math.ceil(r[2] / cell) + 1)
                for j in range(math.floor(r[1] / cell), math.ceil(r[3] / cell) + 1)}
    sc, dc = cells_of(src), cells_of(dst)
    ring = lambda cs: {(i + di, j + dj) for i, j in cs for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1))} - cs
    starts, goals = ring(sc) - blocked, ring(dc) - blocked
    if not starts or not goals:
        return None
    gx = sum(i for i, _ in goals) / len(goals)
    gy = sum(j for _, j in goals) / len(goals)
    pq, seen, parent = [], {}, {}
    for c in starts:
        heapq.heappush(pq, (0.0, 0.0, c, None, None))
    while pq:
        f, g, c, d, par = heapq.heappop(pq)
        if seen.get((c, d), 1e9) <= g:
            continue
        seen[(c, d)] = g
        parent[(c, d)] = par
        if c in goals:
            path, k = [], (c, d)
            while k:
                path.append(k[0])
                k = parent[k]
            path.reverse()
            pts = [path[0]]
            for k in range(1, len(path) - 1):
                if (path[k][0] - path[k - 1][0], path[k][1] - path[k - 1][1]) != (path[k + 1][0] - path[k][0], path[k + 1][1] - path[k][1]):
                    pts.append(path[k])
            pts.append(path[-1])
            return [(i * cell, j * cell) for i, j in pts]
        for nd in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (c[0] + nd[0], c[1] + nd[1])
            if n in blocked or n in sc:
                continue
            ng = g + 1 + (12 if d and nd != d else 0)
            if seen.get((n, nd), 1e9) > ng:
                heapq.heappush(pq, (ng + abs(n[0] - gx) + abs(n[1] - gy), ng, n, nd, (c, d)))
    return None


def draw_groups(b, pl, nodes):
    """a thin box round each job's parts with its name on top, and the arrows of the signal's path"""
    F = K.F_SilkS
    boxes = dict(nodes)
    for g, rects, _, _ in GROUPS:
        x0, y0, x1, y1 = pl.gbox(g)
        boxes[g] = (x0, y0, x1, y1)
        # an L-shaped job (two areas) gets an L-shaped frame: one box per area, joined, inside lines left out
        subs = []
        if g != "isolation" and len(rects) > 1:
            for r in rects:
                ps = [pl.box[m] for m in pl.members[g] if m in pl.box and r[0] <= (pl.box[m][0] + pl.box[m][2]) / 2 <= r[2]
                      and r[1] <= (pl.box[m][1] + pl.box[m][3]) / 2 <= r[3]]
                if ps:
                    subs.append([min(q[0] for q in ps) - BOX_M, min(q[1] for q in ps) - BOX_M,
                                 max(q[2] for q in ps) + BOX_M, max(q[3] for q in ps) + BOX_M])
        if len(subs) == 2:
            lo, hi = sorted(subs, key=lambda q: q[1])
            mid = (lo[3] + hi[1]) / 2
            lo[3], hi[1] = mid, mid   # they meet
            if lo[2] - hi[0] < 0 or hi[2] - lo[0] < 0:
                subs = []
        if len(subs) == 2:
            def edges(q):
                return [("h", q[1], q[0], q[2]), ("h", q[3], q[0], q[2]), ("v", q[0], q[1], q[3]), ("v", q[2], q[1], q[3])]
            for i, q in enumerate(subs):
                o = subs[1 - i]
                for kind, c, a0, a1 in edges(q):
                    cut = (o[0], o[2]) if kind == "h" and o[1] <= c <= o[3] else (o[1], o[3]) if kind == "v" and o[0] <= c <= o[2] else None
                    pieces = [(a0, a1)] if not cut else [(a0, min(a1, cut[0])), (max(a0, cut[1]), a1)]
                    for p0, p1 in pieces:
                        if p1 - p0 > 0.05:
                            if kind == "h":
                                line(b, p0, c, p1, c, F, 0.12)
                            else:
                                line(b, c, p0, c, p1, F, 0.12)
            top = max(subs, key=lambda q: q[3])
            text(b, g, top[0] + 0.2, top[3] + 0.85, 0.8, F, 0, -1)
            continue
        fy = y1 - BOX_TITLE + 0.15   # the frame's top line; the name sits just above it
        for p, q in (((x0, y0), (x1, y0)), ((x1, y0), (x1, fy)), ((x1, fy), (x0, fy)), ((x0, fy), (x0, y0))):
            line(b, *p, *q, F, 0.12)
        if g == "isolation":   # its title goes inside, bottom-right: its top is under the iso power and analog fx boxes
            text(b, g, x1 - 0.2, y0 + 0.75, 0.8, F, 0, 1)
            boxes[g] = (x0, y0, x1, fy + 0.1)
        else:
            text(b, g, x0 + 0.2, fy + 0.85, 0.8, F, 0, -1)
    cell = 0.5
    def grow(r, m):
        return (r[0] - m, r[1] - m, r[2] + m, r[3] + m)
    def cells_of(r):
        return {(i, j) for i in range(math.floor(r[0] / cell), math.ceil(r[2] / cell) + 1)
                for j in range(math.floor(r[1] / cell), math.ceil(r[3] / cell) + 1)}
    # what the arrows keep off: the board's outside, every box and name, every exposed pad
    allc = cells_of((STRIP[0], TONGUE[1], STRIP[2], TOP))
    inside = (cells_of(grow((-POST_X, STRIP[1], POST_X, TOP), -0.6)) | cells_of(grow((STRIP[0], STRIP[1], STRIP[2], POST_Y), -0.6))
              | cells_of(grow(TONGUE, -0.6)))
    fixed = set(allc - inside)
    for fp in b.GetFootprints():
        if fp.GetReference() not in GROUP_OF and fp.GetReference() not in JACKS:   # headers (a jack body: only its pins)
            fp.BuildCourtyardCaches()
            fixed |= cells_of(fbox(fp.GetCourtyard(K.F_CrtYd).BBox()))
        for p in fp.Pads():
            px, py = face(p.GetPosition())
            fixed |= cells_of((px - 0.8, py - 0.8, px + 0.8, py + 0.8))
    for t in b.Drawings():
        if isinstance(t, K.PCB_TEXT) and t.GetLayer() == F:
            fixed |= cells_of(grow(fbox(t.GetBoundingBox()), 0.1))
    used = set()
    def head(xa, ya, xb, yb):   # an arrow head at (xb, yb), pointing from (xa, ya)
        n = math.hypot(xb - xa, yb - ya) or 1.0
        ux, uy = (xb - xa) / n, (yb - ya) / n
        hl = min(0.9, n * 0.4)   # short arrows get small heads (two heads must not meet)
        for sgn in (1, -1):
            c, s_ = math.cos(math.radians(25)), sgn * math.sin(math.radians(25))
            hx, hy = -(ux * c - uy * s_), -(ux * s_ + uy * c)
            line(b, xb, yb, xb + hl * hx, yb + hl * hy, F, 0.15)
    walls = [w for w in (fbox(t.GetBoundingBox()) for t in b.Drawings() if isinstance(t, K.PCB_TEXT) and t.GetLayer() == F)]
    walls += [(STRIP[0], -1e3, TONGUE[0], STRIP[1]), (TONGUE[2], -1e3, STRIP[2], STRIP[1]),   # off the board
              (STRIP[0] - 1, POST_Y, -POST_X, 1e3), (POST_X, POST_Y, STRIP[2] + 1, 1e3)]
    for fp in b.GetFootprints():   # and never across a pad
        for p in fp.Pads():
            px, py = face(p.GetPosition())
            walls.append((px - 0.9, py - 0.9, px + 0.9, py + 0.9))
    def straight(a, c):
        """a short straight arrow across the gap between two neighbouring boxes, if nothing else is in the way"""
        g = 0.25
        ox = min(a[2], c[2]) - max(a[0], c[0])
        oy = min(a[3], c[3]) - max(a[1], c[1])
        segs = []   # every straight line across the gap, middle first
        if ox > 1.0 and (a[3] <= c[1] or c[3] <= a[1]):
            lo, hi = max(a[0], c[0]) + 0.5, min(a[2], c[2]) - 0.5
            for k in range(int((hi - lo) / 0.5) + 1):
                x = (lo + hi) / 2 + (0.5 * ((k + 1) // 2)) * (1 if k % 2 else -1)
                if lo <= x <= hi:
                    segs.append((x, a[3] + g, x, c[1] - g) if a[3] <= c[1] else (x, a[1] - g, x, c[3] + g))
        elif oy > 1.0 and (a[2] <= c[0] or c[2] <= a[0]):
            lo, hi = max(a[1], c[1]) + 0.5, min(a[3], c[3]) - 0.5
            for k in range(int((hi - lo) / 0.5) + 1):
                y = (lo + hi) / 2 + (0.5 * ((k + 1) // 2)) * (1 if k % 2 else -1)
                if lo <= y <= hi:
                    segs.append((a[2] + g, y, c[0] - g, y) if a[2] <= c[0] else (a[0] - g, y, c[2] + g, y))
        for x0, y0, x1, y1 in segs:
            if math.hypot(x1 - x0, y1 - y0) < 0.6:
                continue
            lo_x, hi_x, lo_y, hi_y = min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1)
            if any(lo_x < r[2] and r[0] < hi_x and lo_y < r[3] and r[1] < hi_y
                   for r in list(boxes.values()) + walls if r is not a and r is not c):
                continue
            return [(x0, y0), (x1, y1)]
        return None
    for u, v, both in FLOW:
        pts = straight(boxes[u], boxes[v])
        if pts:
            line(b, *pts[0], *pts[1], F, 0.15)
            head(*pts[0], *pts[1])
            if both:
                head(*pts[1], *pts[0])
            continue
        blocked = (fixed | used | set().union(*(cells_of(grow(r, 0.15)) for k, r in boxes.items() if k not in (u, v) and k not in JACKS)))   # (under a jack body: only its pins block)
        blocked -= cells_of(grow(boxes[u], 0.6)) | cells_of(grow(boxes[v], 0.6))
        pts = route_arrow(blocked, grow(boxes[u], 0.3), grow(boxes[v], 0.3))
        if not pts:
            print(f"  (no room for the arrow {u} -> {v})", flush=True)
            continue
        for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
            line(b, xa, ya, xb, yb, F, 0.15)
            used |= cells_of(grow((min(xa, xb), min(ya, yb), max(xa, xb), max(ya, yb)), 0.3))
        head(*pts[-2], *pts[-1])
        if both:
            head(*pts[1], *pts[0])
    barrier_and_divider(b, boxes)


def clip_silk_at_pads(b, m=0.25):
    """no printing on solder pads: every straight printed line (box sides, arrows, dashes) is cut where it crosses one"""
    pads = []
    for fp in b.GetFootprints():
        for p in fp.Pads():
            pads.append(fbox(p.GetBoundingBox()))
    for s in [d for d in b.Drawings() if isinstance(d, K.PCB_SHAPE) and d.GetShape() == K.SHAPE_T_SEGMENT
              and d.GetLayer() == K.F_SilkS]:
        (x0, y0), (x1, y1) = face(s.GetStart()), face(s.GetEnd())
        horiz, vert = abs(y1 - y0) < 1e-6, abs(x1 - x0) < 1e-6
        if not (horiz or vert):
            continue
        a0, a1 = (min(x0, x1), max(x0, x1)) if horiz else (min(y0, y1), max(y0, y1))
        cuts = []
        for q in pads:
            lo, hi = (q[1] - m, q[3] + m) if horiz else (q[0] - m, q[2] + m)
            c = y0 if horiz else x0
            if lo < c < hi:
                cuts.append(((q[0] - m, q[2] + m) if horiz else (q[1] - m, q[3] + m)))
        if not any(c0 < a1 and a0 < c1 for c0, c1 in cuts):
            continue
        pieces, start = [], a0
        for c0, c1 in sorted(cuts):
            if c1 <= start or c0 >= a1:
                continue
            if c0 > start:
                pieces.append((start, c0))
            start = max(start, c1)
        if start < a1:
            pieces.append((start, a1))
        w = s.GetWidth()
        b.Remove(s)
        for p0, p1 in pieces:
            if p1 - p0 > 0.2:
                n = K.PCB_SHAPE(b)
                n.SetShape(K.SHAPE_T_SEGMENT), n.SetLayer(K.F_SilkS), n.SetWidth(w)
                n.SetStart(kpt(p0, y0) if horiz else kpt(x0, p0)), n.SetEnd(kpt(p1, y0) if horiz else kpt(x0, p1))
                b.Add(n)


def set_font(pcb):
    """the big words in the pedal's mono font (KiCad turns it into outlines in the Gerbers); small ones stay in KiCad's
    stroke font, whose lines are thick enough to print sharp at 1 mm"""
    import re
    s = open(pcb).read()
    s = re.sub(r"\(effects\n\t\t\t\(font\n(\t\t\t\t\(size ([\d.]+) )",
               lambda m: m.group(0) if float(m.group(2)) < 1.8 else f"(effects\n\t\t\t(font\n\t\t\t\t(face \"{FONT}\")\n{m.group(1)}", s)
    open(pcb, "w").write(s)


def edge_keepout(b, width=0.5):
    """a thin no-tracks strip inside every board edge (Freerouting only keeps half a track from the outline)"""
    n = len(OUTLINE)
    for i in range(n):
        (x0, y0), (x1, y1) = OUTLINE[i], OUTLINE[(i + 1) % n]
        dx, dy = (x1 - x0) / max(abs(x1 - x0), abs(y1 - y0)), (y1 - y0) / max(abs(x1 - x0), abs(y1 - y0))
        nx, ny = dy * width, -dx * width   # the outline runs clockwise: inside is to the right
        z = K.ZONE(b)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowTracks(True), z.SetDoNotAllowVias(True), z.SetDoNotAllowPads(False), z.SetDoNotAllowFootprints(False)
        (z.SetDoNotAllowZoneFills if hasattr(z, "SetDoNotAllowZoneFills") else z.SetDoNotAllowCopperPour)(False)
        ls = K.LSET()
        ls.AddLayer(K.F_Cu), ls.AddLayer(K.B_Cu)
        z.SetLayerSet(ls)
        ol = z.Outline()
        ol.NewOutline()
        for px, py in ((x0, y0), (x1, y1), (x1 + nx, y1 + ny), (x0 + nx, y0 + ny)):
            ol.Append(kpt(px, py))
        b.Add(z)


def clip_y(poly, y0, y1):
    """the part of a polygon between y0 and y1 (Sutherland-Hodgman, two horizontal cuts)"""
    def cut(pts, y, keep_above):
        out = []
        for i in range(len(pts)):
            (ax, ay), (bx, by) = pts[i - 1], pts[i]
            ina, inb = (ay >= y) == keep_above, (by >= y) == keep_above
            if inb:
                if not ina:
                    out.append((ax + (bx - ax) * (y - ay) / (by - ay), y))
                out.append((bx, by))
            elif ina:
                out.append((ax + (bx - ax) * (y - ay) / (by - ay), y))
        return out
    return cut(cut(poly, y0, True), y1, False)


def barrier_keepout(b):
    """the no-copper strip between the two worlds: no tracks, no vias, no pour (the barrier parts' pads are allowed)"""
    z = K.ZONE(b)
    z.SetIsRuleArea(True)
    z.SetDoNotAllowTracks(True), z.SetDoNotAllowVias(True), z.SetDoNotAllowPads(False), z.SetDoNotAllowFootprints(False)
    (z.SetDoNotAllowZoneFills if hasattr(z, "SetDoNotAllowZoneFills") else z.SetDoNotAllowCopperPour)(True)
    ls = K.LSET()
    ls.AddLayer(K.F_Cu), ls.AddLayer(K.B_Cu)
    z.SetLayerSet(ls)
    ol = z.Outline()
    ol.NewOutline()
    for px, py in ((TONGUE[0] - 1, BARRIER[0]), (TONGUE[2] + 1, BARRIER[0]), (TONGUE[2] + 1, BARRIER[1]), (TONGUE[0] - 1, BARRIER[1])):
        ol.Append(kpt(px, py))
    b.Add(z)


def zone(b, net, layer, y0=-1e3, y1=1e3):
    z = K.ZONE(b)
    z.SetLayer(layer), z.SetNet(net), z.SetIsRuleArea(False)
    ol = z.Outline()
    ol.NewOutline()
    for x, y in clip_y(OUTLINE, y0, y1):
        ol.Append(kpt(x, y))
    z.SetLocalClearance(K.FromMM(0.3)), z.SetMinThickness(K.FromMM(0.25))
    z.SetThermalReliefGap(K.FromMM(0.3)), z.SetThermalReliefSpokeWidth(K.FromMM(0.45))
    z.SetPadConnection(K.ZONE_CONNECTION_FULL)   # solid joins (pads at the barrier only have the pour on one side)
    b.Add(z)
    return z


def write_project(pcb):
    """kicad-cli's DRC takes its rules from the .kicad_pro next to the board"""
    import json
    # the check uses JLCPCB's real limits (5 mil); the router aims wider (CLEAR) everywhere it can
    rules = {"min_track_width": 0.127, "min_clearance": 0.127, "min_copper_edge_clearance": 0.3,
             "min_via_diameter": VIA, "min_through_hole_diameter": DRILL, "min_text_height": 0.7}
    nc = {"name": "Default", "track_width": TRACK, "clearance": 0.127, "via_diameter": VIA, "via_drill": DRILL}
    json.dump({"board": {"design_settings": {"rules": rules}}, "net_settings": {"classes": [nc], "meta": {"version": 3}},
               "meta": {"filename": os.path.basename(pcb)[:-10] + ".kicad_pro", "version": 1}},
              open(pcb[:-10] + ".kicad_pro", "w"), indent=2)


def run(cmd, **kw):
    print("  $", " ".join(os.path.basename(c) if i == 0 else c for i, c in enumerate(cmd)), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--moves", type=int, default=150000)
    ap.add_argument("--passes", type=int, default=40)
    ap.add_argument("--place-only", action="store_true", help="stop after placing (quick look: build/placed.png)")
    a = ap.parse_args()
    os.makedirs(BUILD, exist_ok=True), os.makedirs(OUT, exist_ok=True)
    b, fps = make_board()

    # 1. fixed parts: the jacks (their bodies), the headers (their pin names)
    obstacles, fixed_pads, nodes = [], {}, {}
    for ref, x in JACKS.items():
        fp = fps[ref]
        put(fp, x + JACK_T[0], WALL_IN - JACK_T[1], 90, "F")
        for g in fp.GraphicalItems():   # its outline runs past the board edge (the ferrule): keep it off the print
            if g.GetLayer() == K.F_SilkS:
                g.SetLayer(K.F_Fab)
        fp.BuildCourtyardCaches()
        nodes[ref] = fbox(fp.GetCourtyard(K.F_CrtYd).BBox())
        obstacles.append(nodes[ref])
    for ref, (x, y, side) in HEADERS.items():
        fp = fps[ref]
        rot = fixed_rot(fp, side, "down")
        n = len(HEADER_PINS[ref])
        put(fp, x, y, rot, side)
        if min(face(p.GetPosition())[0] for p in fp.Pads()) < x - 0.1:   # pins run right to left: keep the span at x ..
            put(fp, x + 2.54 * (n - 1), y, rot, side)
        pin_x = {p.GetNumber(): face(p.GetPosition())[0] for p in fp.Pads()}
        fp.BuildCourtyardCaches()
        x0, y0, x1, y1 = fbox(fp.GetCourtyard(K.F_CrtYd).BBox())
        for g in fp.GraphicalItems():   # its own outline crosses the edge and the labels: the square pad marks pin 1
            if g.GetLayer() in (K.F_SilkS, K.B_SilkS):
                g.SetLayer(K.F_Fab)
        top = y + 0.85 + 0.45   # just past the pads
        for k, lab in enumerate(HEADER_PINS[ref]):   # the pin names, reading up from each pin
            t = text(b, lab, pin_x[str(k + 1)], top + 2, 0.8, K.F_SilkS, 90)
            t.Move(K.VECTOR2I(0, -K.FromMM(top - fbox(t.GetBoundingBox())[1])))
            y1 = max(y1, fbox(t.GetBoundingBox())[3])
        t = text(b, HEADER_TITLE[ref], x + 2.54 * (len(HEADER_PINS[ref]) - 1) / 2, y1 + 1.2, 0.9, K.F_SilkS)
        nodes[ref] = (x0 - 0.3, y - 1.0, x1 + 0.3, fbox(t.GetBoundingBox())[3] + 0.3)   # (not the pins off the edge)
        obstacles.append(nodes[ref])
    extra = {}   # the barrier parts: fixed across the barrier, the side with pedal nets down
    for ref, x in BARRIER_PARTS.items():
        fp, best = fps[ref], None
        for rot in (0, 90, 180, 270):
            put(fp, 0, 0, rot, "F")
            ped = [face(p.GetPosition())[1] for p in fp.Pads() if p.GetNetname() and p.GetNetname() not in ISOLATED]
            iso = [face(p.GetPosition())[1] for p in fp.Pads() if p.GetNetname() in ISOLATED]
            if best is None or min(iso) - max(ped) > best[0]:
                best = (min(iso) - max(ped), rot, (max(ped) + min(iso)) / 2)
        put(fp, x, sum(BARRIER) / 2 - best[2], best[1], "F")
        fp.BuildCourtyardCaches()
        cb = fbox(fp.GetCourtyard(K.F_CrtYd).BBox())
        obstacles.append(cb)
        extra.setdefault(GROUP_OF[ref], []).append(cb)
    for ref, fp in fps.items():
        if ref in JACKS or ref in HEADERS or ref in BARRIER_PARTS:
            for p in fp.Pads():
                px, py = face(p.GetPosition())
                fixed_pads[(ref, p.GetNumber())] = (px, py)
                sz = K.ToMM(p.GetSize(K.F_Cu).x) / 2 + 0.35 if hasattr(p, "GetSize") else 1.5
                obstacles.append((px - sz, py - sz, px + sz, py + sz))
    words, labels = artwork(b)
    fixed_boxes = [nodes[h] for h in HEADERS]
    for t in words:
        bx = fbox(t.GetBoundingBox())   # the parts keep off the printing
        obstacles.append((bx[0] - 0.5, bx[1] - 0.4, bx[2] + 0.5, bx[3] + 0.4))
        fixed_boxes.append(obstacles[-1])

    # 2. everything else on the top too, in its job's box
    movable = [r for r in fps if r not in JACKS and r not in HEADERS and r not in BARRIER_PARTS]
    geo = {r: geometry(fps[r], "F") for r in movable}
    pl = Placer(geo, fixed_pads, obstacles, nets(), random.Random(a.seed))
    pl.fixed_boxes = fixed_boxes
    pl.tall = {r for r in movable if not str(fps[r].GetFPID().GetLibItemName()).startswith(FLAT)}
    pl.extra = extra
    size = lambda r: (geo[r][0][0][2] - geo[r][0][0][0]) * (geo[r][0][0][3] - geo[r][0][0][1])
    pl.seed(sorted(movable, key=lambda r: ([g for g, _, _, _ in GROUPS].index(GROUP_OF[r]), -size(r))))
    c0 = pl.total()
    c1 = pl.anneal(a.moves)
    print(f"placement: wire estimate {c0:.0f} -> {c1:.0f}, boxes overlap {pl.overlap():.1f} mm2")
    for r in movable:
        x, y = pl.pos[r]
        put(fps[r], x, y, pl.rot[r], "F")
    draw_groups(b, pl, nodes)
    clip_silk_at_pads(b)

    edge_keepout(b)
    barrier_keepout(b)
    pcb = os.path.join(BUILD, NAME + ".kicad_pcb")
    write_project(pcb)
    K.SaveBoard(pcb, b)
    if a.place_only:
        run(["kicad-cli", "pcb", "render", "--side", "top", "--width", "1800", "--height", "1100", "--quality", "basic",
             "-o", os.path.join(BUILD, "placed.png"), pcb], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return

    # 3. route
    dsn, ses = os.path.join(BUILD, NAME + ".dsn"), os.path.join(BUILD, NAME + ".ses")
    if os.path.exists(ses):
        os.remove(ses)
    K.ExportSpecctraDSN(b, dsn)
    run([JAVA, "-Djava.awt.headless=true", "-jar", FREEROUTING, "-de", dsn, "-do", ses, "-mp", str(a.passes),
         "-mt", str(max(1, (os.cpu_count() or 2) - 1))], stdout=open(os.path.join(BUILD, "freerouting.log"), "w"),
        stderr=subprocess.STDOUT, timeout=1800)
    if not os.path.exists(ses):
        sys.exit("freerouting wrote no session (see build/freerouting.log)")
    K.ImportSpecctraSES(b, ses)
    for t in b.GetTracks():   # the router necks a few stubs below JLCPCB's 0.127 mm: widen them back (DRC checks spacing)
        if t.GetClass() != "PCB_VIA" and t.GetWidth() < K.FromMM(0.127):
            t.SetWidth(K.FromMM(0.127))
    ignd, gnd = b.FindNet("IGND"), b.FindNet("GND")
    zs = [zone(b, ignd, L, BARRIER[1] + 0.3, 1e3) for L in (K.F_Cu, K.B_Cu)] + \
         [zone(b, gnd, L, -1e3, BARRIER[0] - 0.3) for L in (K.F_Cu, K.B_Cu)]
    K.ZONE_FILLER(b).Fill(b.Zones())
    K.SaveBoard(pcb, b)
    set_font(pcb)
    print(f"zones: {len(zs)} ground pours (IGND above the barrier, GND below)")

    # 4. check + export
    drc = os.path.join(BUILD, "drc.json")
    run(["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all", "--refill-zones", "--save-board", "-o", drc, pcb], stdout=subprocess.DEVNULL)
    import json
    rep = json.load(open(drc))
    viol = rep.get("violations", [])
    unc = rep.get("unconnected_items", [])
    by = {}
    for v in viol:
        by.setdefault((v["severity"], v["type"]), 0)
        by[(v["severity"], v["type"])] += 1
    print(f"DRC: {len(unc)} unconnected, " + (", ".join(f"{n} {s} {t}" for (s, t), n in sorted(by.items())) or "no violations"))

    g = os.path.join(BUILD, "gerbers")
    shutil.rmtree(g, ignore_errors=True), os.makedirs(g)
    run(["kicad-cli", "pcb", "export", "gerbers", "-o", g + "/", "--layers",
         "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts", pcb], stdout=subprocess.DEVNULL)
    run(["kicad-cli", "pcb", "export", "drill", "-o", g + "/", "--format", "excellon", pcb], stdout=subprocess.DEVNULL)
    with zipfile.ZipFile(os.path.join(OUT, NAME + "-gerbers.zip"), "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(g)):   # (no drill map / job file: board houses read those as extra layers)
            if f.endswith((".gbrjob",)) or "drl_map" in f:
                continue
            z.write(os.path.join(g, f), f)
    pos = os.path.join(BUILD, "pos.csv")
    run(["kicad-cli", "pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both", "-o", pos, pcb],
        stdout=subprocess.DEVNULL)
    with open(pos) as f, open(os.path.join(OUT, NAME + "-cpl.csv"), "w", newline="") as o:
        w = csv.writer(o)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for row in csv.DictReader(f):
            w.writerow([row["Ref"], row["PosX"] + "mm", row["PosY"] + "mm", "Top" if row["Side"] == "top" else "Bottom",
                        row["Rot"]])
    shutil.copy(os.path.join(HERE, "carrier-bom.csv"), os.path.join(OUT, NAME + "-bom.csv"))
    shutil.copy(pcb, os.path.join(OUT, NAME + ".kicad_pcb"))
    shutil.copy(pcb[:-10] + ".kicad_pro", os.path.join(OUT, NAME + ".kicad_pro"))
    for side in ("top", "bottom"):
        try:
            run(["kicad-cli", "pcb", "render", "--side", side, "--width", "1600", "--height", "900", "--quality", "basic",
                 "-o", os.path.join(OUT, f"{NAME}-{side}.png"), pcb], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception as e:
            print("  (render skipped:", e, ")")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
