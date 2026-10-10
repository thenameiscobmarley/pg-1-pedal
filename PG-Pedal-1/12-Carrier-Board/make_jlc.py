#!/usr/bin/env python3
"""Writes jlcpcb/ : the JLCPCB assembly files (BOM with JLCPCB/LCSC part numbers + placement), from carrier.py and the
board made by make_board.py (run that first). Every part is fitted, the 4 Neutrik jacks too.
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
    ("0", "R_0603_1608Metric"): "C21189", ("1M", "R_0603_1608Metric"): "C22935",
    ("47nF C0G", "C_1206_3216Metric"): "C5451690", ("OPA1652", "SOIC-8_3.9x4.9mm_P1.27mm"): "C30025",
    ("100nF", "C_0603_1608Metric"): "C14663", ("1uF", "C_0603_1608Metric"): "C15849",
    ("10nF", "C_0603_1608Metric"): "C57112", ("100pF C0G", "C_0603_1608Metric"): "C14858",
    ("47pF C0G", "C_0603_1608Metric"): "C1671", ("4.7uF", "C_0805_2012Metric"): "C1779",
    ("10uF 25V", "C_0805_2012Metric"): "C15850", ("22uF 25V", "C_1206_3216Metric"): "C12891",
    ("22uF 25V", "C_0805_2012Metric"): "C45783", ("51k", "R_0603_1608Metric"): "C23196",
    ("PTC 300mA", "Fuse_1206_3216Metric"): "C19193041", ("SS34", "D_SMA"): "C8678", ("SMAJ12A", "D_SMA"): "C113957",
    ("1N4148W", "D_SOD-123"): "C81598", ("PESD15VL2BT", "SOT-23"): "C38838",
    ("LP2985-33", "SOT-23-5"): "C95414", ("TLV7031", "SOT-23-5"): "C2869832",
    ("NE5532", "SOIC-8_3.9x4.9mm_P1.27mm"): "C7426", ("TPA6139A2", "TSSOP-14_4.4x5mm_P0.65mm"): "C2870791",
    ("9v + seed3 in", "PinHeader_1x07_P2.54mm_Horizontal"): "C32713266",
    ("seed3 out", "PinHeader_1x02_P2.54mm_Horizontal"): "C32713261",
    ("pg-hp", "PinHeader_1x06_P2.54mm_Horizontal"): "C32713265",
    ("pg-line", "PinHeader_1x06_P2.54mm_Horizontal"): "C32713265",
    ("line in", "Jack_6.35mm_Neutrik_NMJ6HCD2_Horizontal"): "C368502",
    ("line out", "Jack_6.35mm_Neutrik_NMJ6HCD2_Horizontal"): "C368502",
    ("no amp", "Jack_6.35mm_Neutrik_NMJ6HCD2_Horizontal"): "C368502",
    ("phones", "Jack_6.35mm_Neutrik_NMJ6HCD2_Horizontal"): "C368502",
}
LCSC.update({   # v3 (isolated codec board)
    ("seed3 + 9v", "PinHeader_1x10_P2.54mm_Horizontal"): "C42453959",
    ("pg-hp", "PinHeader_1x03_P2.54mm_Horizontal"): "C32713262", ("pg-line", "PinHeader_1x03_P2.54mm_Horizontal"): "C32713262",
    ("AMS1117-5.0", "SOT-223-3_TabPin2"): "C6187", ("AMS1117-3.3", "SOT-223-3_TabPin2"): "C6186",
    ("ADS1015", "MSOP-10_3x3mm_P0.5mm"): "C193969", ("4.7k", "R_0603_1608Metric"): "C23162",
    ("B0505S-1WR3", "DCDC_SIP4_B0505S"): "C512048", ("ISO7741", "SOIC-16W_7.5x10.3mm_P1.27mm"): "C571196",
    ("ISO1540", "SOIC-8_3.9x4.9mm_P1.27mm"): "C179739", ("10", "R_0603_1608Metric"): "C22859",
    ("TLV320AIC3204", "Texas_RHB0032E_VQFN-32-1EP_5x5mm_P0.5mm_EP3.45x3.45mm"): "C24109",
    ("33", "R_0603_1608Metric"): "C23140", ("1k", "R_1206_3216Metric"): "C4410",
    ("100uF 6.3V", "C_1206_3216Metric"): "C15008", ("B5819W", "D_SOD-123"): "C8598", ("220", "R_0603_1608Metric"): "C22962",
    ("in", "Jack_6.35mm_Neutrik_NMJ6HFD2_Horizontal"): "C368491", ("out", "Jack_6.35mm_Neutrik_NMJ6HFD2_Horizontal"): "C368491",
})
LCSC.update({   # 0402 (JLCPCB basic)
    ("10k", "R_0402_1005Metric"): "C25744", ("100k", "R_0402_1005Metric"): "C25741", ("1k", "R_0402_1005Metric"): "C11702",
    ("4.7k", "R_0402_1005Metric"): "C25900", ("220", "R_0402_1005Metric"): "C25091", ("1M", "R_0402_1005Metric"): "C26083",
    ("20k", "R_0402_1005Metric"): "C25765", ("33", "R_0402_1005Metric"): "C25105", ("10", "R_0402_1005Metric"): "C25077",
    ("100nF", "C_0402_1005Metric"): "C1525", ("1uF", "C_0402_1005Metric"): "C52923", ("10nF", "C_0402_1005Metric"): "C15195",
    ("100pF C0G", "C_0402_1005Metric"): "C1546",
})
LCSC.update({   # the analog effect + expansion header
    ("MCP4461-103", "TSSOP-20_4.4x6.5mm_P0.65mm"): "C638707", ("TLV9062", "VSSOP-8_3x3mm_P0.65mm"): "C398356",
    ("10nF C0G", "C_0402_1005Metric"): "C22400107", ("expansion", "PinHeader_1x04_P2.54mm_Horizontal"): "C32713263",
})
LCSC.update({   # the analog leveller (2026-10-09)
    ("10uF", "C_0402_1005Metric"): "C15525", ("100", "R_0402_1005Metric"): "C25076", ("MMBT3904", "SOT-23"): "C20526",
    ("MCP4725", "SOT-23-6"): "C144198", ("3.3k", "R_0402_1005Metric"): "C25890", ("470k", "R_0402_1005Metric"): "C25790", ("1N4148WS", "D_SOD-323"): "C2128", ("BAT54S", "SOT-23"): "C47546", ("BAS40W-04", "SOT-323_SC-70"): "C134371", ("12k", "R_0402_1005Metric"): "C25752", ("TLV9062", "Texas_DSG0008A_WSON-8-1EP_2x2mm_P0.5mm_EP0.9x1.6mm"): "C2058009",
})
LCSC.update({("PSM712", "SOT-23"): "C32677"})   # basic part: no extended fee
LCSC.update({("fx loop", "PinHeader_2x04_P2.54mm_Vertical"): "C32713277"})
LCSC.update({("TLV9062", "SOIC-8_3.9x4.9mm_P1.27mm"): "C398355", ("XC6206P332MR", "SOT-23"): "C5446"})
# through-hole parts you solder yourself (big pins, easy): left off the JLCPCB order to save their part-type fees and
# the hand-soldering / manual-assembly charges. Buy them with the pots (see BOM.md).
SKIP = {"J1", "J2", "J10", "J19", "J20", "J21", "J22", "U3", "OC1", "OC2"}   # (OC = the LED + LDR pairs)
# NOT FITTED at all (pads stay empty): C31 / C41, 100 pF on the input's 500 k bias divider, made a 3.2 kHz low-pass
# (found by the board simulation, 10-Carla-Plugin/Source/sim). Radio is still kept out at the jack (1k + the clamp).
DNP = {"C31", "C41", "R31", "R41"}
SKIP |= DNP


def rotation_fix(fp_name):
    """JLCPCB's parts sit at a different zero rotation than KiCad's for some packages (the community table every
    KiCad-to-JLCPCB tool uses: jlc_rotations.csv, from JLCKicadTools). Returns (degrees, dx, dy)."""
    import re
    with open(os.path.join(HERE, "jlc_rotations.csv")) as f:
        rows = list(csv.reader(f))[1:]
    for r in rows:
        if r and re.search(r[0], fp_name):
            return float(r[1]), float(r[2]) if len(r) > 2 and r[2].strip() else 0.0, \
                float(r[3]) if len(r) > 3 and r[3].strip() else 0.0
    return 0.0, 0.0, 0.0


def main():
    import math
    import pcbnew as K
    os.makedirs(OUT, exist_ok=True)
    groups = {}
    # the footprint ACTUALLY on the routed board decides the part (and carrier.py has to agree with it)
    bd = K.LoadBoard(os.path.join(HERE, "build", "pg1-carrier.kicad_pcb"))
    on_board = {f.GetReference(): (f.GetFPID().GetLibItemName().wx_str(), f.GetValue()) for f in bd.GetFootprints()}
    for ref, value, fp, mpn, pins, note in parts:
        if ref in SKIP:
            continue
        pkg = fp.split(":")[-1]
        if ref not in on_board or on_board[ref] != (pkg, value):
            raise SystemExit(f"{ref}: carrier.py says {value} {pkg}, the board has {on_board.get(ref)}")
        code = LCSC.get((value, pkg))
        if not code:
            raise SystemExit(f"no JLCPCB part for {ref} {value} {pkg}")
        g = groups.setdefault(code, [value, pkg, []])   # one line per JLCPCB part: no "matched twice" rows
        g[2].append(ref)
    with open(os.path.join(OUT, "pg1-carrier-bom-jlc.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for code, (value, pkg, refs) in groups.items():
            w.writerow([value, ",".join(refs), pkg, code])
    # placement, straight from the board, in the same coordinates as the Gerbers (KiCad's own, Y up)
    b = K.LoadBoard(os.path.join(HERE, "build", "pg1-carrier.kicad_pcb"))
    with open(os.path.join(OUT, "pg1-carrier-cpl-jlc.csv"), "w", newline="") as o:
        w = csv.writer(o)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for fp in sorted(b.GetFootprints(), key=lambda f: f.GetReference()):
            ref = fp.GetReference()
            if ref in SKIP:
                continue
            name = fp.GetFPID().GetLibItemName().wx_str() if hasattr(fp.GetFPID().GetLibItemName(), "wx_str") else str(fp.GetFPID().GetLibItemName())
            tht = any(p.GetAttribute() == K.PAD_ATTRIB_PTH for p in fp.Pads())
            if tht:   # through-hole: JLCPCB wants the part's middle, KiCad's origin is pin 1: use the middle of its pins
                xs = [K.ToMM(p.GetPosition().x) for p in fp.Pads()]
                ys = [K.ToMM(p.GetPosition().y) for p in fp.Pads()]
                x, y = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
            else:
                x, y = K.ToMM(fp.GetPosition().x), K.ToMM(fp.GetPosition().y)
            rot = fp.GetOrientationDegrees()
            dr, dx, dy = rotation_fix(name)
            if dx or dy:   # offsets are in the part's own frame
                a = math.radians(rot)
                x += dx * math.cos(a) + dy * math.sin(a)
                y += -dx * math.sin(a) + dy * math.cos(a)
            w.writerow([ref, f"{x:.4f}mm", f"{-y:.4f}mm", "Top" if fp.GetLayer() == K.F_Cu else "Bottom",
                        f"{(rot + dr) % 360:.1f}"])
    shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier-gerbers.zip"), os.path.join(OUT, "pg1-carrier-gerbers.zip"))
    n = sum(len(r[2]) for r in groups.values())
    print(f"jlcpcb/: {len(groups)} part kinds, {n} parts (through-hole parts left for you: {", ".join(sorted(SKIP - DNP))}; not fitted: {", ".join(sorted(DNP))})")


if __name__ == "__main__":
    main()
