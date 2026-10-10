#!/usr/bin/env python3
"""Places parts from carrier.py on the routed board where there's room among the PARTS (tracks ignored), as close as
possible to a given point, lifts only the track pieces under them, and lets Freerouting reconnect whatever is missing
(everything else locked). For a part that must sit right at a pin.

    python3 rip_place.py D64:1.0,48.5 D65:2.5,48.5     # ref:x,y  (face mm: where it should be, the nearest free spot)"""
import math, os, shutil, subprocess, sys, json
args, sys.argv = sys.argv[1:], sys.argv[:1]
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import add_leveller as A   # noqa: E402
import pcbnew as K         # noqa: E402
from carrier import parts  # noqa: E402

spec = {p[0]: p for p in parts}
pro = A.PCB[:-10] + ".kicad_pro"
b = K.LoadBoard(A.PCB)
# parts-only occupancy: footprints' courtyards and pads (both layers for through-hole)
occ = A.Occ.__new__(A.Occ)
occ.g = {k: [[0] * A.NX for _ in range(A.NY)] for k in ("F", "B", "Bbody")}
occ.sat = {}
for j in range(A.NY):
    for i in range(A.NX):
        x, y = A.X0 + (i + 0.5) * A.CELL, A.Y0 + (j + 0.5) * A.CELL
        if y < A.ISO_Y or not all(A.inside(x + dx, y + dy) for dx, dy in ((0.5, 0), (-0.5, 0), (0, 0.5), (0, -0.5), (0, 0))):
            for k in occ.g:
                occ.g[k][j][i] = 1
for fp in b.GetFootprints():
    occ.add_fp(fp)
for d in b.Drawings():
    if d.GetLayer() == K.F_SilkS and d.GetClass() == "PCB_TEXT":
        occ.mark("F", A.frect(d.GetBoundingBox()), 0.15)
lifted, boxes = 0, []
loaded = {a_.split(":")[0]: A.load(spec[a_.split(":")[0]][2]) for a_ in args}   # (load every footprint before touching tracks)
for a_ in args:
    ref, xy = a_.split(":")
    tx, ty = map(float, xy.split(","))
    _, val, fpid, mpn, pins, note = spec[ref]
    fp = loaded[ref]
    fp.SetReference(ref), fp.SetValue(val), fp.Reference().SetVisible(False)
    b.Add(fp)
    for p in fp.Pads():
        p.SetNet(b.FindNet(pins[int(p.GetNumber())]))
    best = None
    for rot in (0, 90, 180, 270):
        A.put(fp, 0, 0, rot, "F")
        fp.BuildCourtyardCaches()
        c0 = A.frect(fp.GetCourtyard(K.F_CrtYd).BBox())
        for j in range(int((ty - 8) / A.CELL), int((ty + 8) / A.CELL)):
            for i in range(int((tx - 8) / A.CELL), int((tx + 8) / A.CELL)):
                x, y = i * A.CELL, j * A.CELL
                box = (c0[0] + x, c0[1] + y, c0[2] + x, c0[3] + y)
                if not occ.free("F", box, 0.0):
                    continue
                d = math.hypot(x - tx, y - ty)
                if best is None or d < best[0]:
                    best = (d, x, y, rot)
    if best is None:
        sys.exit(f"no room for {ref} near {tx}, {ty}")
    d, x, y, rot = best
    A.put(fp, x, y, rot, "F")
    occ.add_fp(fp)
    fp.BuildCourtyardCaches()
    boxes.append(A.frect(fp.GetCourtyard(K.F_CrtYd).BBox()))
    print(f"  {ref} at ({x:.2f}, {y:.2f}) rot {rot}, {d:.1f} mm from the target")
m = 0.35   # every part placed first, then the track pieces under them lifted in one go (pcbnew's python breaks otherwise)
gone = [t for t in b.GetTracks() if (t.GetClass() == "PCB_VIA" or t.GetLayer() == K.F_Cu) and
        any(r[0] < cy[2] + m and cy[0] - m < r[2] and r[1] < cy[3] + m and cy[1] - m < r[3]
            for cy in boxes for r in [A.frect(t.GetBoundingBox())])]
for t in gone:
    b.Remove(t)
lifted = len(gone)
print(f"  {lifted} track pieces / vias lifted")
K.SaveBoard(A.PCB, b)
shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier.kicad_pro"), pro)
b = K.LoadBoard(A.PCB)
dsn, ses = os.path.join(HERE, "build", "rip.dsn"), os.path.join(HERE, "build", "rip.ses")
if os.path.exists(ses):
    os.remove(ses)
for t in b.GetTracks():
    t.SetLocked(True)
K.ExportSpecctraDSN(b, dsn)
subprocess.run([A.JAVA, "-Djava.awt.headless=true", "-jar", A.FREEROUTING, "-de", dsn, "-do", ses, "-mp", "20",
                "-mt", str(max(1, (os.cpu_count() or 2) - 1))], stdout=open(os.path.join(HERE, "build", "freerouting-rip.log"), "w"),
               stderr=subprocess.STDOUT, timeout=1800)
K.ImportSpecctraSES(b, ses)
for t in b.GetTracks():
    t.SetLocked(False)
    if t.GetClass() != "PCB_VIA" and t.GetWidth() < K.FromMM(0.127):
        t.SetWidth(K.FromMM(0.127))
K.ZONE_FILLER(b).Fill(b.Zones())
K.SaveBoard(A.PCB, b)
shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier.kicad_pro"), pro)
drc = os.path.join(HERE, "build", "drc-rip.json")
subprocess.run(["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all", "--refill-zones", "--save-board", "-o", drc, A.PCB],
               stdout=subprocess.DEVNULL)
shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier.kicad_pro"), pro)
rep = json.load(open(drc))
by = {}
for v in rep.get("violations", []):
    by[(v["severity"], v["type"])] = by.get((v["severity"], v["type"]), 0) + 1
print(f"DRC: {len(rep.get('unconnected_items', []))} unconnected, " + ", ".join(f"{n} {s} {t}" for (s, t), n in sorted(by.items())))
