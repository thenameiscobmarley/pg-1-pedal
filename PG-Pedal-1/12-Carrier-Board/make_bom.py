#!/usr/bin/env python3
"""Writes carrier-bom.csv in PCBWay's assembly BOM format (one line per part kind, with its designators) from carrier.py."""
import csv, collections
from carrier import parts

MAKER = {"NE5532DR": "Texas Instruments", "TPA6139A2PWR": "Texas Instruments", "LP2985-33DBVR": "Texas Instruments",
         "TLV7031DBVR": "Texas Instruments", "PESD15VL2BT": "Nexperia", "BAT54": "Nexperia", "MF-NSMF030X-2": "Bourns",
         "SMAJ12A": "Littelfuse", "SS34": "onsemi", "NMJ6HCD2": "Neutrik", "EEE-1EA470WP": "Panasonic",
         "EEE-1CA470WR": "Panasonic", "EEE-1EA100SR": "Panasonic"}
groups = collections.OrderedDict()
for ref, value, fp, mpn, pins, note in parts:
    key = ("6.35 mm stereo PCB jack" if "Jack" in fp else value, fp.split(":")[-1], mpn)
    groups.setdefault(key, []).append(ref)


def describe(value, pkg):
    size = next((s for s in ("0603", "0805", "1206") if s in pkg), "")
    if pkg.startswith("R_") and value == "0":
        return f"0 ohm jumper {size}"
    if pkg.startswith("R_"):
        return f"resistor {value} {'' if '%' in value else '1% '}{size}".replace("  ", " ")
    if pkg.startswith("C_"):
        return f"ceramic capacitor {value} {'' if 'C0G' in value else 'X7R '}{size}, 25 V or more".replace("  ", " ")
    if pkg.startswith("CP_Elec"):
        return f"aluminium electrolytic {value}"
    return value


with open("carrier-bom.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Item #", "Designator", "Qty", "Manufacturer", "Mfg Part #", "Description / Value", "Package/Footprint",
                "Type", "Your Instructions / Notes"])
    for i, ((value, pkg, mpn), refs) in enumerate(groups.items(), 1):
        tht = pkg.startswith("PinHeader") or "Jack" in pkg
        note = ("please fit and solder" if "Jack" in pkg else
                "2.54 mm male header, any brand: please fit and solder" if tht else
                "any brand with the same value and package" if not mpn else "")
        w.writerow([i, ",".join(refs), len(refs), MAKER.get(mpn, "any" if not mpn else ""), mpn or "", describe(value, pkg),
                    pkg, "THT" if tht else "SMD", note])
smt = sum(len(r) for (v, p, m), r in groups.items() if not (p.startswith("PinHeader") or "Jack" in p))
print(f"{len(groups)} part kinds, {sum(len(r) for r in groups.values())} parts ({smt} SMT)")
