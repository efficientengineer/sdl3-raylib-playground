#!/usr/bin/env python3
# Regenerates halm.map and hart_yard.map. Authoring tool; the .map files are the deliverable and can
# also be hand-edited. Run from anywhere: python3 story/field/maps/_generate.py
# Method and the width rules are documented in src/FIELD_NOTES.md ("Map design").
# Everything is built from desire lines: junction hulls + offset ribbons, so every shared navmesh
# edge is an exact vertex match while nothing is axis-aligned. See FIELD.md "Map design rules".
import math, os, sys, zlib, struct

OUT = os.path.dirname(os.path.abspath(__file__))

def sub(a, b): return (a[0] - b[0], a[1] - b[1])
def add(a, b): return (a[0] + b[0], a[1] + b[1])
def mul(a, k): return (a[0] * k, a[1] * k)
def norm(a):
    l = math.hypot(a[0], a[1])
    return (a[0] / l, a[1] / l) if l > 1e-9 else (1.0, 0.0)
def perp(a): return (-a[1], a[0])
def mid(e): return ((e[0][0] + e[1][0]) / 2.0, (e[0][1] + e[1][1]) / 2.0)

def hull(pts):
    pts = sorted(set((round(p[0], 4), round(p[1], 4)) for p in pts))
    def half(ps):
        out = []
        for p in ps:
            while len(out) >= 2 and (out[-1][0] - out[-2][0]) * (p[1] - out[-2][1]) - (out[-1][1] - out[-2][1]) * (p[0] - out[-2][0]) <= 0:
                out.pop()
            out.append(p)
        return out
    lo, up = half(pts), half(pts[::-1])
    return lo[:-1] + up[:-1]

def is_convex(poly, name):
    n, sign = len(poly), 0
    for i in range(n):
        a, b, c = poly[i], poly[(i + 1) % n], poly[(i + 2) % n]
        cr = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        if abs(cr) < 1e-7: continue
        s = 1 if cr > 0 else -1
        if sign and s != sign:
            print("  !! %s not convex at vertex %d" % (name, i))
            return False
        sign = s
    return True

def poly_contains(poly, x, z):
    n, sign = len(poly), 0
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        cr = (b[0] - a[0]) * (z - a[1]) - (b[1] - a[1]) * (x - a[0])
        if abs(cr) < 1e-9: continue
        s = 1 if cr > 0 else -1
        if sign and s != sign: return False
        sign = s
    return True

class Map:
    def __init__(s, w, h):
        s.w, s.h, s.polys = w, h, []
    def add(s, verts, kind=None, name="poly"):
        is_convex([(v[0], v[1]) for v in verts], name)
        s.polys.append((verts, kind))
    def nav_lines(s):
        return ["%d: %s%s" % (i + 1, ", ".join("%.2f %.2f %.2f" % v for v in vs), (" | " + k) if k else "")
                for i, (vs, k) in enumerate(s.polys)]
    def cover(s, x, z):
        for vs, k in s.polys:
            if poly_contains([(v[0], v[1]) for v in vs], x, z): return vs, k
        return None

def junction(P, spokes, R, height=0.0):
    """spokes: [(target, halfwidth)]. Each spoke's two points end up adjacent on the hull, so a
    ribbon can start on exactly that edge and the linker joins them."""
    pts, edges = [], {}
    for i, (target, hw) in enumerate(spokes):
        d = norm(sub(target, P))
        base = add(P, mul(d, R))
        A, B = add(base, mul(perp(d), hw)), sub(base, mul(perp(d), hw))
        edges[i] = (A, B)
        pts += [A, B]
    Hp = hull(pts)
    Hr = [(round(p[0], 4), round(p[1], 4)) for p in Hp]
    for i, (A, B) in edges.items():
        ar, br = (round(A[0], 4), round(A[1], 4)), (round(B[0], 4), round(B[1], 4))
        if ar not in Hr or br not in Hr:
            print("  !! spoke %d off the hull (raise R)" % i)
        elif abs(Hr.index(ar) - Hr.index(br)) not in (1, len(Hr) - 1):
            print("  !! spoke %d not an adjacent hull pair (raise R)" % i)
    return [(p[0], p[1], height) for p in Hp], edges

def ribbon(mp, pts, hws, heights, start_edge=None, end_edge=None, kind=None, name="ribbon"):
    """A bending corridor: one convex quad per segment, consecutive quads sharing a whole edge."""
    n = len(pts)
    L, R = [], []
    for i in range(n):
        if i == 0: d = norm(sub(pts[1], pts[0]))
        elif i == n - 1: d = norm(sub(pts[-1], pts[-2]))
        else: d = norm(add(norm(sub(pts[i], pts[i - 1])), norm(sub(pts[i + 1], pts[i]))))
        L.append(add(pts[i], mul(perp(d), hws[i])))
        R.append(sub(pts[i], mul(perp(d), hws[i])))
    def orient(edge, l, r):
        a, b = edge
        straight = math.hypot(a[0] - l[0], a[1] - l[1]) + math.hypot(b[0] - r[0], b[1] - r[1])
        swapped = math.hypot(b[0] - l[0], b[1] - l[1]) + math.hypot(a[0] - r[0], a[1] - r[1])
        return (a, b) if straight <= swapped else (b, a)
    if start_edge: L[0], R[0] = orient(start_edge, L[0], R[0])
    if end_edge: L[-1], R[-1] = orient(end_edge, L[-1], R[-1])
    for i in range(n - 1):
        seg = math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1])
        if seg < max(hws[i], hws[i + 1]) * 1.05:
            print("  !! %s seg %d is only %.2f long for width %.2f" % (name, i, seg, max(hws[i], hws[i + 1])))
        mp.add([(L[i][0], L[i][1], heights[i]), (L[i + 1][0], L[i + 1][1], heights[i + 1]),
                (R[i + 1][0], R[i + 1][1], heights[i + 1]), (R[i][0], R[i][1], heights[i])],
               kind, "%s seg %d" % (name, i))

def ring(mp, hull_poly, inner_k, height, kind):
    """Turns a junction hull into a ring round a central island, so the well keeps its own ground."""
    cx = sum(p[0] for p in hull_poly) / len(hull_poly)
    cz = sum(p[1] for p in hull_poly) / len(hull_poly)
    inner = [(cx + (p[0] - cx) * inner_k, cz + (p[1] - cz) * inner_k) for p in hull_poly]
    n = len(hull_poly)
    for i in range(n):
        j = (i + 1) % n
        mp.add([(hull_poly[i][0], hull_poly[i][1], height), (hull_poly[j][0], hull_poly[j][1], height),
                (inner[j][0], inner[j][1], height), (inner[i][0], inner[i][1], height)], kind, "ring %d" % i)

def face_rot(cx, cz, tx, tz):
    """Yaw in degrees putting the building's local +Z face toward (tx,tz)."""
    d = norm((tx - cx, tz - cz))
    return round(math.degrees(math.atan2(-d[0], d[1])), 1)

def near_polyline(pts, x, z, r):
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        ab = sub(b, a)
        L2 = ab[0] * ab[0] + ab[1] * ab[1]
        t = 0.0 if L2 < 1e-9 else max(0.0, min(1.0, ((x - a[0]) * ab[0] + (z - a[1]) * ab[1]) / L2))
        p = add(a, mul(ab, t))
        if math.hypot(x - p[0], z - p[1]) <= r: return True
    return False

def hchar(v): return str(v) if v < 10 else chr(ord('a') + v - 10)

# Blocker walls: a box extruded along a polyline, generated from the open navmesh edges so the
# walkable edge is the wall's inner face and nothing in the world is a paper-thin strip.
WALLKIND = {"wall":  ("wall_stone", "wall_stone_top", 0.45, 0.80),
            "fence": ("fence_wood", "fence_top",      0.22, 0.75),
            "hedge": ("hedge",      "hedge_top",      0.60, 0.95)}

def key(p): return (round(p[0], 3), round(p[1], 3))

def wall_runs(mp, default_kind, skip):
    """Open edges -> chained centrelines, offset outward by half the wall width."""
    polys = [ccw([(v[0], v[1]) for v in vs]) for vs, k in mp.polys]
    kinds = [k or default_kind for vs, k in mp.polys]
    edges = set()
    for i, p in enumerate(polys):
        for e in range(len(p)):
            edges.add((key(p[e]), key(p[(e + 1) % len(p)]), i, e))
    shared = set()
    lookup = {}
    for (a, b, i, e) in edges:
        lookup.setdefault(frozenset((a, b)), []).append((i, e))
    for fs, lst in lookup.items():
        if len(lst) > 1:
            for it in lst: shared.add(it)
    out = {}
    for (a, b, i, e) in edges:
        if (i, e) in shared: continue
        mx, mz = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        if skip(mx, mz, a, b): continue
        out.setdefault(kinds[i], []).append((a, b))
    runs = []
    for kind, segs in out.items():
        segs = list(segs)
        while segs:
            a, b = segs.pop(0)
            chain = [a, b]
            grew = True
            while grew:
                grew = False
                for j, (c, d) in enumerate(segs):
                    # only continue if the run keeps going the same way: a reversal here means two
                    # different boundaries happen to meet at this vertex, and joining them zig-zags
                    if c == chain[-1]:
                        prev = norm(sub(chain[-1], chain[-2]))
                        if prev[0] * norm(sub(d, c))[0] + prev[1] * norm(sub(d, c))[1] < -0.2: continue
                        chain.append(d); segs.pop(j); grew = True; break
                    if d == chain[0]:
                        nxt = norm(sub(chain[1], chain[0]))
                        if nxt[0] * norm(sub(d, c))[0] + nxt[1] * norm(sub(d, c))[1] < -0.2: continue
                        chain.insert(0, c); segs.pop(j); grew = True; break
            runs.append((kind, chain))
    return runs

def offset_run(chain, half):
    """Push a chain of nav-edge points outward by `half`, mitred, so the inner face sits on the edge."""
    n = len(chain)
    out = []
    for i in range(n):
        if i == 0: d = norm(sub(chain[1], chain[0]))
        elif i == n - 1: d = norm(sub(chain[-1], chain[-2]))
        else: d = norm(add(norm(sub(chain[i], chain[i - 1])), norm(sub(chain[i + 1], chain[i]))))
        nrm = (d[1], -d[0])                       # outward for a CCW polygon
        sc = 1.0
        if 0 < i < n - 1:
            a = norm(sub(chain[i], chain[i - 1]))
            sc = 1.0 / max(0.4, abs((a[1]) * nrm[0] + (-a[0]) * nrm[1]))
        out.append((chain[i][0] + nrm[0] * half * sc, chain[i][1] + nrm[1] * half * sc))
    return out

def wall_lines(mp, default_kind, skip):
    lines = []
    for kind, chain in wall_runs(mp, default_kind, skip):
        if kind not in WALLKIND or len(chain) < 2: continue
        side, top, w, h = WALLKIND[kind]
        pts = offset_run(chain, w / 2.0)
        body = ", ".join("%.2f %.2f" % p for p in pts)
        lines.append("%s box:%.2f:%.2f:%s:%s caps %s" % (kind, w, h, side, top, body))
    return lines

KEEP_SPLAT = "--keep-splat" in sys.argv

def write_png(path, w, h, rgba):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgba[y * w * 4:(y + 1) * w * 4]
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 6))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)

def ccw(poly):
    a = sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
            for i in range(len(poly)))
    return poly if a >= 0 else poly[::-1]

def sdf(poly, x, z):
    """Signed distance to a convex polygon: negative inside, positive outside."""
    best = -1e9
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        ex, ez = b[0] - a[0], b[1] - a[1]
        l = math.hypot(ex, ez)
        if l < 1e-9: continue
        d = -((ex * (z - a[1]) - ez * (x - a[0])) / l)
        if d > best: best = d
    return best

def ramp(d, inner, outer):
    if d <= inner: return 1.0
    if d >= outer: return 0.0
    t = (outer - d) / (outer - inner)
    return t * t * (3.0 - 2.0 * t)

def prep(polys):
    """(ccw vertices, bbox) for fast rejection."""
    out = []
    for p in polys:
        q = ccw([(v[0], v[1]) for v in p])
        xs = [v[0] for v in q]; zs = [v[1] for v in q]
        out.append((q, min(xs), min(zs), max(xs), max(zs)))
    return out

def splat(path, mp, W, H, n, stone_kinds, discs, stone_ring=0.0, shore=None):
    """Paints the four ground weights: R grass, G dirt, B stone, A plank (unused so far)."""
    stone = prep([vs for vs, k in mp.polys if k in stone_kinds])
    dirt = prep([vs for vs, k in mp.polys if k not in stone_kinds])
    px = bytearray(W * n * H * n * 4)
    for ty in range(H * n):
        wz = (ty + 0.5) / n
        for tx in range(W * n):
            wx = (tx + 0.5) / n
            ws = 0.0
            for q, x0, z0, x1, z1 in stone:
                if wx < x0 - 1.2 or wx > x1 + 1.2 or wz < z0 - 1.2 or wz > z1 + 1.2: continue
                v = ramp(sdf(q, wx, wz), -0.7, 0.8)
                if v > ws: ws = v
            wd = 0.0
            for q, x0, z0, x1, z1 in dirt:
                if wx < x0 - 1.8 or wx > x1 + 1.8 or wz < z0 - 1.8 or wz > z1 + 1.8: continue
                v = ramp(sdf(q, wx, wz), -0.4, 1.0)
                if v > wd: wd = v
                if stone_ring > 0.0:                      # a worn apron round the yard
                    a = ramp(sdf(q, wx, wz), -0.2, stone_ring)
                    if a > ws: ws = a
            if shore is not None:                         # the shoreline, painted not tiled
                for i in range(len(shore) - 1):
                    a, b = shore[i], shore[i + 1]
                    abx, abz = b[0] - a[0], b[1] - a[1]
                    L2 = abx * abx + abz * abz
                    t = 0.0 if L2 < 1e-9 else max(0.0, min(1.0, ((wx - a[0]) * abx + (wz - a[1]) * abz) / L2))
                    dd = math.hypot(wx - (a[0] + abx * t), wz - (a[1] + abz * t))
                    v = ramp(dd, 1.0, 2.4)
                    if v > wd: wd = v
            for (cx, cz, r, amt) in discs:                # worn patches: doorways, the well
                d = math.hypot(wx - cx, wz - cz)
                v = amt * ramp(d, r * 0.45, r)
                if v > wd: wd = v
            if stone_ring > 0.0: ws = max(0.0, ws - wd)
            wd = wd * (1.0 - ws)
            wg = max(0.0, 1.0 - ws - wd)
            o = (ty * W * n + tx) * 4
            px[o] = int(wg * 255); px[o + 1] = int(wd * 255); px[o + 2] = int(ws * 255); px[o + 3] = 0
    write_png(path, W * n, H * n, px)
    print("  splat %s %dx%d" % (os.path.basename(path), W * n, H * n))

# ═══════════════════════════════════════ HALM ═══════════════════════════════════════
# Origin point: the well. Every street below was drawn outward from it before a building was placed.
W, H = 40, 30
m = Map(W, H)
WELL = (18.6, 17.4)

sq_hull, sq_edge = junction(WELL, [((19.4, 13.8), 1.50),    # 0 the hill road
                                   ((15.0, 18.4), 1.35),    # 1 the west street
                                   ((22.6, 18.6), 1.45),    # 2 the east street
                                   ((17.6, 21.2), 1.05)],   # 3 the south lane: the one pinch
                           3.8)
ring(m, [(p[0], p[1]) for p in sq_hull], 0.34, 0.0, "wall")

N2 = (8.6, 20.4)
n2_hull, n2_edge = junction(N2, [((11.8, 19.2), 1.35), ((8.6, 16.6), 1.10), ((7.4, 22.6), 0.95)], 2.6)
m.add([(p[0], p[1], 0.0) for p in n2_hull], "hedge", "N2")
N3 = (29.0, 20.6)
n3_hull, n3_edge = junction(N3, [((25.6, 19.6), 1.45), ((28.6, 17.0), 1.00), ((29.4, 22.6), 1.00)], 2.6)
m.add([(p[0], p[1], 0.0) for p in n3_hull], "hedge", "N3")
N4 = (15.4, 25.4)
n4_hull, n4_edge = junction(N4, [((16.6, 23.4), 1.05), ((18.6, 26.2), 1.00)], 2.2)
m.add([(p[0], p[1], 0.0) for p in n4_hull], "hedge", "N4")

# Hart's hill terrace, split into three so only the gate piece loses its wall
RAMP_EDGE = ((24.8, 7.4), (21.0, 7.8))
T1 = [(16.2, 4.2), (20.6, 3.0), (21.0, 7.8), (16.9, 8.1)]
T2 = [(20.6, 3.0), (24.2, 2.6), (24.8, 7.4), (21.0, 7.8)]
T3 = [(24.2, 2.6), (28.4, 3.8), (28.5, 7.0), (24.8, 7.4)]
for p, nm in ((T1, "T1"), (T2, "T2"), (T3, "T3")):
    m.add([(v[0], v[1], 4.0) for v in p], "wall", nm)

# the grain yard: a lopsided walled enclosure with one gate
GATE_EDGE = ((9.9, 14.6), (7.5, 15.2))
GY1 = [(4.4, 10.6), (9.4, 10.0), (9.9, 14.6), (7.5, 15.2)]
GY2 = [(4.4, 10.6), (7.5, 15.2), (5.4, 16.3), (3.1, 13.0)]
m.add([(v[0], v[1], 0.0) for v in GY1], "wall", "GY1")
m.add([(v[0], v[1], 0.0) for v in GY2], "wall", "GY2")

# ---- the nine streets ----
ribbon(m, [mid(sq_edge[0]), (21.0, 10.6), (22.9, 7.6)],
       [1.50, 1.75, 1.91], [0.0, 1.6, 4.0], sq_edge[0], RAMP_EDGE, "hedge", "hill road")
ribbon(m, [mid(sq_edge[1]), (12.9, 18.6), mid(n2_edge[0])],
       [1.35, 1.30, 1.35], [0.0] * 3, sq_edge[1], n2_edge[0], "hedge", "west street")
ribbon(m, [mid(sq_edge[2]), (24.3, 18.6), mid(n3_edge[0])],
       [1.45, 1.40, 1.45], [0.0] * 3, sq_edge[2], n3_edge[0], "hedge", "east street")
ribbon(m, [mid(sq_edge[3]), mid(n4_edge[0])],
       [1.05, 1.05], [0.0] * 2, sq_edge[3], n4_edge[0], "hedge", "south lane")
ribbon(m, [mid(n2_edge[1]), mid(GATE_EDGE)],
       [1.10, 1.24], [0.0] * 2, n2_edge[1], GATE_EDGE, "hedge", "grain lane")
ribbon(m, [mid(n2_edge[2]), (5.8, 25.2), (4.6, 27.6)],
       [0.95, 1.00, 1.10], [0.0] * 3, n2_edge[2], None, "hedge", "west lane")
ribbon(m, [mid(n3_edge[1]), (30.0, 15.4), (31.6, 13.0)],
       [1.00, 1.05, 1.20], [0.0] * 3, n3_edge[1], None, "hedge", "shed lane")
ribbon(m, [mid(n3_edge[2]), (30.6, 25.8), (32.0, 27.9), (36.6, 28.9)],
       [1.00, 1.05, 1.30, 1.30], [0.0] * 4, n3_edge[2], None, "hedge", "ford lane")
ribbon(m, [mid(n4_edge[1]), (20.0, 26.8), (22.2, 27.4)],
       [1.00, 1.10, 1.40], [0.0] * 3, n4_edge[1], None, "hedge", "back yard")

# ---- terrain: the hill, and a stream that ignores every street ----
HILL = [(10.5, 0.0), (31.5, 0.0), (32.5, 5.0), (30.0, 8.6), (26.0, 8.1), (23.4, 9.6),
        (20.0, 8.9), (16.4, 9.6), (13.0, 7.2), (10.6, 3.4)]
STREAM = [(36.5, 0.0), (34.6, 6.0), (36.4, 11.5), (34.0, 17.0), (36.2, 22.0), (33.4, 26.5), (34.6, 30.0)]

def plane_h(vs, x, z):
    i0 = 0
    i1 = max(range(1, len(vs)), key=lambda i: (vs[i][0] - vs[0][0]) ** 2 + (vs[i][1] - vs[0][1]) ** 2)
    i2 = max((i for i in range(1, len(vs)) if i != i1),
             key=lambda i: abs((vs[i1][0] - vs[0][0]) * (vs[i][1] - vs[0][1]) - (vs[i1][1] - vs[0][1]) * (vs[i][0] - vs[0][0])))
    x1, z1, y1 = vs[i1][0] - vs[i0][0], vs[i1][1] - vs[i0][1], vs[i1][2] - vs[i0][2]
    x2, z2, y2 = vs[i2][0] - vs[i0][0], vs[i2][1] - vs[i0][1], vs[i2][2] - vs[i0][2]
    det = x1 * z2 - x2 * z1
    if abs(det) < 1e-6: return vs[0][2]
    pa = (y1 * z2 - y2 * z1) / det
    pb = (x1 * y2 - x2 * y1) / det
    pc = vs[i0][2] - pa * vs[i0][0] - pb * vs[i0][1]
    return pa * x + pb * z + pc

def height_at(x, z):
    c = m.cover(x, z)
    if c is not None: return max(0, int(round(plane_h(c[0], x, z))))
    return 4 if poly_contains(HILL, x, z) else 0

def ground_at(x, z):
    if near_polyline(STREAM, x, z, 0.9): return 'w'
    if near_polyline(STREAM, x, z, 1.5): return 'd'
    c = m.cover(x, z)
    if c is not None: return 's' if c[1] == "wall" else 'd'
    if 20.8 <= x <= 24.4 and z <= 3.4: return 'd'     # the road keeps going north, off the map
    return '.'

# ---- buildings: fronts to the path, none parallel to a neighbour, no uniform spacing ----
blds = [
    (22.4, 12.2, 5.0, 3.4, 3.6, "guild_hall",   WELL, 0.85, 0.42),
    (13.2, 12.8, 3.2, 2.6, 2.6, "house_a",      (16.6, 16.6)),
    (20.6, 20.8, 2.6, 2.2, 2.2, "house_b",      (18.8, 19.6)),
    (11.8, 14.6, 3.0, 2.4, 2.4, "house_a",      (13.4, 17.6)),
    (11.8, 22.2, 2.4, 2.2, 2.2, "house_b",      (14.4, 21.8)),
    (5.0, 7.0, 4.0, 2.6, 2.2, "grain_shed",     (7.2, 12.4)),
    (24.4, 22.6, 3.2, 2.4, 2.4, "house_a",      (26.8, 21.0)),
    (32.6, 15.6, 2.4, 2.2, 2.2, "house_b",      (31.0, 15.0)),
    (12.8, 3.8, 2.8, 2.4, 2.4, "ladder_house",  (16.8, 5.8)),
    (13.9, 5.6, 2.0, 2.6, 2.2, "ladder_house",  (16.8, 6.6)),   # the wing: an L out of two sweeps
    (19.8, 28.2, 2.4, 1.6, 2.2, "house_b",      (19.0, 27.0)),
]
bld_lines, rots = [], []
for b in blds:
    bx, bz, bw, bd, bh, bid, tgt = b[:7]
    pitch, eave = (b[7], b[8]) if len(b) > 8 else (0.55, 0.18)
    r = face_rot(bx + bw / 2, bz + bd / 2, tgt[0], tgt[1])
    rots.append(((bx, bz), r))
    bld_lines.append("%.1f %.1f %.1f %.1f %.1f %s rot %.1f pitch %.2f eave %.2f"
                     % (bx, bz, bw, bd, bh, bid, r, pitch, eave))
for (bx, bz, bw, bd, bh, bid, tgt) in [b[:7] for b in blds]:   # never sit on the navmesh
    for (ox, oz) in ((0.2, 0.2), (bw - 0.2, 0.2), (0.2, bd - 0.2), (bw - 0.2, bd - 0.2), (bw / 2, bd / 2)):
        if m.cover(bx + ox, bz + oz) is not None:
            print("  !! %s at %.1f,%.1f overlaps a walkable polygon" % (bid, bx, bz))
            break
for i in range(len(rots)):
    for j in range(i + 1, len(rots)):
        if math.hypot(rots[i][0][0] - rots[j][0][0], rots[i][0][1] - rots[j][0][1]) < 9.0:
            d = abs(rots[i][1] - rots[j][1]) % 180
            if min(d, 180 - d) < 8.0:
                print("  !! buildings %d,%d neighbours and near-parallel (%.1f / %.1f)" % (i, j, rots[i][1], rots[j][1]))

out = []
out += ["## meta", "name: halm", "size: %d %d" % (W, H), "spawn: 18 20 N",
        "ground: grass", "wall: cliff", "edges: fence",
        "landmark: the lopsided well square, and the road that climbs round the shoulder of Hart's hill",
        "splat: halm_splat.png 8",
        "splat_layers: grass:1 dirt:1:0.15 stone:8:0.6 plank:6:0.4",
        "tile_scale: 1 1 2 1",
        "splat_blend: dither", "",
        "## tiles", "d dirt", "s stone", "w water", "p plank", ""]
out += ["## height"] + ["".join(hchar(height_at(x + 0.5, z + 0.5)) for x in range(W)) for z in range(H)] + [""]
out += ["## ground"] + ["".join(ground_at(x + 0.5, z + 0.5) for x in range(W)) for z in range(H)] + [""]
out += ["## nav",
        "# id: x z h, ...  [| fence|hedge|wall|none]   h in half-steps, per vertex, so a ramp is one sloped poly"]
out += m.nav_lines() + [""]
out += ["## cameras",
        "# rect(x z w d) then the shot. Last definition wins on overlap; a landmark is in frame in each.",
        "# <id> x z w d <mode> ... — the id keys the capture and the painting for that zone",
        "base 0 0 40 30 follow 0 50 30 13 0.8",
        "square 14.0 13.5 9.0 8.0 fixed 18.60 12.00 30.00 18.60 1.20 17.40 26",
        "hillroad 17.5 6.0 8.0 8.5 fixed 19.0 8.5 19.0 22.6 2.4 5.2 30",
        "weststreet 5.5 16.5 9.0 7.0 rail 5.5 6.5 26.0 14.0 6.5 26.0 18.6 1.2 17.4 32",
        "grainyard 3.0 9.0 10.0 8.0 fixed 8.0 7.0 21.0 6.8 1.2 12.6 30", ""]
EXITS = [(21, 3, 3, 1)]
def halm_skip(mx, mz, a, b):
    if abs(height_at(mx + 0.45, mz + 0.45) - height_at(mx - 0.45, mz - 0.45)) > 0.01: return True
    for (bx, bz, bw, bd, bh, bid, tgt) in [b[:7] for b in blds]:    # a building already is a wall
        cx, cz = bx + bw / 2, bz + bd / 2
        r = math.radians(face_rot(cx, cz, tgt[0], tgt[1]))
        c, sn = math.cos(-r), math.sin(-r)
        lx = (mx - cx) * c - (mz - cz) * sn
        lz = (mx - cx) * sn + (mz - cz) * c
        if abs(lx) <= bw / 2 + 0.8 and abs(lz) <= bd / 2 + 0.8: return True
    for (ex, ez, ew, ed) in EXITS:                                  # a way out is not a wall
        if mx >= ex - 0.7 and mx <= ex + ew + 0.7 and mz >= ez - 0.7 and mz <= ez + ed + 0.7: return True
    return False

out += ["## sweeps",
        "# id profile [spline] [caps] [along S] [scale s0 s1]  x z [h], x z [h], ...",
        "# a profile swept along a path. Blockers below are generated from the open navmesh edges:",
        "# the walkable edge is each wall's inner face."]
out += wall_lines(m, "fence", halm_skip)
out += ["gatepost box:0.5:1.6:wall_stone:wall_stone_top caps 20.75 3.10, 20.75 4.20",
        "gatepost box:0.5:1.6:wall_stone:wall_stone_top caps 24.15 2.95, 24.15 4.05"]
# the road crosses the stream on a bridge: deck plus two parapets, arched with explicit heights
out += ["fordbridge bridge:3.20:0.22:plank:wall_stone caps "
        "31.90 27.90 0.10, 33.40 28.25 0.34, 35.00 28.55 0.34, 36.40 28.80 0.10", ""]
out += ["## lathes",
        "# id profile x z [h] [segments]   the profile revolved about a vertical axis (x = radius)",
        "wellring ring:1.05:0.42:0.22:wall_stone 18.60 17.40 0.02 20", ""]
out += ["## buildings",
        "# x z w d h id [rot deg]   footprint in map units, height in world units, +Z face = front"]
out += bld_lines + [""]
out += ["## props",
        "18.6 17.4 well",
        "22.4 5.6 sign",
        "16.9 18.9 milestone", "20.6 15.6 cart", "10.2 16.6 barrel", "7.2 13.2 cart",
        "26.6 18.4 barrel", "31.6 11.6 fence", "4.0 28.2 fence",
        "1.4 3.2 tree_a", "3.0 6.6 tree_b", "1.0 12.0 tree_a", "2.2 19.4 tree_b",
        "0.8 23.0 tree_a", "2.6 28.4 tree_b", "6.6 28.8 tree_a", "12.4 28.2 tree_b",
        "14.6 26.8 tree_a", "23.4 28.6 tree_b", "27.0 27.8 tree_a", "33.2 28.4 tree_b",
        "37.6 24.0 tree_a", "38.4 16.6 tree_b", "37.2 8.4 tree_a", "38.6 3.0 tree_b",
        "33.6 2.2 tree_a", "31.0 6.2 tree_b", "13.2 2.0 tree_a", "9.6 5.0 tree_b",
        "6.4 8.0 tree_a", "27.0 10.8 tree_b", "25.4 12.4 tree_a", ""]
out += ["## npcs",
        "# x z walker facing Name [scene-id | say:line]",
        "17 19 villager_a E Marta say:halm.marta",
        "12 19 villager_b N Ostler say:halm.ostler",
        "22 19 clerk W Hunter 003b_after_the_warning",
        "9 16 villager_a S Grainwife say:halm.grainwife",
        "22 6 villager_b S Gate_Watch say:halm.gate_watch", ""]
out += ["## exits", "# x z w d map spawn-x spawn-z facing", "21 3 3 1 hart_yard 11 14 N", ""]
out += ["## triggers",
        "17 16 3 3 message halm.well",
        "20 15 4 2 message halm.guild_hall_door",
        "4 26 4 4 zone halm_outskirts",
        "30 25 4 4 zone halm_outskirts", ""]
# worn ground: the well, and a patch at every building door (front face + 1.2 cells out)
discs = [(WELL[0], WELL[1], 2.8, 0.65)]
for (bx, bz, bw, bd, bh, bid, tgt) in [b[:7] for b in blds]:
    cx, cz = bx + bw / 2, bz + bd / 2
    r = math.radians(face_rot(cx, cz, tgt[0], tgt[1]))
    nx, nz = -math.sin(r), math.cos(r)
    discs.append((cx + nx * (bd / 2 + 1.1), cz + nz * (bd / 2 + 1.1), 1.7, 0.9))
sp = os.path.join(OUT, "halm_splat.png")
if KEEP_SPLAT and os.path.exists(sp): print("  splat kept (hand-painted)")
else: splat(sp, m, W, H, 8, ("wall",), discs, shore=STREAM)
print("halm: %d nav polys, %d buildings" % (len(m.polys), len(blds)))
open(os.path.join(OUT, "halm.map"), "w").write("\n".join(out) + "\n")

# ════════════════════════════════════ HART'S YARD ════════════════════════════════════
W2, H2 = 24, 18
y = Map(W2, H2)
Y1 = [(5.0, 6.2), (11.5, 4.6), (12.5, 9.5), (6.0, 10.8)]
Y2 = [(11.5, 4.6), (17.6, 6.4), (17.0, 10.2), (12.5, 9.5)]
Y3 = [(6.0, 10.8), (12.5, 9.5), (13.5, 14.0), (13.0, 14.08), (9.4, 14.43), (7.4, 14.6)]
Y4 = [(12.5, 9.5), (17.0, 10.2), (17.9, 13.3), (13.5, 14.0)]
Y5 = [(13.0, 14.08), (13.3, 16.8), (9.1, 17.0), (9.4, 14.43)]
for p, nm in ((Y1, "Y1"), (Y2, "Y2"), (Y3, "Y3"), (Y4, "Y4"), (Y5, "Y5")):
    y.add([(v[0], v[1], 0.0) for v in p], "fence", nm)

def ground2(x, z):
    if y.cover(x, z) is not None: return 'd'
    if math.hypot(x - 11.5, z - 9.5) < 8.6: return 's'
    return '.'

yo = []
yo += ["## meta", "name: hart_yard", "size: %d %d" % (W2, H2), "spawn: 11 13 N",
       "ground: grass", "wall: cliff", "edges: fence",
       "landmark: the six posts set in a crooked arc round the ladder house",
       "splat: hart_yard_splat.png 8",
       "splat_layers: grass:1 dirt:1:0.15 stone:8:0.6 plank:6:0.4",
       "tile_scale: 1 1 2 1",
       "splat_blend: dither", "",
       "## tiles", "d dirt", "s stone", "w water", "p plank", ""]
yo += ["## height"] + ["".join("0" for _ in range(W2)) for _ in range(H2)] + [""]
yo += ["## ground"] + ["".join(ground2(x + 0.5, z + 0.5) for x in range(W2)) for z in range(H2)] + [""]
yo += ["## nav"] + y.nav_lines() + [""]
yo += ["## cameras",
       "# one low fixed shot: you walk into the yard, not across it. The ladder house holds the frame.",
       "yard 0 0 24 18 fixed 11.5 8.5 28.0 11.5 0.9 8.5 32", ""]
yo += ["## sweeps",
       "# generated from the open navmesh edges; the walkable edge is each wall's inner face"]
yo += wall_lines(y, "fence", lambda mx, mz, a, b: (10.0 <= mx <= 13.3 and 14.5 <= mz <= 17.2))
yo += [""]
yo += ["## buildings", "2.4 5.4 3.0 2.6 2.6 ladder_house rot %.1f" % face_rot(3.9, 6.7, 8.2, 8.4), ""]
yo += ["## props",
       "12.6 3.2 practice_post", "14.8 2.8 practice_post", "16.6 3.6 practice_post",
       "18.8 5.6 practice_post", "19.4 8.4 practice_post", "18.6 11.4 practice_post",
       "4.6 12.0 barrel", "5.6 12.8 barrel", "3.4 9.6 cart", "8.4 16.0 sign",
       "2.0 2.6 tree_a", "7.2 1.6 tree_b", "13.0 0.8 tree_a", "20.4 1.8 tree_b",
       "22.4 6.4 tree_a", "21.6 12.2 tree_b", "19.2 15.4 tree_a", "15.0 16.6 tree_b",
       "6.0 16.8 tree_a", "2.4 15.2 tree_b", "1.2 11.0 tree_a", ""]
yo += ["## npcs", "10 9 hart N Hart say:hart_yard.hart", ""]
yo += ["## exits", "10 15 3 1 halm 22 5 S", ""]
yo += ["## triggers",
       "16 3 6 3 message hart_yard.practice_posts", ""]
sp2 = os.path.join(OUT, "hart_yard_splat.png")
if KEEP_SPLAT and os.path.exists(sp2): print("  splat kept (hand-painted)")
else: splat(sp2, y, W2, H2, 8, ("wall",), [(4.0, 6.8, 2.0, 0.8)], stone_ring=2.2)
print("hart_yard: %d nav polys" % len(y.polys))
open(os.path.join(OUT, "hart_yard.map"), "w").write("\n".join(yo) + "\n")
