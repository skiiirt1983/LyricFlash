"""Tiny software renderer for the LyricFlash assembly guide (numpy + matplotlib, painter's algorithm)."""
import math
import re
import zipfile

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection

TILT = math.radians(20.0)
ZB = 47.5                       # board origin height (v9 report: board centre 47.5 mm above the desk)
import os
PROJ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "out", "phase2", "lyricflash_case_project.3mf")


# ------------------------------------------------------------------ meshes
class Mesh:
    def __init__(self, V, F, color, name=""):
        self.V = np.asarray(V, float)
        self.F = np.asarray(F, int)
        self.color = np.array(matplotlib.colors.to_rgb(color))
        self.name = name

    def copy(self, color=None):
        return Mesh(self.V.copy(), self.F.copy(), color if color is not None else self.color, self.name)

    def moved(self, R=None, t=(0, 0, 0)):
        V = self.V if R is None else self.V @ np.asarray(R).T
        m = self.copy()
        m.V = V + np.asarray(t, float)
        return m

    def bbox(self):
        return self.V.min(0), self.V.max(0)


def load_3mf(path=PROJ):
    z = zipfile.ZipFile(path)
    xml = z.read("3D/3dmodel.model").decode()
    cfg = z.read("Metadata/model_settings.config").decode()
    meshes = {}
    for m in re.finditer(r'<object id="(\d+)" name="([^"]+)"[^>]*><mesh><vertices>(.*?)</vertices><triangles>(.*?)</triangles>', xml, re.S):
        oid, name, vs, ts = m.groups()
        V = np.array(re.findall(r'x="([-\d.e]+)" y="([-\d.e]+)" z="([-\d.e]+)"', vs), float)
        F = np.array(re.findall(r'v1="(\d+)" v2="(\d+)" v3="(\d+)"', ts), int)
        meshes[oid] = (name, V, F)
    out = {}
    for m in re.finditer(r'<assemble_item object_id="(\d+)"[^>]*transform="([^"]+)"', cfg):
        oid, tr = m.groups()
        a = np.array(tr.split(), float)
        M = a[:9].reshape(3, 3)          # 3MF: row vector p' = p M + t
        t = a[9:]
        name, V, F = meshes[oid]
        out[name] = (V @ M + t, F)
    # world frame: footprint centred on X/Y (top shell bbox = footprint)
    V = out["top_shell"][0]
    c = (V.min(0) + V.max(0)) / 2
    off = np.array([c[0], c[1], 0.0])
    return {k: (V - off, F) for k, (V, F) in out.items()}


def box_mesh(lo, hi, color, name=""):
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    V = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    F = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4), (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    return Mesh(V, F, color, name)


def rbox_mesh(lo, hi, r, color, name="", n=6):
    """Box with rounded vertical edges."""
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    r = min(r, (x1 - x0) / 2 - 1e-3, (y1 - y0) / 2 - 1e-3)
    pts = []
    for cx, cy, a0 in ((x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180), (x1 - r, y0 + r, 270)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return prism_mesh(pts, z0, z1, color, name)


def prism_mesh(pts, z0, z1, color, name=""):
    pts = list(pts)
    k = len(pts)
    V = [(x, y, z0) for x, y in pts] + [(x, y, z1) for x, y in pts]
    cx = sum(p[0] for p in pts) / k
    cy = sum(p[1] for p in pts) / k
    V += [(cx, cy, z0), (cx, cy, z1)]
    b, t = 2 * k, 2 * k + 1
    F = []
    for i in range(k):
        j = (i + 1) % k
        F += [(i, j, k + j), (i, k + j, k + i), (b, j, i), (t, k + i, k + j)]
    return Mesh(V, F, color, name)


def cyl_mesh(r, z0, z1, color, name="", n=72, c=(0, 0)):
    pts = [(c[0] + r * math.cos(2 * math.pi * i / n), c[1] + r * math.sin(2 * math.pi * i / n)) for i in range(n)]
    return prism_mesh(pts, z0, z1, color, name)


def merge(meshes, color=None, name=""):
    V, F, o = [], [], 0
    for m in meshes:
        V.append(m.V)
        F.append(m.F + o)
        o += len(m.V)
    out = Mesh(np.vstack(V), np.vstack(F), color or meshes[0].color, name)
    return out


# ------------------------------------------------------------------ frames
def Rx(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


def Ry(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def Rz(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def bf_to_wf(m):
    """Board frame -> world frame."""
    return m.moved(Rx(TILT), (0, 0, ZB))


def wf_to_bf(m):
    R = Rx(TILT)
    mm = m.copy()
    mm.V = (m.V - np.array([0, 0, ZB])) @ R      # inverse rotation: v R  == R^T v
    return mm


def p_bf_to_wf(p):
    return Rx(TILT) @ np.asarray(p, float) + np.array([0, 0, ZB])


# ------------------------------------------------------------------ components (simplified)
GOLD = "#d4a64a"
BLACK = "#2a2b30"
HIL = "#f08a24"          # highlight (part being installed)
GHOST = "#c9ccd2"


def board_bf(highlight=None):
    """Simplified board in the board frame. Returns list of meshes."""
    parts = []
    parts.append(cyl_mesh(24.48, 5.8, 6.9, "#15171c", "glass"))
    parts.append(cyl_mesh(23.0, 1.2, 5.8, "#30343c", "frame"))
    parts.append(cyl_mesh(23.014, 0.0, 1.2, "#1f6b3a", "pcb"))
    usb_c = highlight == "usb" and HIL or "#b9bcc2"
    parts.append(box_mesh((-23.794, -4.47, -3.25), (-16.086, 4.47, -0.09), usb_c, "usb"))
    j1 = HIL if highlight in ("j1", "plugs") else "#ece6d6"
    h3 = HIL if highlight in ("h3", "plugs") else "#ece6d6"
    parts.append(box_mesh((-10.631, 15.919, -4.8), (-2.981, 20.119, 0.0), j1, "J1"))
    parts.append(box_mesh((2.67, 15.918, -4.8), (10.32, 20.118, 0.0), h3, "H3"))
    parts.append(box_mesh((-6, -6, -1.0), (6, 6, 0.0), "#3a3d44", "chip"))
    parts.append(box_mesh((-9.0, -21.0, -5.8), (11.0, -18.5, 0.0), "#141414", "header"))
    for (x, y), a in (((-17.54, 10.959), 22.6), ((-17.35, -11.738), -17.6)):
        m = box_mesh((-1.3, -1.7, -1.0), (1.3, 1.7, 0.0), "#c0392b", "mic").moved(Rz(math.radians(a)), (x, y, 0))
        parts.append(m)
    return parts


# v9: cell stands on its thin edge along the right wall. Pocket x 10.96..22.26, y -29..29, z 1.6..38.1
BAT_X0, BAT_X1 = 10.96, 22.26


def battery_wf(color="#3c6fb4"):
    cx = (BAT_X0 + BAT_X1) / 2
    return rbox_mesh((cx - 5.15, -28.0, 1.6), (cx + 5.15, 28.0, 36.1), 1.2, color, "battery")


def battery_leads_wf():
    cx = (BAT_X0 + BAT_X1) / 2
    a = box_mesh((cx - 2.2, 28.0, 31.5), (cx - 1.0, 31.0, 32.7), "#d23b2f")
    b = box_mesh((cx + 1.0, 28.0, 31.5), (cx + 2.2, 31.0, 32.7), "#222222")
    return [a, b]


def switch_wf(color="#2f9e6e"):
    # pocket left of the cell at the back: x 6.26..10.96, centre Y 22.7, knob down through the floor
    cx = BAT_X0 - 4.7 / 2
    body = box_mesh((cx - 1.75, 22.7 - 4.25, 3.4), (cx + 1.75, 22.7 + 4.25, 6.9), color, "switch")
    knob = box_mesh((cx - 0.75, 22.7 - 0.75, 0.4), (cx + 0.75, 22.7 + 0.75, 3.4), "#e8e8e8", "knob")
    return [body, knob]


# ------------------------------------------------------------------ renderer
def view_matrix(azim, elev):
    """Camera looking at the origin from (azim, elev) in degrees. Returns R so that cam = R @ world;
    cam x = right, cam y = up, cam z = toward the viewer."""
    a, e = math.radians(azim), math.radians(elev)
    d = np.array([math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)])   # toward the viewer
    up = np.array([0, 0, 1.0])
    if abs(np.dot(d, up)) > 0.999:
        up = np.array([0, 1.0, 0])
    right = np.cross(up, d)
    right /= np.linalg.norm(right)
    u = np.cross(d, right)
    return np.vstack([right, u, d])


LIGHT = np.array([0.35, 0.45, 0.82])
LIGHT /= np.linalg.norm(LIGHT)


def render(ax, meshes, azim=-60, elev=25, R=None, alpha_map=None, edge=False, light=None):
    """Draw meshes into a 2D matplotlib axis. Returns the view matrix (for overlays)."""
    if R is None:
        R = view_matrix(azim, elev)
    Lc = R @ (LIGHT if light is None else light)          # light fixed relative to the camera
    Lc = np.array([-0.35, 0.55, 0.76])
    Lc /= np.linalg.norm(Lc)
    polys, cols, depth = [], [], []
    for m in meshes:
        P = m.V @ R.T
        T = P[m.F]
        n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
        nl = np.linalg.norm(n, axis=1)
        ok = nl > 1e-12
        T, n, nl = T[ok], n[ok], nl[ok]
        n = n / nl[:, None]
        vis = n[:, 2] > -1e-6
        T, n = T[vis], n[vis]
        lam = np.clip(n @ Lc, 0, 1)
        shade = 0.42 + 0.58 * lam + 0.10 * n[:, 2]
        c = np.clip(m.color[None, :] * shade[:, None] + 0.06 * (lam[:, None] ** 8), 0, 1)
        a = 1.0 if alpha_map is None else alpha_map.get(m.name, 1.0)
        c = np.hstack([c, np.full((len(c), 1), a)])
        polys.append(T[:, :, :2])
        cols.append(c)
        depth.append(T[:, :, 2].mean(1))
    polys = np.vstack(polys)
    cols = np.vstack(cols)
    depth = np.concatenate(depth)
    o = np.argsort(depth)
    pc = PolyCollection(polys[o], facecolors=cols[o], edgecolors=cols[o], linewidths=0.25, antialiased=True)
    ax.add_collection(pc)
    allp = polys.reshape(-1, 2)
    lo, hi = allp.min(0), allp.max(0)
    return R, lo, hi


def proj(R, p):
    p = np.atleast_2d(np.asarray(p, float))
    q = p @ R.T
    return q[:, :2]


def setup_ax(ax, lo, hi, pad=4):
    ax.set_xlim(lo[0] - pad, hi[0] + pad)
    ax.set_ylim(lo[1] - pad, hi[1] + pad)
    ax.set_aspect("equal")
    ax.axis("off")


def figure(w=7.0, h=5.0):
    fig = plt.figure(figsize=(w, h), dpi=200)
    ax = fig.add_axes([0, 0, 1, 1])
    return fig, ax


# ------------------------------------------------------------------ z-buffer rasterizer
class Cam:
    def __init__(self, R, lo, hi, W, pad=0.06):
        self.R = R
        span = hi - lo
        self.s = W * (1 - 2 * pad) / span[0]
        self.W = W
        self.H = int(round(span[1] * self.s + 2 * pad * W))
        self.ox = lo[0] - pad * W / self.s
        self.oy = hi[1] + pad * W / self.s

    def depth(self, p):
        return (np.atleast_2d(np.asarray(p, float)) @ self.R.T)[:, 2]

    def visible(self, p, tol=0.4):
        q = self.px(p)
        d = self.depth(p)
        out = np.ones(len(q), bool)
        for i, ((x, y), z) in enumerate(zip(q, d)):
            xi, yi = int(x), int(y)
            if 0 <= xi < self.W and 0 <= yi < self.H:
                out[i] = z >= self.zbuf[yi, xi] - tol
        return out

    def px(self, p):
        q = np.atleast_2d(np.asarray(p, float)) @ self.R.T
        return np.stack([(q[:, 0] - self.ox) * self.s, (self.oy - q[:, 1]) * self.s], 1)


def raster(meshes, azim=-60, elev=25, R=None, W=1600, ss=2, cam=None, bounds_meshes=None, pad=0.06,
           outline=True, bg=(1, 1, 1), extra_bounds=None):
    """Returns (rgb image float HxWx3, cam). cam.px(world points) -> pixel coords in the output image."""
    if R is None:
        R = view_matrix(azim, elev)
    if cam is None:
        bm = bounds_meshes or meshes
        allp = np.vstack([m.V for m in bm]) @ R.T
        if extra_bounds is not None:
            allp = np.vstack([allp, np.asarray(extra_bounds) @ R.T])
        cam = Cam(R, allp[:, :2].min(0), allp[:, :2].max(0), W, pad)
    S = ss
    Wp, Hp = cam.W * S, cam.H * S
    zbuf = np.full((Hp, Wp), -np.inf)
    img = np.ones((Hp, Wp, 3)) * np.array(bg)
    ids = np.full((Hp, Wp), -1, int)
    Lc = np.array([-0.35, 0.55, 0.76])
    Lc /= np.linalg.norm(Lc)
    for mi, m in enumerate(meshes):
        P = m.V @ R.T
        X = (P[:, 0] - cam.ox) * cam.s * S
        Y = (cam.oy - P[:, 1]) * cam.s * S
        Z = P[:, 2]
        T = P[m.F]
        n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
        nl = np.linalg.norm(n, axis=1)
        good = nl > 1e-12
        n[good] /= nl[good, None]
        lam = np.clip(n @ Lc, 0, 1)
        shade = 0.40 + 0.55 * lam + 0.12 * np.clip(n[:, 2], 0, 1)
        spec = 0.10 * lam ** 12
        cols = np.clip(m.color[None, :] * shade[:, None] + spec[:, None], 0, 1)
        for fi in np.nonzero(good)[0]:
            if n[fi, 2] < -1e-4 and getattr(m, "cull", True):
                continue
            a, b, c = m.F[fi]
            xs = np.array([X[a], X[b], X[c]])
            ys = np.array([Y[a], Y[b], Y[c]])
            x0 = max(int(np.floor(xs.min())), 0)
            x1 = min(int(np.ceil(xs.max())), Wp - 1)
            y0 = max(int(np.floor(ys.min())), 0)
            y1 = min(int(np.ceil(ys.max())), Hp - 1)
            if x1 < x0 or y1 < y0:
                continue
            den = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
            if abs(den) < 1e-12:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / den
            w1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / den
            w2 = 1 - w0 - w1
            inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
            if not inside.any():
                continue
            z = w0 * Z[a] + w1 * Z[b] + w2 * Z[c]
            sub = zbuf[y0:y1 + 1, x0:x1 + 1]
            upd = inside & (z > sub)
            if not upd.any():
                continue
            sub[upd] = z[upd]
            img[y0:y1 + 1, x0:x1 + 1][upd] = cols[fi]
            ids[y0:y1 + 1, x0:x1 + 1][upd] = mi
    if outline:
        e = np.zeros((Hp, Wp), bool)
        e[:, 1:] |= ids[:, 1:] != ids[:, :-1]
        e[1:, :] |= ids[1:, :] != ids[:-1, :]
        zf = np.where(np.isfinite(zbuf), zbuf, -1e4)
        dz = np.zeros((Hp, Wp))
        dz[:, 1:] = np.maximum(dz[:, 1:], np.abs(zf[:, 1:] - zf[:, :-1]))
        dz[1:, :] = np.maximum(dz[1:, :], np.abs(zf[1:, :] - zf[:-1, :]))
        e |= dz > 1.2
        img[e] = img[e] * 0.25 + np.array([0.08, 0.08, 0.1]) * 0.75
    out = img.reshape(cam.H, S, cam.W, S, 3).mean((1, 3))
    cam.zbuf = zbuf.reshape(cam.H, S, cam.W, S).max((1, 3))
    return out, cam
