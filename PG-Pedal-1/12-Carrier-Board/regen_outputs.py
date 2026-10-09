#!/usr/bin/env python3
"""Rewrites the order files (gerbers zip, renders, board copy) from build/pg1-carrier.kicad_pcb without re-routing.
Use after hand edits to the routed board (silk labels, hole offsets). Then run make_jlc.py."""
import os, shutil, subprocess, zipfile
BUILD, OUT, NAME = 'build', 'pcbway', 'pg1-carrier'
pcb = os.path.join(BUILD, NAME + '.kicad_pcb')
run = lambda a: subprocess.run(a, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
g = os.path.join(BUILD, 'gerbers'); shutil.rmtree(g, ignore_errors=True); os.makedirs(g)
run(["kicad-cli", "pcb", "export", "gerbers", "-o", g + "/", "--layers",
     "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts", pcb])
run(["kicad-cli", "pcb", "export", "drill", "-o", g + "/", "--format", "excellon", pcb])
with zipfile.ZipFile(os.path.join(OUT, NAME + "-gerbers.zip"), "w", zipfile.ZIP_DEFLATED) as z:
    for f in sorted(os.listdir(g)):
        if f.endswith(".gbrjob") or "drl_map" in f:
            continue
        z.write(os.path.join(g, f), f)
shutil.copy(pcb, os.path.join(OUT, NAME + ".kicad_pcb"))
for side in ("top", "bottom"):
    run(["kicad-cli", "pcb", "render", "--side", side, "--width", "1600", "--height", "900", "--quality", "basic",
         "-o", os.path.join(OUT, f"{NAME}-{side}.png"), pcb])
print("outputs rewritten")
