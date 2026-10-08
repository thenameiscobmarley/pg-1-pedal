#!/usr/bin/env python3
"""Writes jlcpcb/ : the JLCPCB assembly files (BOM with JLCPCB/LCSC part numbers + placement), from carrier.py and the
board made by make_board.py (run that first). The 4 jacks aren't stocked by JLCPCB: they're left off (fitted by hand).
Part numbers checked in JLCPCB's parts library on 2026-10-08 ("base" = no extra fee, the rest are "extended")."""
import csv, os, shutil
from carrier import parts

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "jlcpcb")
LCSC = {  # (value, footprint name) -> JLCPCB part
    ("10k", "R_0603_1608Metric"): "C25804", ("1k", "R_0603_1608Metric"): "C21190",
    ("100k", "R_0603_1608Metric"): "C25803", ("20k", "R_0603_1608Metric"): "C4184",
    ("100", "R_0603_1608Metric"): "C22775", ("22k", "R_0603_1608Metric"): "C31850",
    ("1", "R_0603_1608Metric"): "C22936",
    ("11.5k 1%", "R_0603_1608Metric"): "C25949", ("470k", "R_0603_1608Metric"): "C23178",
    ("0", "R_0603_1608Metric"): "C21189",
    ("100nF", "C_0603_1608Metric"): "C14663", ("1uF", "C_0603_1608Metric"): "C15849",
    ("10nF", "C_0603_1608Metric"): "C57112", ("100pF C0G", "C_0603_1608Metric"): "C14858",
    ("47pF C0G", "C_0603_1608Metric"): "C1671", ("4.7uF", "C_0805_2012Metric"): "C1779",
    ("10uF 25V", "C_0805_2012Metric"): "C15850", ("22uF 25V", "C_1206_3216Metric"): "C12891",
    ("22uF 25V", "C_0805_2012Metric"): "C45783", ("51k", "R_0603_1608Metric"): "C23196",
    ("PTC 300mA", "Fuse_1206_3216Metric"): "C19193041", ("SS34", "D_SMA"): "C8678", ("SMAJ12A", "D_SMA"): "C113957",
    ("1N4148W", "D_SOD-123"): "C81598", ("PESD15VL2BT", "SOT-23"): "C38838",
    ("LP2985-33", "SOT-23-5"): "C95414", ("TLV7031", "SOT-23-5"): "C2869832",
    ("NE5532", "SOIC-8_3.9x4.9mm_P1.27mm"): "C7426", ("TPA6139A2", "TSSOP-14_4.4x5mm_P0.65mm"): "C2870791",
    ("seed audio", "PinHeader_1x07_P2.54mm_Horizontal"): "C32713266",
    ("pg-hp", "PinHeader_1x06_P2.54mm_Horizontal"): "C32713265",
    ("pg-line", "PinHeader_1x06_P2.54mm_Horizontal"): "C32713265",
    ("9V in", "PinHeader_1x02_P2.54mm_Vertical"): "C32713268",
}
SKIP = {"J1", "J2", "J3", "J4"}   # 6.35 mm jacks: not stocked, fitted by hand (Tayda A-1122)


def main():
    os.makedirs(OUT, exist_ok=True)
    groups = {}
    for ref, value, fp, mpn, pins, note in parts:
        if ref in SKIP:
            continue
        pkg = fp.split(":")[-1]
        code = LCSC.get((value, pkg))
        if not code:
            raise SystemExit(f"no JLCPCB part for {ref} {value} {pkg}")
        groups.setdefault((value, pkg, code), []).append(ref)
    with open(os.path.join(OUT, "pg1-carrier-bom-jlc.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (value, pkg, code), refs in groups.items():
            w.writerow([value, ",".join(refs), pkg, code])
    pos = os.path.join(HERE, "build", "pos.csv")
    with open(pos) as f, open(os.path.join(OUT, "pg1-carrier-cpl-jlc.csv"), "w", newline="") as o:
        w = csv.writer(o)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for row in csv.DictReader(f):
            if row["Ref"] in SKIP:
                continue
            w.writerow([row["Ref"], row["PosX"] + "mm", row["PosY"] + "mm", "Top" if row["Side"] == "top" else "Bottom",
                        row["Rot"]])
    shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier-gerbers.zip"), os.path.join(OUT, "pg1-carrier-gerbers.zip"))
    n = sum(len(r) for r in groups.values())
    print(f"jlcpcb/: {len(groups)} part kinds, {n} parts (jacks left off)")


if __name__ == "__main__":
    main()
