#!/usr/bin/env python3
"""Re-routes one window of the routed board: tracks inside it are unlocked (Freerouting may move them), everything
else stays locked.   python3 reroute_window.py x0 y0 x1 y1 [passes]   (face mm)"""
import os, shutil, subprocess, sys, json
x0, y0, x1, y1 = map(float, sys.argv[1:5]); passes = sys.argv[5] if len(sys.argv) > 5 else "40"
sys.argv = sys.argv[:1]
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import add_leveller as A   # noqa: E402
import pcbnew as K         # noqa: E402
pro = A.PCB[:-10] + ".kicad_pro"
b = K.LoadBoard(A.PCB)
n = 0
for t in b.GetTracks():
    r = A.frect(t.GetBoundingBox())
    inside = x0 <= r[0] and r[2] <= x1 and y0 <= r[1] and r[3] <= y1
    t.SetLocked(not inside)
    n += inside
print(f"  {n} track pieces free to move")
dsn, ses = os.path.join(HERE, "build", "win.dsn"), os.path.join(HERE, "build", "win.ses")
if os.path.exists(ses):
    os.remove(ses)
K.ExportSpecctraDSN(b, dsn)
subprocess.run([A.JAVA, "-Djava.awt.headless=true", "-jar", A.FREEROUTING, "-de", dsn, "-do", ses, "-mp", passes,
                "-mt", str(max(1, (os.cpu_count() or 2) - 1))], stdout=open(os.path.join(HERE, "build", "freerouting-win.log"), "w"),
               stderr=subprocess.STDOUT, timeout=2400)
K.ImportSpecctraSES(b, ses)
for t in b.GetTracks():
    t.SetLocked(False)
    if t.GetClass() != "PCB_VIA" and t.GetWidth() < K.FromMM(0.127):
        t.SetWidth(K.FromMM(0.127))
K.ZONE_FILLER(b).Fill(b.Zones())
K.SaveBoard(A.PCB, b)
shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier.kicad_pro"), pro)
drc = os.path.join(HERE, "build", "drc-win.json")
subprocess.run(["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all", "--refill-zones", "--save-board", "-o", drc, A.PCB],
               stdout=subprocess.DEVNULL)
shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier.kicad_pro"), pro)
rep = json.load(open(drc))
by = {}
for v in rep.get("violations", []):
    by[(v["severity"], v["type"])] = by.get((v["severity"], v["type"]), 0) + 1
print(f"DRC: {len(rep.get('unconnected_items', []))} unconnected, " + ", ".join(f"{n} {s} {t}" for (s, t), n in sorted(by.items())))
for u in rep.get("unconnected_items", []):
    print("  UNCONN", [(i["description"][:50], round(i["pos"]["x"] - 100, 1), round(100 - i["pos"]["y"], 1)) for i in u["items"]])
