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
from carrier import parts, nets   # noqa: E402

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
TONGUE = (-34.0, 28.0, 28.0, TOP)
OUTLINE = [(-55.5, TOP), (55.5, TOP), (55.5, 43.0), (28.0, 43.0), (28.0, 28.0), (-34.0, 28.0), (-34.0, 43.0), (-55.5, 43.0)]
# Neutrik NMJ6HCD2 (datasheet ST-NMJ6HCD2): body front on the wall, pins 4 / 10.35 / 16.7 mm in from it, rows 16.23 apart,
# axis 8.14 mm above the board. KiCad's footprint has its origin on the T pin and the front toward +x: turned 90 deg.
JACK_T = (-8.115, 16.7)                # T pin from (jack axis, inside of the wall)
JACKS = {"J1": -42.0, "J2": 0.0, "J3": 21.0, "J4": 42.0}
# headers: (first pin x, row y, side). Right-angle ones on the bottom point off the lower edge (jumpers lie flat).
HEADERS = {"J19": (-14.5, 30.0, "B"), "J20": (-32.6, 30.0, "B"), "J10": (8.6, 30.0, "B"), "J9": (2.2, 30.0, "B")}
HEADER_PINS = {   # printed beside each pin
    "J10": ["in l", "in r", "out l", "out r", "agnd", "9v", "dgnd"],
    "J19": ["bus l", "hp l", "gnd", "bus r", "hp r", "gnd"],
    "J20": ["bus l", "line l", "gnd", "bus r", "line r", "gnd"],
    "J9": ["+", "-"]}
HEADER_TITLE = {"J10": "seed3", "J19": "pg-hp", "J20": "pg-line", "J9": "9v"}
JACK_NAME = {"J1": "line in", "J2": "line out", "J3": "no amp", "J4": "phones"}

TRACK, CLEAR, VIA, DRILL = 0.3, 0.2, 0.6, 0.3
GAP = 0.35                             # extra room around each part's courtyard (space for tracks and vias)
EDGE = 0.4                             # courtyard to board edge
NET_W = {"AGND": 0.12, "GND": 0.35, "+9V": 0.5, "+3V3A": 0.6, "VREF": 0.7}
STICK = {"C66": "U4", "C67": "U5", "C68": "U6", "C64": "U7", "C65": "U8", "C4": "U1", "C5": "U1", "C3": "U1",
         "C60": "U7", "C61": "U7", "C62": "U8", "C63": "U8", "R60": "U8"}   # keep these right at their chip


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
    ds.m_CopperEdgeClearance = K.FromMM(0.4)
    ds.SetAuxOrigin(kpt(STRIP[0], TONGUE[1]))   # the board's lower-left corner = 0,0 in the placement file
    ds.m_TrackMinWidth = K.FromMM(0.15)   # Freerouting necks down to 0.15 mm into the TSSOP pins (PCBWay: 0.1 mm min)
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
        first_left = pads["1"][0] == min(px)
        if want == "down" and along_x and first_left and (y0 + y1) / 2 < min(py) - 0.5:
            return rot
        if want == "up" and along_x and first_left:
            return rot
    sys.exit(f"no rotation for {fp.GetReference()}")


# ---------------------------------------------------------------- placement (simulated annealing)
class Placer:
    CELL = 4.0

    def __init__(self, geo, fixed_pads, obstacles, netlist, rng):
        self.geo, self.rng = geo, rng
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
        b = self.geo[r][rot][0]
        return (x + b[0] - GAP, y + b[1] - GAP, x + b[2] + GAP, y + b[3] + GAP)

    @staticmethod
    def inside(b):
        for x0, y0, x1, y1 in (STRIP, TONGUE):
            if b[0] >= x0 + EDGE - GAP and b[1] >= y0 + EDGE - GAP and b[2] <= x1 - EDGE + GAP and b[3] <= y1 - EDGE + GAP:
                return True
        return False

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

    def free(self, b, skip=()):
        if not self.inside(b):
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
        other = STICK.get(r)
        if not other or other not in self.pos or r not in self.pos:
            return 0.0
        (x, y), (u, v) = self.pos[r], self.pos[other]
        return 2.0 * (abs(x - u) + abs(y - v))

    def part_cost(self, rs):
        ns = set(n for r in rs for n in self.of[r])
        st = set(rs) | {k for k, v in STICK.items() if v in rs}
        return sum(self.net_cost(n) for n in ns) + sum(self.stick_cost(s) for s in st)

    def total(self):
        return sum(self.net_cost(n) for n in self.nets) + sum(self.stick_cost(s) for s in STICK)

    # --- first placement: each part as close as it fits to what it's wired to
    def seed(self, order):
        xs = [x / 2 for x in range(int(STRIP[0] * 2), int(STRIP[2] * 2) + 1)]
        ys = [y / 2 for y in range(int(TONGUE[1] * 2), int(STRIP[3] * 2) + 1)]
        for r in order:
            anchors = [self.pin(rr, q) for n in self.of[r] if NET_W.get(n, 1) >= 1 or r in STICK
                       for rr, q in self.nets[n] if rr != r]
            anchors = [a for a in anchors if a] or [(-3.0, 40.0)]
            if r in STICK and STICK[r] in self.pos:
                anchors = [self.pos[STICK[r]]]
            tx = sum(a[0] for a in anchors) / len(anchors)
            ty = sum(a[1] for a in anchors) / len(anchors)
            for x, y in sorted(((x, y) for x in xs for y in ys), key=lambda p: (p[0] - tx) ** 2 + (p[1] - ty) ** 2):
                rot = next((q for q in (0, 90, 180, 270) if self.free(self.boxat(r, x, y, q))), None)
                if rot is not None:
                    self.set(r, x, y, rot)
                    break
            else:
                sys.exit(f"{r} doesn't fit on the board")

    def anneal(self, moves=120000):
        rng = self.rng
        cost = self.total()
        t0, t1 = 4.0, 0.02
        sizes = {r: round((self.geo[r][0][0][2] - self.geo[r][0][0][0]) * (self.geo[r][0][0][3] - self.geo[r][0][0][1]), 0)
                 for r in self.refs}
        for k in range(moves):
            t = t0 * (t1 / t0) ** (k / moves)
            r = rng.choice(self.refs)
            x, y = self.pos[r]
            rot = self.rot[r]
            if rng.random() < 0.2:   # swap with a same-size part
                s = rng.choice(self.refs)
                if s == r or sizes[s] != sizes[r]:
                    continue
                before = self.part_cost([r, s])
                (u, v), rs = self.pos[s], self.rot[s]
                b1, b2 = self.boxat(r, u, v, rs), self.boxat(s, x, y, rot)
                ov = b1[0] < b2[2] and b2[0] < b1[2] and b1[1] < b2[3] and b2[1] < b1[3]
                if ov or not self.free(b1, (r, s)) or not self.free(b2, (r, s)):
                    continue
                self.set(r, u, v, rs), self.set(s, x, y, rot)
                d = self.part_cost([r, s]) - before
                if d > 0 and rng.random() > math.exp(-d / t):
                    self.set(r, x, y, rot), self.set(s, u, v, rs)
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
            if not self.free(b, (r,)):
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
    t.SetTextSize(K.VECTOR2I(K.FromMM(size), K.FromMM(size))), t.SetTextThickness(K.FromMM(size * 0.15))
    t.SetTextAngleDegrees(rot)
    if just:
        t.SetHorizJustify(K.GR_TEXT_H_ALIGN_LEFT if just < 0 else K.GR_TEXT_H_ALIGN_RIGHT)
    if layer == K.B_SilkS:
        t.SetMirrored(True)
    b.Add(t)
    return t


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


def zone(b, net, layer):
    z = K.ZONE(b)
    z.SetLayer(layer), z.SetNet(net), z.SetIsRuleArea(False)
    ol = z.Outline()
    ol.NewOutline()
    for x, y in OUTLINE:
        ol.Append(kpt(x, y))
    z.SetLocalClearance(K.FromMM(0.3)), z.SetMinThickness(K.FromMM(0.25))
    z.SetThermalReliefGap(K.FromMM(0.3)), z.SetThermalReliefSpokeWidth(K.FromMM(0.45))
    z.SetPadConnection(K.ZONE_CONNECTION_THT_THERMAL)   # solid to SMD pads (a fine-pitch pad only fits one spoke)
    b.Add(z)
    return z


def write_project(pcb):
    """kicad-cli's DRC takes its rules from the .kicad_pro next to the board"""
    import json
    rules = {"min_track_width": 0.15, "min_clearance": CLEAR, "min_copper_edge_clearance": 0.4,
             "min_via_diameter": VIA, "min_through_hole_diameter": DRILL, "min_text_height": 0.7}
    nc = {"name": "Default", "track_width": TRACK, "clearance": CLEAR, "via_diameter": VIA, "via_drill": DRILL}
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
    a = ap.parse_args()
    os.makedirs(BUILD, exist_ok=True), os.makedirs(OUT, exist_ok=True)
    b, fps = make_board()

    # 1. fixed parts
    obstacles, fixed_pads = [], {}
    for ref, x in JACKS.items():
        put(fps[ref], x + JACK_T[0], WALL_IN - JACK_T[1], 90, "F")
        for g in fps[ref].GraphicalItems():   # its outline runs past the board edge (the ferrule): keep it off the print
            if g.GetLayer() == K.F_SilkS:
                g.SetLayer(K.F_Fab)
    for ref, (x, y, side) in HEADERS.items():
        fp = fps[ref]
        put(fp, x, y, fixed_rot(fp, side, "down"), side)
        fp.BuildCourtyardCaches()
        x0, y0, x1, y1 = fbox(fp.GetCourtyard(K.B_CrtYd).BBox())
        for g in fp.GraphicalItems():   # its own outline crosses the edge and the labels: the square pad marks pin 1
            if g.GetLayer() in (K.F_SilkS, K.B_SilkS):
                g.SetLayer(K.B_Fab)
        top = y + 0.85 + 0.45   # just past the pads
        for k, lab in enumerate(HEADER_PINS[ref]):   # the pin names, reading up from each pin
            t = text(b, lab, x + 2.54 * k, top + 2, 0.8, K.B_SilkS, 90)
            t.Move(K.VECTOR2I(0, -K.FromMM(top - fbox(t.GetBoundingBox())[1])))
            top_k = fbox(t.GetBoundingBox())[3]
            y1 = max(y1, top_k)
        t = text(b, HEADER_TITLE[ref], x + 2.54 * (len(HEADER_PINS[ref]) - 1) / 2, y1 + 1.2, 0.9)
        obstacles.append((x0 - 0.3, y0, x1 + 0.3, fbox(t.GetBoundingBox())[3] + 0.3))
    for ref, fp in fps.items():
        if ref in JACKS or ref in HEADERS:
            for p in fp.Pads():
                px, py = face(p.GetPosition())
                fixed_pads[(ref, p.GetNumber())] = (px, py)
                sz = K.ToMM(p.GetSize(K.F_Cu).x) / 2 + 0.35 if hasattr(p, "GetSize") else 1.5
                obstacles.append((px - sz, py - sz, px + sz, py + sz))
    for t in [text(b, JACK_NAME[ref], x, 49.0, 1.0) for ref, x in JACKS.items()] + [text(b, "pg-1 carrier v2", 44.0, 44.8, 1.0)]:
        bx = fbox(t.GetBoundingBox())   # the parts keep off the printing
        obstacles.append((bx[0] - 0.3, bx[1] - 0.3, bx[2] + 0.3, bx[3] + 0.3))

    # 2. everything else on the bottom
    movable = [r for r in fps if r not in JACKS and r not in HEADERS]
    geo = {r: geometry(fps[r], "B") for r in movable}
    pl = Placer(geo, fixed_pads, obstacles, nets(), random.Random(a.seed))
    pl.seed(movable)
    c0 = pl.total()
    c1 = pl.anneal(a.moves)
    print(f"placement: wire estimate {c0:.0f} -> {c1:.0f}")
    for r in movable:
        x, y = pl.pos[r]
        put(fps[r], x, y, pl.rot[r], "B")

    edge_keepout(b)
    pcb = os.path.join(BUILD, NAME + ".kicad_pcb")
    write_project(pcb)
    K.SaveBoard(pcb, b)

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
    agnd = b.FindNet("AGND")
    zs = [zone(b, agnd, K.F_Cu), zone(b, agnd, K.B_Cu)]
    K.ZONE_FILLER(b).Fill(b.Zones())
    K.SaveBoard(pcb, b)
    print(f"zones: {len(zs)} AGND pours")

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
    run(["kicad-cli", "pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both", "--use-drill-file-origin", "-o", pos, pcb],
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
