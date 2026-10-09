#!/usr/bin/env python3
"""Adds a few more parts from carrier.py to the routed board (nearest free spot to the copper they connect to),
then routes each of their pads with route_one.py. Nothing already there moves.

    python3 place_extra.py R81 R86"""
import os, shutil, subprocess, sys
sys.argv, refs = sys.argv[:1], sys.argv[1:]
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import add_leveller as A   # noqa: E402
import pcbnew as K         # noqa: E402
from carrier import parts  # noqa: E402

spec = {p[0]: p for p in parts}
pro = A.PCB[:-10] + ".kicad_pro"
b = K.LoadBoard(A.PCB)
occ = A.Occ(b)
anchor = {}
for fp in b.GetFootprints():
    for p in fp.Pads():
        if p.GetNetname():
            anchor.setdefault(p.GetNetname(), []).append(A.face(p.GetPosition()))
for ref in refs:
    _, val, fpid, mpn, pins, note = spec[ref]
    fp = A.load(fpid)
    fp.SetReference(ref), fp.SetValue(val), fp.Reference().SetVisible(False)
    b.Add(fp)
    for p in fp.Pads():
        p.SetNet(b.FindNet(pins[int(p.GetNumber())]))
    best = None
    flat = fp.GetFPID().GetLibItemName().wx_str().startswith(("R_0402", "C_0402"))
    for rot in (0, 90, 180, 270):
        A.put(fp, 0, 0, rot, "F")
        fp.BuildCourtyardCaches()
        box0 = A.frect(fp.GetCourtyard(K.F_CrtYd).BBox())
        pads0 = [(p.GetNetname(), A.face(p.GetPosition())) for p in fp.Pads()]
        for j in range(int(A.ISO_Y / A.CELL), int(70.5 / A.CELL)):
            for i in range(int(-56 / A.CELL), int(56 / A.CELL)):
                x, y = i * A.CELL, j * A.CELL
                box = (box0[0] + x, box0[1] + y, box0[2] + x, box0[3] + y)
                if not occ.free("F", box, 0.0):
                    continue
                D = A.DC_ZONE
                if not flat and box[0] < D[2] and D[0] < box[2] and box[1] < D[3] and D[1] < box[3]:
                    continue
                cost = sum(min(((px + x - ax) ** 2 + (py + y - ay) ** 2) ** 0.5 for ax, ay in anchor[n]) for n, (px, py) in pads0)
                if best is None or cost < best[0]:
                    best = (cost, x, y, rot)
    cost, x, y, rot = best
    A.put(fp, x, y, rot, "F")
    occ.add_fp(fp)
    print(f"  {ref} at ({x:.2f}, {y:.2f}) rot {rot}, {cost:.1f} mm from its nets")
K.SaveBoard(A.PCB, b)
shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier.kicad_pro"), pro)
for ref in refs:
    for pad in ("1", "2"):
        r = subprocess.run([sys.executable, os.path.join(HERE, "route_one.py"), ref, pad], capture_output=True, text=True, cwd=HERE)
        print("  ", (r.stdout.strip() or r.stderr.strip().splitlines()[-1]))
        shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier.kicad_pro"), pro)
