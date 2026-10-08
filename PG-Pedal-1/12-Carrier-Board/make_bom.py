#!/usr/bin/env python3
"""Writes carrier-bom.csv (PCBWay assembly format: one line per part kind, with designators) from carrier.py."""
import csv, collections
from carrier import parts
groups = collections.OrderedDict()
for ref, value, fp, mpn, pins, note in parts:
    if fp.startswith("NetTie"):
        continue
    key = (value, fp.split(":")[-1], mpn)
    groups.setdefault(key, []).append(ref)
with open("carrier-bom.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Item", "Designator", "Qty", "Value", "Package", "Manufacturer Part Number", "Assembly"])
    for i, ((value, pkg, mpn), refs) in enumerate(groups.items(), 1):
        tht = pkg.startswith("PinHeader") or "Jack" in pkg
        w.writerow([i, ",".join(refs), len(refs), value, pkg, mpn or "generic", "THT (hand / optional)" if tht else "SMT"])
smt = sum(len(r) for (v, p, m), r in groups.items() if not (p.startswith("PinHeader") or "Jack" in p))
print(f"{len(groups)} part kinds, {sum(len(r) for r in groups.values())} parts ({smt} SMT)")
