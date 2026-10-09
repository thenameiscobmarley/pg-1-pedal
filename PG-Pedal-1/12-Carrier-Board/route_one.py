#!/usr/bin/env python3
"""Routes ONE missing connection on the routed board (a small maze router, both layers, vias allowed), keeping to the
board's rules (0.2 mm track, 0.127 mm clearance, 0.6 / 0.3 vias). For the odd link Freerouting gives up on.

    python3 route_one.py U15 5      # from pad 5 of U15 to the nearest copper of its own net"""
import heapq, math, sys
import pcbnew as K

PCB = "build/pg1-carrier.kicad_pcb"
TRACK, CLEAR, VIA_D, VIA_DR, STEP = 0.2, 0.15, 0.6, 0.3, 0.1   # (0.15: a little over the 0.127 rule)
mm = K.ToMM


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    L = dx * dx + dy * dy
    t = 0 if L == 0 else max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / L))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def main(ref, padnum, margin=14.0, away=None):
    """away: (x0, x1, y0, y1) face mm: its own net's copper in there doesn't count as reached (an island's own pads)"""
    b = K.LoadBoard(PCB)
    fp = b.FindFootprintByReference(ref)
    pad = [p for p in fp.Pads() if p.GetNumber() == padnum][0]
    net = pad.GetNetCode()
    sx, sy = mm(pad.GetPosition().x), mm(pad.GetPosition().y)
    # copper on each layer: (kind, geometry, net)
    cu = {K.F_Cu: [], K.B_Cu: []}
    for t in b.GetTracks():
        if t.GetClass() == "PCB_VIA":
            for L in cu:
                cu[L].append(("c", (mm(t.GetPosition().x), mm(t.GetPosition().y), mm(t.GetWidth()) / 2), t.GetNetCode()))
        else:
            cu[t.GetLayer()].append(("s", (mm(t.GetStart().x), mm(t.GetStart().y), mm(t.GetEnd().x), mm(t.GetEnd().y), mm(t.GetWidth()) / 2), t.GetNetCode()))
    for f in b.GetFootprints():
        for p in f.Pads():
            bb = p.GetBoundingBox()
            r = (mm(bb.GetLeft()), mm(bb.GetTop()), mm(bb.GetRight()), mm(bb.GetBottom()))
            for L in cu:
                if p.IsOnLayer(L):
                    cu[L].append(("r", r, p.GetNetCode()))
    for d in b.Drawings():
        if d.GetLayer() == K.Edge_Cuts:
            for L in cu:
                cu[L].append(("s", (mm(d.GetStart().x), mm(d.GetStart().y), mm(d.GetEnd().x), mm(d.GetEnd().y), 0.0), -1))
    # the nearest own-net copper (not this pad) decides the search window
    own = [(L, g) for L in cu for k, g, n in cu[L] if n == net and not (k == "r" and abs((g[0] + g[2]) / 2 - sx) < 0.05 and abs((g[1] + g[3]) / 2 - sy) < 0.05)]
    def gdist(k, g, x, y):
        if k == "s":
            return seg_dist(x, y, *g[:4]) - g[4]
        if k == "c":
            return math.hypot(x - g[0], y - g[1]) - g[2]
        return math.hypot(max(g[0] - x, 0, x - g[2]), max(g[1] - y, 0, y - g[3]))
    tx = min(own, key=lambda q: gdist(q[1] and ("s" if len(q[1]) == 5 else "c" if len(q[1]) == 3 else "r"), q[1], sx, sy))
    x0, y0 = sx - margin, sy - margin
    nx, ny = int(2 * margin / STEP), int(2 * margin / STEP)
    kind = lambda g: "s" if len(g) == 5 else ("c" if len(g) == 3 else "r")
    blocked, goal = {}, {}
    for L in cu:
        bl, go = set(), set()
        for k, g, n in cu[L]:
            if k == "s":
                gx0, gy0, gx1, gy1 = min(g[0], g[2]) - g[4], min(g[1], g[3]) - g[4], max(g[0], g[2]) + g[4], max(g[1], g[3]) + g[4]
            elif k == "c":
                gx0, gy0, gx1, gy1 = g[0] - g[2], g[1] - g[2], g[0] + g[2], g[1] + g[2]
            else:
                gx0, gy0, gx1, gy1 = g
            pad_ = TRACK / 2 + CLEAR + 0.05
            i0, i1 = max(0, int((gx0 - pad_ - x0) / STEP)), min(nx, int((gx1 + pad_ - x0) / STEP) + 1)
            j0, j1 = max(0, int((gy0 - pad_ - y0) / STEP)), min(ny, int((gy1 + pad_ - y0) / STEP) + 1)
            for i in range(i0, i1):
                for j in range(j0, j1):
                    x, y = x0 + i * STEP, y0 + j * STEP
                    dd = gdist(k, g, x, y)
                    if n == net and n > 0:
                        fxx, fyy = x - 100.0, 100.0 - y
                        if away and away[0] <= fxx <= away[1] and away[2] <= fyy <= away[3]:
                            continue
                        if dd < -0.02:
                            go.add((i, j))
                    elif dd < TRACK / 2 + CLEAR:
                        bl.add((i, j))
        blocked[L], goal[L] = bl, go
    via_bl = set()   # a via needs room on both layers
    r_via = VIA_D / 2 + CLEAR - TRACK / 2 - CLEAR
    for L in cu:
        for (i, j) in blocked[L]:
            pass
    holes = [(mm(p.GetPosition().x), mm(p.GetPosition().y), mm(p.GetDrillSize().x) / 2) for f in b.GetFootprints() for p in f.Pads()
             if p.GetDrillSize().x > 0] + [(mm(t.GetPosition().x), mm(t.GetPosition().y), mm(t.GetDrill()) / 2) for t in b.GetTracks()
                                           if t.GetClass() == "PCB_VIA"]
    def via_ok(i, j):
        x, y = x0 + i * STEP, y0 + j * STEP
        if any(math.hypot(x - hx, y - hy) < hr + VIA_DR / 2 + 0.3 for hx, hy, hr in holes):   # drill to drill (any net)
            return False
        for L in cu:
            for k, g, n in cu[L]:
                if n == net:
                    continue
                if k == "s" and not (min(g[0], g[2]) - 2 < x < max(g[0], g[2]) + 2 and min(g[1], g[3]) - 2 < y < max(g[1], g[3]) + 2):
                    continue
                if k == "r" and not (g[0] - 2 < x < g[2] + 2 and g[1] - 2 < y < g[3] + 2):
                    continue
                if k == "c" and abs(g[0] - x) > 2 and abs(g[1] - y) > 2:
                    continue
                if gdist(k, g, x, y) < VIA_D / 2 + CLEAR:
                    return False
        return True
    si, sj = int(round((sx - x0) / STEP)), int(round((sy - y0) / STEP))
    start = [(K.F_Cu if pad.IsOnLayer(K.F_Cu) else K.B_Cu, si, sj)] + ([(K.B_Cu, si, sj)] if pad.GetAttribute() == K.PAD_ATTRIB_PTH else [])
    pq, best, par = [], {}, {}
    for s in start:
        heapq.heappush(pq, (0.0, s)); best[s] = 0.0; par[s] = None
    end = None
    viacache = {}
    while pq:
        c, s = heapq.heappop(pq)
        if c > best.get(s, 1e9) + 1e-9:
            continue
        L, i, j = s
        if (i, j) in goal[L] and c > 0.3:
            end = s
            break
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
            ni, nj = i + di, j + dj
            if not (0 <= ni < nx and 0 <= nj < ny):
                continue
            n2 = (L, ni, nj)
            # leaving the start pad is allowed through its own clearance zone
            if (ni, nj) in blocked[L]:
                continue
            nc = c + math.hypot(di, dj) * STEP
            if nc < best.get(n2, 1e9):
                best[n2] = nc; par[n2] = s; heapq.heappush(pq, (nc, n2))
        oL = K.B_Cu if L == K.F_Cu else K.F_Cu
        n2 = (oL, i, j)
        if (i, j) not in blocked[oL]:
            ok = viacache.get((i, j))
            if ok is None:
                ok = viacache[(i, j)] = via_ok(i, j)
            if ok:
                nc = c + 1.5
                if nc < best.get(n2, 1e9):
                    best[n2] = nc; par[n2] = s; heapq.heappush(pq, (nc, n2))
    if end is None:
        sys.exit("no path")
    path = []
    s = end
    while s:
        path.append(s); s = par[s]
    path.reverse()
    # make tracks: merge straight runs, a via at each layer change
    pts = lambda s: K.VECTOR2I(K.FromMM(x0 + s[1] * STEP), K.FromMM(y0 + s[2] * STEP))
    run = [path[0]]
    added = 0
    def flush(run):
        nonlocal added
        corners = [run[0]]
        for k in range(1, len(run) - 1):
            d1 = (run[k][1] - run[k - 1][1], run[k][2] - run[k - 1][2]); d2 = (run[k + 1][1] - run[k][1], run[k + 1][2] - run[k][2])
            if d1 != d2:
                corners.append(run[k])
        corners.append(run[-1])
        for a_, b_ in zip(corners, corners[1:]):
            t = K.PCB_TRACK(b); t.SetStart(pts(a_)); t.SetEnd(pts(b_)); t.SetWidth(K.FromMM(TRACK)); t.SetLayer(a_[0]); t.SetNetCode(net)
            b.Add(t); added += 1
    for s in path[1:]:
        if s[0] != run[-1][0]:
            if len(run) > 1:
                flush(run)
            v = K.PCB_VIA(b); v.SetPosition(pts(s)); v.SetWidth(K.FromMM(VIA_D)); v.SetDrill(K.FromMM(VIA_DR)); v.SetNetCode(net); b.Add(v)
            added += 1
            run = [s]
        else:
            run.append(s)
    if len(run) > 1:
        flush(run)
    # the first piece starts at the pad's centre
    K.ZONE_FILLER(b).Fill(b.Zones())
    b.Save(PCB)
    print(f"routed {ref}.{padnum}: {added} pieces, {best[end]:.1f} mm")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], away=tuple(map(float, sys.argv[3:7])) if len(sys.argv) >= 7 else None)
