import math
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch
from PIL import Image

import lfr
import scene

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "img") + "/"
BASE_IN = "#55585f"     # base colour for interior views (real part is black PETG; lightened so details show)
BASE_OUT = "#33353b"
CLIP = "#6b7079"
BAT = "#3c6fb4"
SPK = "#7d5aa6"
SW = "#2f9e6e"
ORANGE = lfr.HIL
INK = "#1d2330"
BLUE = "#1f6fd1"


def P(name, color=None):
    return scene.part(name, color)


def save_img(img, cam, path, notes=(), arrows=(), w_in=None, labels=(), curves=()):
    """Overlay annotations (pixel coords from cam.px) and save PNG."""
    H, W = img.shape[:2]
    dpi = 200
    fig = plt.figure(figsize=(W / dpi, H / dpi), dpi=dpi)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.imshow(img, interpolation="lanczos")
    ax.set_xlim(0, W)
    ax.set_ylim(H, 0)
    ax.axis("off")
    for (p0, p1, col, kw) in arrows:
        for lw_, c_ in ((11, "white"), (6, col)):
            ax.annotate("", xy=p1, xytext=p0, zorder=6,
                        arrowprops=dict(arrowstyle="-|>,head_length=0.9,head_width=0.55", lw=lw_, color=c_,
                                        mutation_scale=22, shrinkA=0, shrinkB=0))
    for cv in curves:
        pts, col = cv[0], cv[1]
        lw = cv[2] if len(cv) > 2 else 4.5
        ax.plot(pts[:, 0], pts[:, 1], color=col, lw=lw, solid_capstyle="round", zorder=5)
    for (txt, anchor, at, col) in labels:
        if anchor is None:
            hw = max(len(l) for l in txt.split("\n")) * 11.5 + 22
            at = (min(max(at[0], hw), W - hw), min(max(at[1], 45), H - 45))
            ax.text(at[0], at[1], txt, fontsize=11, fontweight="bold", color=INK, ha="center", va="center",
                    bbox=dict(boxstyle="round,pad=0.35", fc="white", ec=col, lw=2), zorder=10)
            continue
        hw = max(len(l) for l in txt.split("\n")) * 11.5 + 22
        at = (min(max(at[0], hw), W - hw), min(max(at[1], 45), H - 45))
        ax.annotate(txt, xy=anchor, xytext=at, fontsize=11, fontweight="bold", color=INK, ha="center", va="center",
                    bbox=dict(boxstyle="round,pad=0.35", fc="white", ec=col, lw=2),
                    arrowprops=dict(arrowstyle="-", color=col, lw=2, shrinkA=0, shrinkB=2) if anchor is not None else None,
                    zorder=10)
    fig.savefig(path, dpi=dpi)
    plt.close(fig)


def px(cam, p):
    return tuple(cam.px(p)[0])


def bf_flip(m, lift=(0, 0, 0)):
    """Board-frame mesh -> 'top shell face-down on the table' frame (glass down), plus a lift."""
    mm = m.copy()
    V = m.V * np.array([1, -1, -1])
    mm.V = V + np.asarray(lift, float)
    return mm


def bf_flip_pt(p, lift=(0, 0, 0)):
    return np.asarray(p, float) * np.array([1, -1, -1]) + np.asarray(lift, float)


def rot_about_hinge_bf(m, deg):
    """Rotate a board-frame mesh about the tilt-in hinge (BF X -24.2, Z 5.5, along Y). + = open."""
    hx, hz = -24.2, 5.5
    R = lfr.Ry(math.radians(deg))
    mm = m.copy()
    q = m.V - np.array([hx, 0, hz])
    mm.V = q @ R.T + np.array([hx, 0, hz])
    return mm


def base_set(color=BASE_IN, clips=True, battery=True, speaker=False, switch=True, hl=()):
    ms = [P("base", color)]
    if clips:
        ms += [P("clip_front", ORANGE if "clips" in hl else CLIP), P("clip_back", ORANGE if "clips" in hl else CLIP)]
    if battery:
        ms += [lfr.battery_wf(ORANGE if "battery" in hl else BAT)] + lfr.battery_leads_wf()
    if switch:
        ms += lfr.switch_wf(ORANGE if "switch" in hl else SW)
    return ms


# ---------------------------------------------------------------- images
def hero():
    ms = [P("top_shell"), P("base", BASE_OUT)] + scene.board_wf()
    img, cam = lfr.raster(ms, azim=-125, elev=22, W=1300, pad=0.03)
    save_img(img, cam, OUT + "hero.png")
    img, cam = lfr.raster(ms, azim=58, elev=16, W=1300, pad=0.03)
    save_img(img, cam, OUT + "hero_back.png")


def parts():
    # top shell, face up
    img, cam = lfr.raster([P("top_shell")], azim=-120, elev=35, W=900, pad=0.04)
    save_img(img, cam, OUT + "part_top.png")
    img, cam = lfr.raster([P("base", BASE_OUT)], azim=-120, elev=38, W=900, pad=0.04)
    save_img(img, cam, OUT + "part_base.png")
    for n in ("clip_back", "clip_front"):
        img, cam = lfr.raster([P(n, CLIP)], azim=-60, elev=30, W=500, pad=0.12)
        save_img(img, cam, OUT + f"part_{n}.png")
    img, cam = lfr.raster([lfr.battery_wf()] + lfr.battery_leads_wf(), azim=-150, elev=20, W=700, pad=0.06)
    save_img(img, cam, OUT + "part_battery.png")
    img, cam = lfr.raster(lfr.switch_wf(), azim=-60, elev=25, W=500, pad=0.12)
    save_img(img, cam, OUT + "part_switch.png")
    b = [bf for bf in lfr.board_bf()]
    img, cam = lfr.raster(b, azim=-70, elev=40, W=800, pad=0.05)
    save_img(img, cam, OUT + "part_board.png")


def layout_map():
    ms = base_set()
    img, cam = lfr.raster(ms, azim=-90, elev=89.9, W=1200, pad=0.21)
    L = []
    p = px(cam, (16.6, 0, 40))
    L.append(("Battery\n(on its edge)", p, (p[0] + 200, p[1] + 10), BAT))
    p = px(cam, (8.6, 22.7, 7))
    L.append(("Switch\n(optional)", p, (p[0] - 40, p[1] + 330), SW))
    c = scene.D["clip_back"][0]
    p = px(cam, c.mean(0) + np.array([0, 0, 20]))
    L.append(("Latch clip\n(back wall)", p, (p[0] - 120, p[1] - 150), "#888c94"))
    c = scene.D["clip_front"][0]
    p = px(cam, c.mean(0) + np.array([0, 0, 30]))
    L.append(("Latch clip\n(front wall)", p, (p[0] - 60, p[1] + 170), "#888c94"))
    p = px(cam, (-25.5, 0, 31))
    L.append(("USB-C opening\n+ 2 mic holes", p, (p[0] - 150, p[1] + 10), BLUE))
    p = px(cam, (0, 37, 10))
    L.append(("Cable cleat", p, (p[0] + 300, p[1] + 10), "#888c94"))
    L.append(("FRONT (screen side)", None, (cam.W / 2, cam.H - 50), "#bbbbbb"))
    L.append(("BACK", None, (cam.W / 2 - 260, 70), "#bbbbbb"))
    save_img(img, cam, OUT + "map.png", labels=L)


def exploded():
    n = lfr.Rx(lfr.TILT) @ np.array([0, 0, 1.0])
    lt, lb = 52 * n, 28 * n
    ms = [P("top_shell").moved(None, lt)] + [m.moved(None, lb) for m in scene.board_wf()]
    ms += base_set("#45474e")
    lift_top, lift_board = lt[2], lb[2]
    img, cam = lfr.raster(ms, azim=-125, elev=20, W=1100, pad=0.05)
    L = []
    p = px(cam, (-20, -20 + lt[1], 46 + lift_top))
    L.append(("Top shell (gold)", p, (p[0] - 120, p[1] - 30), lfr.GOLD))
    p = px(cam, (-24, 0 + lb[1], 33 + lift_board))
    L.append(("Display board", p, (p[0] - 160, p[1] + 20), "#1f6b3a"))
    p = px(cam, (-27, -10, 12))
    L.append(("Base (black)\nbattery, clips\nand switch inside", p, (p[0] - 150, p[1] + 20), "#777"))
    save_img(img, cam, OUT + "exploded.png", labels=L)


def step_switch():
    ms = base_set(clips=False, battery=False, speaker=False, hl=("switch",))
    # zoom on the back-right corner
    sw = lfr.switch_wf()
    R = lfr.view_matrix(-115, 55)
    R = lfr.view_matrix(-120, 68)
    box = lfr.box_mesh((-2, 12, 0), (24, 33, 10), "#000")
    img, cam = lfr.raster(ms, R=R, W=1000, bounds_meshes=[box], pad=0.03)
    s0 = px(cam, (19.5, 22.7, 5.2))
    s1 = px(cam, (10.5, 22.7, 5.2))
    p = px(cam, (8.6, 22.7, 3))
    save_img(img, cam, OUT + "step_switch.png", arrows=[(s0, s1, INK, {})],
             labels=[("Knob points DOWN\nthrough the floor slot", p, (p[0] + 40, p[1] + 260), SW)])
    img, cam = lfr.raster(ms, R=lfr.view_matrix(-60, -60), W=900, pad=0.05)
    k = px(cam, (8.6, 22.7, 0))
    save_img(img, cam, OUT + "step_switch_under.png", labels=[("Knob sits just\nbelow the bottom", k, (k[0] - 60, k[1] + 200), SW)])


def step_battery():
    ms = base_set(hl=("battery",))
    bat = [m for m in ms if m.name == "battery"]
    rest = [m for m in ms if m.name != "battery"]
    lifted = [m.moved(None, (0, 0, 48)) for m in bat] + [m.moved(None, (0, 0, 48)) for m in lfr.battery_leads_wf()]
    rest = [m for m in rest if m.name != "box"]
    rest = [m for m in rest if not (m.V[:, 1].min() >= 27.9 and m.V[:, 2].min() > 31)]   # drop the seated leads
    R = lfr.view_matrix(-125, 38)
    img, cam = lfr.raster(rest + lifted, R=R, W=1000, pad=0.05)
    a0 = px(cam, (16.6, -34, 72))
    a1 = px(cam, (16.6, -34, 48))
    leads = px(cam, (16.6, 29.5, 80.6))
    save_img(img, cam, OUT + "step_battery.png", arrows=[(a0, a1, INK, {})],
             labels=[("Leads at the back, on top", leads, (leads[0] - 240, leads[1] - 40), ORANGE)])


def step_clips():
    ms = base_set(battery=False, switch=False, hl=("clips",))
    R = lfr.view_matrix(-60, 50)
    img, cam = lfr.raster(ms, R=R, W=1000, pad=0.05)
    pb = px(cam, scene.D["clip_back"][0].mean(0))
    pf = px(cam, scene.D["clip_front"][0].mean(0))
    save_img(img, cam, OUT + "step_clips.png",
             labels=[("Back wall,\nleft of the middle", pb, (pb[0] + 230, pb[1] - 60), ORANGE),
                     ("Front wall,\nleft part", pf, (pf[0] - 60, pf[1] + 260), ORANGE)])
    # close-ups
    for n, az, el in (("clip_back", -90, 35), ("clip_front", 90, 35)):
        cv = scene.D[n][0]
        lo, hi = cv.min(0) - np.array([7, 7, 9]), cv.max(0) + np.array([7, 7, 4])
        box = lfr.box_mesh(lo, hi, "#000")
        img, cam = lfr.raster(ms, R=lfr.view_matrix(az, el), W=700, bounds_meshes=[box], pad=0.02)
        save_img(img, cam, OUT + f"step_{n}_zoom.png")


def step_speaker():
    ms = base_set(hl=("speaker",))
    sp = [m for m in ms if m.name in ("speaker", "cone")]
    rest = [m for m in ms if m.name not in ("speaker", "cone")]
    lifted = [m.moved(None, (0, 0, 16)) for m in sp]
    R = lfr.view_matrix(-120, 40)
    img, cam = lfr.raster(rest + lifted, R=R, W=1000, pad=0.05)
    a0 = px(cam, (20.2, 0, 42))
    a1 = px(cam, (20.2, 0, 26))
    g = px(cam, (26.4, 0, 12))
    save_img(img, cam, OUT + "step_speaker.png", arrows=[(a0, a1, INK, {})],
             labels=[("Cone faces the\nright wall (grille)", g, (g[0] + 30, g[1] + 200), SPK)])


def top_shell_flipped(lift_board=0, hl=None):
    ts = bf_flip(lfr.wf_to_bf(P("top_shell")))
    zmin = ts.V[:, 2].min()
    ts = ts.moved(None, (0, 0, -zmin))
    board = [bf_flip(m, (0, 0, -zmin + lift_board)) for m in lfr.board_bf(hl)]
    return ts, board, -zmin


def step_board():
    ts, board, dz = top_shell_flipped(lift_board=22)
    cloth = lfr.box_mesh((-45, -45, -1.2), (45, 45, 0), "#e9e4da", "cloth")
    R = lfr.view_matrix(-110, 32)
    img, cam = lfr.raster([cloth, ts] + board, R=R, W=1000, pad=0.03, bounds_meshes=[ts] + board)
    a0 = px(cam, (32, 0, 30 + dz))
    a1 = px(cam, (32, 0, 6 + dz))
    g = px(cam, (0, 0, dz - 6.9 + 22 + 1))
    usb = px(cam, bf_flip_pt((-23.8, 0, -1.6), (0, 0, dz + 22)))
    save_img(img, cam, OUT + "step_board.png", arrows=[(a0, a1, INK, {})],
             labels=[("Glass side DOWN", g, (g[0] + 300, g[1] - 150), "#1f6b3a"),
                     ("USB-C on this side", usb, (usb[0] - 40, usb[1] - 170), BLUE)])
    # seated + plugs highlighted
    ts, board, dz = top_shell_flipped(lift_board=0, hl="j1")
    R = lfr.view_matrix(-100, 62)
    img, cam = lfr.raster([cloth, ts] + board, R=R, W=1000, pad=0.03, bounds_meshes=[ts])
    j1 = px(cam, bf_flip_pt((-6.8, 18, -4.8), (0, 0, dz)))
    h3 = px(cam, bf_flip_pt((6.5, 18, -4.8), (0, 0, dz)))
    save_img(img, cam, OUT + "step_plugs.png",
             labels=[("J1 = BATTERY", j1, (j1[0] - 60, j1[1] + 190), ORANGE)])


def closing_scene(open_deg):
    ts, board, dz = top_shell_flipped()
    base_bf = [lfr.wf_to_bf(m) for m in base_set(BASE_OUT)]
    base_bf = [rot_about_hinge_bf(m, open_deg) for m in base_bf]
    base = [bf_flip(m, (0, 0, dz)) for m in base_bf]
    return [ts] + board + base, dz


def step_close():
    for name, deg in (("close_a", 24), ("close_b", 0)):
        ms, dz = closing_scene(deg)
        R = lfr.view_matrix(-90, 6)
        ref, _ = closing_scene(24)
        img, cam = lfr.raster(ms, R=R, W=1000, pad=0.04, bounds_meshes=ref)
        arrows, labels = [], []
        hinge = px(cam, bf_flip_pt((-24.2, 0, 5.5), (0, 0, dz)))
        if deg:
            # curved arrow on the right end
            c = np.array(bf_flip_pt((-24.2, 0, 5.5), (0, 0, dz)))
            r = 50
            angs = np.radians(np.linspace(deg + 14, 4, 30))
            pts = np.array([px(cam, (c[0] + r * math.cos(a), 0, c[2] + r * math.sin(a))) for a in angs])
            arrows.append((tuple(pts[-6]), tuple(pts[-1]), ORANGE, {}))
            labels.append(("1. Hook the USB-C\nopening on first", hinge, (hinge[0] - 10, hinge[1] - 300), BLUE))
            ce = [(pts[:-4], "white", 11), (pts[:-4], ORANGE, 6)]
            labels.append(("2. Swing this\nend down", tuple(pts[8]), (pts[8][0] + 60, pts[8][1] - 140), ORANGE))
        else:
            ce = []
            labels.append(("Closed: base sits flat,\nno gap, no forcing", None, (cam.W / 2, 80), "#888"))
        save_img(img, cam, OUT + f"step_{name}.png", arrows=arrows, labels=labels, curves=ce)


def step_latches():
    ms = [P("top_shell"), P("base", BASE_OUT)] + scene.board_wf()
    R = lfr.view_matrix(180, 14)
    img, cam = lfr.raster(ms, R=R, W=1100, pad=0.16)
    arrows, labels = [], []
    # back latch: x 12.3 on the back wall (y 31.4); seam height there ~ 46
    cb = scene.D["clip_back"][0].mean(0)
    cf = scene.D["clip_front"][0].mean(0)
    for (x, y, zs, nm) in ((cb[0], 33.0, cb[2] - 4, "Back latch"), (cf[0], -33.0, cf[2] - 4, "Front latch")):
        top = px(cam, (x, y, zs + 9))
        bot = px(cam, (x, y, zs - 11))
        mid = px(cam, (x, y, zs))
        arrows.append(((top[0], top[1] - 130), (top[0], top[1] + 5), ORANGE, {}))
        arrows.append(((bot[0], bot[1] + 130), (bot[0], bot[1] - 5), ORANGE, {}))
        labels.append((nm, mid, (mid[0] + (-200 if nm == "Back latch" else 200), mid[1] - 20), ORANGE))
    save_img(img, cam, OUT + "step_latches.png", arrows=arrows, labels=labels)


def smooth3(path, n=400, k=31):
    path = np.asarray(path, float)
    seg = np.r_[0, np.cumsum(np.linalg.norm(np.diff(path, axis=0), axis=1))]
    ts = np.linspace(0, seg[-1], n)
    P3 = np.stack([np.interp(ts, seg, path[:, i]) for i in range(3)], 1)
    w = np.ones(k) / k
    return np.stack([np.convolve(np.pad(P3[:, i], k // 2, mode="edge"), w, mode="valid") for i in range(3)], 1)


def step_cleat():
    b = P("base", "#3a3c42")
    c = b.V[b.F].mean(1)
    cl = (c[:, 1] > 33.2) & (np.abs(c[:, 0]) < 16) & (c[:, 2] < 20)
    cleat = lfr.Mesh(b.V, b.F[cl], "#8d939e", "cleat")
    rest = lfr.Mesh(b.V, b.F[~cl], "#3a3c42", "base")
    ms = [P("top_shell"), rest, cleat] + scene.board_wf()
    R = lfr.view_matrix(62, 32)
    box = lfr.box_mesh((-26, 28, 0), (24, 46, 22), "#000")
    img, cam = lfr.raster(ms, R=R, W=1100, pad=0.03, bounds_meshes=[box])
    # cable: in along the back wall from the USB-C side, behind horn A (in the gap), down across the
    # front of the stem, up and over horn B, back down behind it, then away from the case
    path = [(-34, 33.6, 9), (-22, 33.8, 9.5), (-12, 34.2, 10.5), (-6, 35, 10.5), (-2, 41.5, 6.5), (3, 42, 6.5),
            (8, 41.5, 10), (11, 40.5, 15.8), (12.5, 37.5, 16.6), (13, 34.2, 14), (12.5, 33.8, 10), (11, 37, 6.5),
            (8, 42, 3.5), (6, 52, 1.6), (5, 60, 1.6)]
    P3 = smooth3(path)
    vis = cam.visible(P3, tol=0.6)
    pts = cam.px(P3)
    curves = []
    # split into runs
    start = 0
    for i in range(1, len(P3) + 1):
        if i == len(P3) or vis[i] != vis[start]:
            seg = pts[max(start - 1, 0):i]
            if vis[start]:
                curves += [(seg, "white", 14), (seg, BLUE, 8)]
            else:
                curves += [(seg, "#9fc3f2", 5)]
            start = i
    a = px(cam, (-12, 38, 13.5))
    bb = px(cam, (12, 38, 15))
    save_img(img, cam, OUT + "step_cleat.png", curves=curves,
             arrows=[(tuple(pts[-40]), tuple(pts[-1]), BLUE, {})],
             labels=[("1. Behind / under\nthis horn", a, (a[0] + 80, a[1] - 230), BLUE),
                     ("2. Over the top of\nthis horn", bb, (bb[0] - 60, bb[1] - 240), BLUE),
                     ("from the USB-C side", tuple(pts[5]), (pts[5][0] - 40, pts[5][1] + 170), BLUE)])


if __name__ == "__main__":
    import os
    os.makedirs(OUT, exist_ok=True)
    todo = sys.argv[1:] or ["hero", "parts", "layout_map", "exploded", "step_switch", "step_battery", "step_clips",
                             "step_board", "step_close", "step_latches", "step_cleat"]
    for t in todo:
        globals()[t]()
        print("done", t, flush=True)
