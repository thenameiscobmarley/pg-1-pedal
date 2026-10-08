#!/usr/bin/env python3
"""Checks the routed board keeps its two worlds apart: every track, via and pour of an isolated net stays above the
barrier strip, every pedal-side one below it. (The barrier parts' own pads are the only bridges.)"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pcbnew as K   # noqa: E402
from carrier import ISOLATED   # noqa: E402
from make_board import BARRIER, face   # noqa: E402

b = K.LoadBoard(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "build", "pg1-carrier.kicad_pcb"))
bad = []
for t in b.GetTracks():
    net = t.GetNetname()
    if not net:
        continue
    pts = [face(t.GetStart()), face(t.GetEnd())] if t.GetClass() != "PCB_VIA" else [face(t.GetPosition())]
    iso = net in ISOLATED
    for x, y in pts:
        if (iso and y < BARRIER[1]) or (not iso and y > BARRIER[0]):
            bad.append((net, round(x, 2), round(y, 2)))
for z in b.Zones():
    if z.GetIsRuleArea() or not z.GetNetname():
        continue
    bb = z.GetBoundingBox()
    (x0, y1), (x1, y0) = face(bb.GetOrigin()), face(bb.GetEnd())
    iso = z.GetNetname() in ISOLATED
    if (iso and y0 < BARRIER[1] - 0.01) or (not iso and y1 > BARRIER[0] + 0.01):
        bad.append(("pour " + z.GetNetname(), round(y0, 2), round(y1, 2)))
print("isolation: OK, nothing crosses the barrier" if not bad else f"isolation: {len(bad)} PROBLEMS: {bad[:10]}")
sys.exit(1 if bad else 0)
