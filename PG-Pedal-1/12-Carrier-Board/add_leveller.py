#!/usr/bin/env python3
"""Adds the analog leveller (carrier.py: U15 U16 Q1 OC1 OC2 R80-R85 C80-C85) to the ROUTED board without moving
anything already on it: each new part goes in the free space nearest the parts it connects to (the LED + LDR pairs
on the lid side, soldered on the parts side), then Freerouting routes only what's missing.

    python3 add_leveller.py --place-only   # place, save, render build/leveller-placed.png
    python3 add_leveller.py                # place + route + pours + DRC (backup: build/pg1-carrier.before-leveller.kicad_pcb)
Then: python3 regen_outputs.py && python3 make_jlc.py"""
import argparse, math, os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pcbnew as K   # noqa: E402
from carrier import parts, ISOLATED   # noqa: E402
from make_board import load, put, kpt, face, OUTLINE, BARRIER, DC_ZONE, JAVA, FREEROUTING, clip_silk_at_pads   # noqa: E402

PCB = os.path.join(HERE, "build", "pg1-carrier.kicad_pcb")
BACKUP = os.path.join(HERE, "build", "pg1-carrier.before-leveller.kicad_pcb")
NEW = ["OC1", "OC2", "U16", "C83", "R80", "R85", "C80", "C85", "Q1", "R83", "R82", "U15", "C82"]
STICK = {"C83": "U16", "C82": "U15"}   # decoupling caps: right at their chip
WEIGHT = {"LVA_L": 6.0, "LVA_R": 6.0, "LED_DAC": 6.0}   # (and the DAC next to its transistor: I2C can travel)   # high-impedance nodes: keep them short (the follower's output can travel)
PULL = {"LVA_L": "LO_L", "LVA_R": "LO_R", "LVC_L": "LO_L", "LVC_R": "LO_R"}   # the LDR nodes belong next to the codec's outputs
BACK = {"OC1", "OC2"}         # bodies on the lid side
CELL, GAP = 0.25, 0.25
ISO_Y = BARRIER[1] + 0.5      # isolated side only
NAMES = {"U16": "TLV9062", "U15": "MCP4725", "Q1": "3904"}
LUGS_Y = 58.0                 # the panel DC jack's solder lugs hang over the board below this (its back end, ~y 52.5)
# (under the DC jack's body, 2.3 mm up, the board allows 0402s and the 0.8 mm WSON op-amp)


def inside(x, y):
    c = False
    for k in range(len(OUTLINE)):
        (ax, ay), (bx, by) = OUTLINE[k], OUTLINE[(k + 1) % len(OUTLINE)]
        if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
            c = not c
    return c


def frect(bb):
    return (K.ToMM(bb.GetLeft()) - 100.0, 100.0 - K.ToMM(bb.GetBottom()), K.ToMM(bb.GetRight()) - 100.0, 100.0 - K.ToMM(bb.GetTop()))


X0, Y0, X1, Y1 = -56.0, 10.0, 56.0, 71.0
NX, NY = int((X1 - X0) / CELL), int((Y1 - Y0) / CELL)


class Occ:
    """blocked cells: 'F' copper side (parts, pads, tracks, vias, printing), 'B' copper (pads, tracks, vias),
    'Bbody' the lid side for a part's body (only other parts' pins / bodies block it). Off the board (0.5 mm in from
    the edge) and the pedal side count as blocked. Rectangle checks use summed-area tables."""
    def __init__(self, b):
        self.g = {k: [[0] * NX for _ in range(NY)] for k in ("F", "B", "Bbody")}
        self.sat = {}
        for j in range(NY):
            for i in range(NX):
                x, y = X0 + (i + 0.5) * CELL, Y0 + (j + 0.5) * CELL
                if y < ISO_Y or not all(inside(x + dx, y + dy) for dx, dy in ((0.5, 0), (-0.5, 0), (0, 0.5), (0, -0.5), (0, 0))):
                    for k in self.g:
                        self.g[k][j][i] = 1
        for fp in b.GetFootprints():
            self.add_fp(fp)
        for t in b.GetTracks():
            r = frect(t.GetBoundingBox())
            if t.GetClass() == "PCB_VIA":
                self.mark("F", r, 0.3), self.mark("B", r, 0.3)
            else:
                self.mark("F" if t.GetLayer() == K.F_Cu else "B", r, 0.25)
        for d in b.Drawings():
            if d.GetLayer() == K.F_SilkS and d.GetClass() == "PCB_TEXT":
                self.mark("F", frect(d.GetBoundingBox()), 0.15)
        for z in b.Zones():
            if z.GetIsRuleArea():
                r = frect(z.GetBoundingBox())
                for k in self.g:
                    self.mark(k, r, 0.1)

    def span(self, r, m):
        return (max(0, math.floor((r[0] - m - X0) / CELL)), max(0, math.floor((r[1] - m - Y0) / CELL)),
                min(NX, math.ceil((r[2] + m - X0) / CELL)), min(NY, math.ceil((r[3] + m - Y0) / CELL)))

    def mark(self, k, r, m):
        i0, j0, i1, j1 = self.span(r, m)
        g = self.g[k]
        for j in range(j0, j1):
            row = g[j]
            for i in range(i0, i1):
                row[i] = 1
        self.sat.pop(k, None)

    def add_fp(self, fp):
        fp.BuildCourtyardCaches()
        for L, k in ((K.F_CrtYd, "F"), (K.B_CrtYd, "Bbody")):
            c = fp.GetCourtyard(L)
            if c.OutlineCount():
                self.mark(k, frect(c.BBox()), GAP)
        for p in fp.Pads():
            r = frect(p.GetBoundingBox())
            if p.GetAttribute() == K.PAD_ATTRIB_PTH:
                for k in self.g:
                    self.mark(k, r, 0.35)
            elif p.IsOnLayer(K.F_Cu):
                self.mark("F", r, 0.3)

    def free(self, k, r, m):
        if k not in self.sat:
            g, S = self.g[k], [[0] * (NX + 1) for _ in range(NY + 1)]
            for j in range(NY):
                acc, row, prev, cur = 0, g[j], S[j], S[j + 1]
                for i in range(NX):
                    acc += row[i]
                    cur[i + 1] = prev[i + 1] + acc
            self.sat[k] = S
        i0, j0, i1, j1 = self.span(r, m)
        if (r[0] - m) < X0 or (r[2] + m) > X1 or (r[1] - m) < Y0 or (r[3] + m) > Y1:
            return False
        S = self.sat[k]
        return S[j1][i1] - S[j0][i1] - S[j1][i0] + S[j0][i0] == 0


def stitch_islands(b):
    """a piece of ground pour cut off by the new tracks (none of its net's pads / vias / tracks in it): a via joins it
    to the pour on the other side. Returns how many were added."""
    added = 0
    for z in [z for z in b.Zones() if not z.GetIsRuleArea()]:
        net = z.GetNetCode()
        # a pour piece is tied down if a via or a through-hole pin of its net is in it (both layers meet there)
        items = [p.GetPosition() for fp in b.GetFootprints() for p in fp.Pads() if p.GetNetCode() == net and p.GetAttribute() == K.PAD_ATTRIB_PTH]
        items += [t.GetPosition() for t in b.GetTracks() if t.GetNetCode() == net and t.GetClass() == "PCB_VIA"]
        for L in (K.F_Cu, K.B_Cu):
            if not z.IsOnLayer(L):
                continue
            other = K.B_Cu if L == K.F_Cu else K.F_Cu
            fill = z.GetFilledPolysList(L)
            for i in range(fill.OutlineCount()):
                ol = fill.Outline(i)
                if any(ol.PointInside(q) for q in items):
                    continue
                bb = ol.BBox()
                spot = None
                for yy in range(bb.GetTop(), bb.GetBottom(), K.FromMM(0.25)):
                    for xx in range(bb.GetLeft(), bb.GetRight(), K.FromMM(0.25)):
                        c = K.VECTOR2I(xx, yy)
                        ring = [K.VECTOR2I(xx + int(K.FromMM(0.5) * math.cos(a)), yy + int(K.FromMM(0.5) * math.sin(a)))
                                for a in [k * math.pi / 4 for k in range(8)]] + [c]
                        if all(fill.Contains(q) for q in ring) and \
                                any(z2.GetNetCode() == net and z2.IsOnLayer(other) and all(z2.GetFilledPolysList(other).Contains(q) for q in ring)
                                    for z2 in b.Zones() if not z2.GetIsRuleArea()):
                            spot = c
                            break
                    if spot:
                        break
                if spot:
                    v = K.PCB_VIA(b)
                    v.SetPosition(spot), v.SetWidth(K.FromMM(0.6)), v.SetDrill(K.FromMM(0.3)), v.SetNetCode(net)
                    b.Add(v)
                    items.append(spot)
                    added += 1
                    print(f"  stitched a cut-off {z.GetNetname()} pour piece at {face(spot)}")
    return added


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--place-only", action="store_true")
    a.add_argument("--passes", type=int, default=20)
    a = a.parse_args()
    if not os.path.exists(BACKUP):
        shutil.copy(PCB, BACKUP)
    # the board's rules live in its project file (make_board.write_project): the backup needs its own copy, or KiCad
    # loads it with default rules (0.2 mm) and the router + DRC see violations everywhere
    pro = PCB[:-10] + ".kicad_pro"
    shutil.copy(os.path.join(HERE, "pcbway", "pg1-carrier.kicad_pro"), pro)
    shutil.copy(pro, BACKUP[:-10] + ".kicad_pro")
    b = K.LoadBoard(BACKUP)
    have = {fp.GetReference() for fp in b.GetFootprints()}
    spec = {p[0]: p for p in parts}
    nets = {}
    for n in b.GetNetsByName().values() if hasattr(b.GetNetsByName(), "values") else []:
        pass
    def net(name):
        ni = b.FindNet(name)
        if ni is None:
            ni = K.NETINFO_ITEM(b, name)
            b.Add(ni)
        return ni
    # where each net already has pads (the anchors new parts are pulled toward)
    anchor = {}
    for fp in b.GetFootprints():
        for p in fp.Pads():
            n = p.GetNetname()
            if n:
                anchor.setdefault(n, []).append(face(p.GetPosition()))
    occ = Occ(b)
    placed, chip_at = {}, {}
    for ref in NEW:
        if ref in have:
            sys.exit(f"{ref} is already on the board (start from the backup)")
        r_, val, fpid, mpn, pins, note = spec[ref]
        fp = load(fpid)
        fp.SetReference(ref), fp.SetValue(val)
        fp.Reference().SetVisible(False)
        b.Add(fp)
        for p in fp.Pads():
            n = pins.get(int(p.GetNumber())) if p.GetNumber().isdigit() else None
            if n:
                p.SetNet(net(n))
        side = "B" if ref in BACK else "F"
        best = None
        sig = [n for n in pins.values() if n not in ("IGND",)]
        for rot in (0, 90, 180, 270):
            put(fp, 0, 0, rot, side)
            fp.BuildCourtyardCaches()
            cy = fp.GetCourtyard(K.B_CrtYd if side == "B" else K.F_CrtYd).BBox()
            box0 = frect(cy)
            pads0 = [(p.GetNetname(), face(p.GetPosition()), frect(p.GetBoundingBox())) for p in fp.Pads()]
            for j in range(int(ISO_Y / CELL), int(70.5 / CELL)):
                for i in range(int(-56 / CELL), int(56 / CELL)):
                    x, y = i * CELL, j * CELL
                    box = (box0[0] + x, box0[1] + y, box0[2] + x, box0[3] + y)
                    if side == "F":
                        if not occ.free("F", box, 0.0):
                            continue
                        if box[0] < DC_ZONE[2] and DC_ZONE[0] < box[2] and box[1] < DC_ZONE[3] and DC_ZONE[1] < box[3] \
                                and not fp.GetFPID().GetLibItemName().wx_str().startswith(("R_0402", "C_0402", "Texas_DSG")):
                            continue
                    else:
                        if not occ.free("Bbody", box, 0.0):
                            continue
                        if any(not occ.free("F", (q[0] + x, q[1] + y, q[2] + x, q[3] + y), 0.05) for _, _, q in pads0):
                            continue   # (lid-side tracks in the way are taken up below and re-routed)
                        if any(q[0] + x < DC_ZONE[2] and DC_ZONE[0] < q[2] + x and q[1] + y < LUGS_Y and DC_ZONE[1] < q[3] + y
                               for _, _, q in pads0):
                            continue   # under the DC jack's body (2.3 mm up) clipped leads (~1 mm) are fine; not under its lugs
                    cost = 0.0
                    if ref in STICK:
                        cx, cy = chip_at[STICK[ref]]
                        cost = math.hypot((box[0] + box[2]) / 2 - cx, (box[1] + box[3]) / 2 - cy)
                    for n, (px, py), _ in ([] if ref in STICK else pads0):
                        targets = anchor.get(n) or anchor.get(PULL.get(n, ""), [])   # (pulled only until the net has pads)
                        if n in ("IGND", "") or not targets:
                            continue
                        cost += WEIGHT.get(n, 1.0) * min(math.hypot(px + x - ax, py + y - ay) for ax, ay in targets)
                    if best is None or cost < best[0]:
                        best = (cost, x, y, rot)
        if best is None:
            sys.exit(f"no room for {ref}")
        cost, x, y, rot = best
        put(fp, x, y, rot, side)
        if side == "B":   # lid-side track pieces under the new pins: take them up (the router reconnects them)
            pr = [frect(p.GetBoundingBox()) for p in fp.Pads()]
            gone = [t for t in b.GetTracks() if t.GetClass() != "PCB_VIA" and t.GetLayer() == K.B_Cu and
                    any(q[0] - 0.45 < r[2] and r[0] < q[2] + 0.45 and q[1] - 0.45 < r[3] and r[1] < q[3] + 0.45
                        for q in pr for r in [frect(t.GetBoundingBox())])]
            for t in gone:
                b.Remove(t)
            if gone:
                print(f"       ({len(gone)} lid-side track pieces taken up for {ref})")
        occ.add_fp(fp)
        for p in fp.Pads():
            if p.GetNetname():
                anchor.setdefault(p.GetNetname(), []).append(face(p.GetPosition()))
        placed[ref] = (x, y, rot, side)
        chip_at[ref] = (x, y)
        print(f"  {ref:4s} at ({x:6.2f}, {y:6.2f}) rot {rot:3d} {side}  wire {cost:5.1f} mm")
    # part names on the board, like the rest: in the nearest clear spot (off pads and other printing)
    clip_silk_at_pads(b)
    blocks = []
    for fp in b.GetFootprints():
        for p in fp.Pads():
            q = frect(p.GetBoundingBox())
            blocks.append((q[0] - 0.2, q[1] - 0.2, q[2] + 0.2, q[3] + 0.2))
        for g in fp.GraphicalItems():
            if g.GetLayer() == K.F_SilkS:
                q = frect(g.GetBoundingBox())
                blocks.append((q[0] - 0.1, q[1] - 0.1, q[2] + 0.1, q[3] + 0.1))
    for d in b.Drawings():
        if d.GetLayer() == K.F_SilkS:
            q = frect(d.GetBoundingBox())
            blocks.append((q[0] - 0.1, q[1] - 0.1, q[2] + 0.1, q[3] + 0.1))
    hit = lambda r, q: r[0] < q[2] and q[0] < r[2] and r[1] < q[3] and q[1] < r[3]
    for ref, name in NAMES.items():
        fp = b.FindFootprintByReference(ref)
        fp.BuildCourtyardCaches()
        c = frect(fp.GetCourtyard(K.F_CrtYd).BBox())
        cx, cy = (c[0] + c[2]) / 2, (c[1] + c[3]) / 2
        cands = [(cx, cy)] + [p for d in (0.6, 1.0, 1.5, 2.2, 3.0)
                              for p in ((cx, c[3] + d), (cx, c[1] - d), (c[0] - d - 1.6, cy), (c[2] + d + 1.6, cy))]
        done = False
        for x, y in cands:
            for rot in (0, 90):
                t = K.PCB_TEXT(b)
                t.SetText(name), t.SetLayer(K.F_SilkS)
                t.SetTextSize(K.VECTOR2I(K.FromMM(0.7), K.FromMM(0.7))), t.SetTextThickness(K.FromMM(0.12))
                t.SetPosition(kpt(x, y)), t.SetTextAngle(K.EDA_ANGLE(rot, K.DEGREES_T))
                t.SetHorizJustify(K.GR_TEXT_H_ALIGN_CENTER), t.SetVertJustify(K.GR_TEXT_V_ALIGN_CENTER)
                r = frect(t.GetBoundingBox())
                if any(hit(r, q) for q in blocks) or not all(inside(px, py) for px, py in ((r[0], r[1]), (r[2], r[3]), (r[0], r[3]), (r[2], r[1]))):
                    continue
                b.Add(t)
                blocks.append((r[0] - 0.15, r[1] - 0.15, r[2] + 0.15, r[3] + 0.15))
                done = True
                break
            if done:
                break
        print(f"  label {name}: {'ok' if done else 'NO ROOM'}")
    K.SaveBoard(PCB, b)
    shutil.copy(BACKUP[:-10] + ".kicad_pro", pro)   # (SaveBoard rewrites the project file)
    if a.place_only:
        subprocess.run(["kicad-cli", "pcb", "render", "--side", "top", "--width", "1800", "--height", "1100", "--quality", "basic",
                        "-o", os.path.join(HERE, "build", "leveller-placed.png"), PCB], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return
    # route only what's missing (everything already routed comes along in the DSN as it is)
    dsn, ses = os.path.join(HERE, "build", "leveller.dsn"), os.path.join(HERE, "build", "leveller.ses")
    if os.path.exists(ses):
        os.remove(ses)
    for t in b.GetTracks():
        t.SetLocked(True)   # exported as fixed wiring: the router works round it
    K.ExportSpecctraDSN(b, dsn)
    subprocess.run([JAVA, "-Djava.awt.headless=true", "-jar", FREEROUTING, "-de", dsn, "-do", ses, "-mp", str(a.passes),
                    "-mt", str(max(1, (os.cpu_count() or 2) - 1))],
                   stdout=open(os.path.join(HERE, "build", "freerouting-leveller.log"), "w"), stderr=subprocess.STDOUT, timeout=1800)
    if not os.path.exists(ses):
        sys.exit("freerouting wrote no session (build/freerouting-leveller.log)")
    before = {(K.ToMM(t.GetStart().x), K.ToMM(t.GetStart().y), K.ToMM(t.GetEnd().x), K.ToMM(t.GetEnd().y)) for t in b.GetTracks()}
    K.ImportSpecctraSES(b, ses)
    after = {(K.ToMM(t.GetStart().x), K.ToMM(t.GetStart().y), K.ToMM(t.GetEnd().x), K.ToMM(t.GetEnd().y)) for t in b.GetTracks()}
    print(f"tracks: {len(before)} before, {len(after)} after, {len(before - after)} of the old ones changed")
    for t in b.GetTracks():
        t.SetLocked(False)
        if t.GetClass() != "PCB_VIA" and t.GetWidth() < K.FromMM(0.127):
            t.SetWidth(K.FromMM(0.127))
    K.ZONE_FILLER(b).Fill(b.Zones())
    for _ in range(3):
        if not stitch_islands(b):
            break
        K.ZONE_FILLER(b).Fill(b.Zones())
    K.SaveBoard(PCB, b)
    shutil.copy(BACKUP[:-10] + ".kicad_pro", pro)
    import json
    drc = os.path.join(HERE, "build", "drc-leveller.json")
    subprocess.run(["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all", "--refill-zones", "--save-board", "-o", drc, PCB],
                   stdout=subprocess.DEVNULL)
    rep = json.load(open(drc))
    by = {}
    for v in rep.get("violations", []):
        by[(v["severity"], v["type"])] = by.get((v["severity"], v["type"]), 0) + 1
    print(f"DRC: {len(rep.get('unconnected_items', []))} unconnected, " +
          (", ".join(f"{n} {s} {t}" for (s, t), n in sorted(by.items())) or "no violations"))


if __name__ == "__main__":
    main()
