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
CARRIER = "#5d626c"
MAG = "#c9ccd2"
MAGL = "#8a8f99"
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


def base_set(color=BASE_IN, battery=True, switch=True, magnets=True, hl=()):
    ms = [P("base", color)]
    if battery:
        ms += [lfr.battery_wf(ORANGE if "battery" in hl else BAT)] + lfr.battery_leads_wf()
    if switch:
        ms += lfr.switch_wf(ORANGE if "switch" in hl else SW)
    if magnets:
        ms += lfr.magnets_wf(ORANGE if "magnets" in hl else MAG)
    return ms


def board_unit_bf(hl=None, carrier_col=CARRIER):
    """Board + carrier plate in the board frame (they move together once screwed)."""
    return lfr.board_bf(hl) + [lfr.wf_to_bf(P("carrier", carrier_col))]


# ---------------------------------------------------------------- images
def hero():
    ms = [P("top_shell"), P("base", BASE_OUT)] + scene.board_wf()
    img, cam = lfr.raster(ms, azim=-125, elev=22, W=1300, pad=0.03)
    save_img(img, cam, OUT + "hero.png")
    img, cam = lfr.raster(ms, azim=58, elev=16, W=1300, pad=0.03)
    save_img(img, cam, OUT + "hero_back.png")


def parts():
    img, cam = lfr.raster([P("top_shell")], azim=-120, elev=35, W=900, pad=0.04)
    save_img(img, cam, OUT + "part_top.png")
    img, cam = lfr.raster([P("base", BASE_OUT)], azim=-120, elev=38, W=900, pad=0.04)
    save_img(img, cam, OUT + "part_base.png")
    img, cam = lfr.raster([lfr.wf_to_bf(P("carrier", CARRIER))], azim=-70, elev=55, W=800, pad=0.06)
    save_img(img, cam, OUT + "part_carrier.png")
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
    p = px(cam, ((lfr.BAT_X0 + lfr.BAT_X1) / 2, 0, 40))
    L.append(("Battery\n(on its edge)", p, (p[0] + 230, p[1] + 10), BAT))
    p = px(cam, (-22.5, 0, 6))
    L.append(("Switch\n(left wall)", p, (p[0] - 40, p[1] + 330), SW))
    for i, t in enumerate(lfr.tab_points_wf()):
        p = px(cam, t)
        if i == 0:
            L.append(("Screw posts (2)", p, (p[0] + 40, p[1] + 200), "#888c94"))
    m = lfr.seam_point(*lfr.MAGNET_XY[1])
    p = px(cam, m)
    L.append(("Magnets in\nall 4 corners", p, (p[0] + 40, p[1] - 150), MAGL))
    p = px(cam, (-25.5, 0, 31))
    L.append(("USB-C opening\n+ 2 mic holes", p, (p[0] - 150, p[1] - 120), BLUE))
    p = px(cam, (0, 37, 10))
    L.append(("Cable cleat", p, (p[0] + 300, p[1] + 10), "#888c94"))
    L.append(("FRONT (screen side)", None, (cam.W / 2, cam.H - 50), "#bbbbbb"))
    L.append(("BACK", None, (cam.W / 2 - 260, 70), "#bbbbbb"))
    save_img(img, cam, OUT + "map.png", labels=L)


def exploded():
    n = lfr.Rx(lfr.TILT) @ np.array([0, 0, 1.0])
    lt, lb = 56 * n, 30 * n
    unit = [lfr.bf_to_wf(m).moved(None, lb) for m in board_unit_bf()]
    ms = [P("top_shell").moved(None, lt)] + unit + base_set("#45474e")
    img, cam = lfr.raster(ms, azim=-125, elev=20, W=1100, pad=0.05)
    L = []
    p = px(cam, (-20, -20 + lt[1], 49 + lt[2]))
    L.append(("Top shell (gold)", p, (p[0] - 120, p[1] - 30), lfr.GOLD))
    p = px(cam, (-24, 0 + lb[1], 36 + lb[2]))
    L.append(("Display board on\nits carrier plate", p, (p[0] - 160, p[1] + 20), "#1f6b3a"))
    p = px(cam, (-27, -10, 12))
    L.append(("Base (black)\nbattery, switch\nand magnets inside", p, (p[0] - 150, p[1] + 20), "#777"))
    save_img(img, cam, OUT + "exploded.png", labels=L)


def step_magnets():
    ms = base_set(battery=False, switch=False, magnets=False)
    mags = lfr.magnets_wf(ORANGE, lift=9.0)
    R = lfr.view_matrix(-120, 48)
    img, cam = lfr.raster(ms + mags, R=R, W=1000, pad=0.05)
    arrows, labels = [], []
    nrm = lfr.Rx(lfr.TILT) @ np.array([0, 0, 1.0])
    for i, (x, y) in enumerate(lfr.MAGNET_XY):
        p = lfr.seam_point(x, y)
        arrows.append((px(cam, p + nrm * 17), px(cam, p + nrm * 3), INK, {}))
    p = px(cam, lfr.seam_point(*lfr.MAGNET_XY[0]))
    labels.append(("One 4 x 2 magnet\nin each corner", p, (p[0] - 40, p[1] + 200), ORANGE))
    save_img(img, cam, OUT + "step_magnets.png", arrows=arrows, labels=labels)


def step_switch():
    b = P("base", BASE_IN)
    c = b.V[b.F].mean(1)
    keep = (c[:, 0] < -12) & (np.abs(c[:, 1] - lfr.SWITCH_Y) < 14) & (c[:, 2] < 18)      # cut-away: just the left wall's bottom
    rest = [lfr.Mesh(b.V, b.F[keep], BASE_IN, "base")]
    sw_out = lfr.switch_wf(ORANGE, out=9.0)
    box = lfr.box_mesh((-27, lfr.SWITCH_Y - 12, 0), (-8, lfr.SWITCH_Y + 12, 14), "#000")
    R = lfr.view_matrix(20, 28)                 # from inside the case, looking at the left wall
    img, cam = lfr.raster(rest + sw_out, R=R, W=1000, bounds_meshes=[box], pad=0.03)
    a0 = px(cam, (-12.0, lfr.SWITCH_Y, 4.2))
    a1 = px(cam, (-19.5, lfr.SWITCH_Y, 4.2))
    k = px(cam, (-26.43, lfr.SWITCH_Y, 4.2))
    save_img(img, cam, OUT + "step_switch.png", arrows=[(a0, a1, INK, {})],
             labels=[("Knob first, through\nthe slot in the wall", k, (k[0] + 60, k[1] - 230), SW)])
    seated = base_set(battery=False, switch=True, magnets=False)
    box = lfr.box_mesh((-32, lfr.SWITCH_Y - 14, 0), (-20, lfr.SWITCH_Y + 14, 16), "#000")
    img, cam = lfr.raster(seated, R=lfr.view_matrix(-160, 12), W=900, bounds_meshes=[box], pad=0.04)
    k = px(cam, (-28.2, lfr.SWITCH_Y, 4.2))
    save_img(img, cam, OUT + "step_switch_out.png",
             labels=[("Knob stands ~1.85 mm\nout of the wall", k, (k[0] - 20, k[1] - 230), SW)])


def step_battery():
    ms = base_set(hl=("battery",))
    bat = [m for m in ms if m.name == "battery"]
    rest = [m for m in ms if m.name != "battery"]
    lifted = [m.moved(None, (0, 0, 48)) for m in bat] + [m.moved(None, (0, 0, 48)) for m in lfr.battery_leads_wf()]
    rest = [m for m in rest if not (m.V[:, 1].min() >= 27.9 and m.V[:, 2].min() > 31)]   # drop the seated leads
    R = lfr.view_matrix(-125, 38)
    img, cam = lfr.raster(rest + lifted, R=R, W=1000, pad=0.05)
    cx = (lfr.BAT_X0 + lfr.BAT_X1) / 2
    a0 = px(cam, (cx, -34, 72))
    a1 = px(cam, (cx, -34, 48))
    leads = px(cam, (cx, 29.5, 80.6))
    save_img(img, cam, OUT + "step_battery.png", arrows=[(a0, a1, INK, {})],
             labels=[("Leads at the back, on top", leads, (leads[0] - 240, leads[1] - 40), ORANGE)])


def step_carrier():
    """On the bench: board face-down on a cloth, plate laid on its back, three M2 x 3 flat heads."""
    cloth = lfr.box_mesh((-45, -45, -1.2), (45, 45, 0), "#e9e4da", "cloth")
    glass_top = 6.9
    board = [bf_flip(m, (0, 0, glass_top)) for m in lfr.board_bf()]
    plate = bf_flip(lfr.wf_to_bf(P("carrier", ORANGE)), (0, 0, glass_top + 12))
    screws = []
    for x, y in lfr.MOUNT_XY if hasattr(lfr, "MOUNT_XY") else [(-13.75, -14.698), (0.0, 20.501), (13.75, -14.701)]:
        h = lfr.cyl_mesh(1.9, 0, 1.2, "#9aa0aa", "screw", n=32, c=(x, -y))
        sh = lfr.cyl_mesh(1.0, -3, 0, "#9aa0aa", "screw", n=20, c=(x, -y))
        z = glass_top + 3.5 + 12 + 2.4 + 8
        screws += [h.moved(None, (0, 0, z)), sh.moved(None, (0, 0, z))]
    R = lfr.view_matrix(-110, 40)
    img, cam = lfr.raster([cloth] + board + [plate] + screws, R=R, W=1000, pad=0.03,
                          bounds_meshes=board + [plate] + screws)
    arrows = []
    for x, y in [(-13.75, -14.698), (0.0, 20.501), (13.75, -14.701)]:
        arrows.append((px(cam, (x, -y, glass_top + 3.5 + 32)), px(cam, (x, -y, glass_top + 3.5 + 16)), INK, {}))
    g = px(cam, (0, 0, 2))
    p = px(cam, (-13.75, 14.7, glass_top + 3.5 + 14.4))
    save_img(img, cam, OUT + "step_carrier.png", arrows=arrows,
             labels=[("Board glass-side DOWN\non a soft cloth", g, (g[0] + 300, g[1] + 120), "#1f6b3a"),
                     ("Plate: countersinks up, FRONT and\nUSB-C arrows readable", p, (p[0] - 120, p[1] - 220), ORANGE)])


def step_board_in():
    for name, deg in (("board_in_a", 22), ("board_in_b", 0)):
        unit = [lfr.bf_to_wf(rot_about_hinge_bf(m, -deg)) for m in board_unit_bf(carrier_col=ORANGE)]
        base = base_set(BASE_OUT)
        ref = [lfr.bf_to_wf(rot_about_hinge_bf(m, -22)) for m in board_unit_bf()] + base
        R = lfr.view_matrix(-90, 8)
        img, cam = lfr.raster(base + unit, R=R, W=1000, pad=0.04, bounds_meshes=ref)
        arrows, labels, curves = [], [], []
        hinge = px(cam, lfr.p_bf_to_wf((-24.2, 0, 5.5)))
        if deg:
            c = lfr.p_bf_to_wf((-24.2, 0, 5.5))
            r = 46
            angs = np.radians(np.linspace(deg + 12, 3, 30))
            pts = np.array([px(cam, (c[0] + r * math.cos(a), 0, c[2] + r * math.sin(a))) for a in angs])
            arrows.append((tuple(pts[-6]), tuple(pts[-1]), ORANGE, {}))
            curves = [(pts[:-4], "white", 11), (pts[:-4], ORANGE, 6)]
            labels.append(("1. USB-C into its\nopening first", hinge, (hinge[0] - 10, hinge[1] - 300), BLUE))
            labels.append(("2. Lower this\nside down", tuple(pts[8]), (pts[8][0] + 60, pts[8][1] - 140), ORANGE))
        else:
            labels.append(("Seated: the three tabs\nrest on their posts", None, (cam.W / 2, 80), "#888"))
        save_img(img, cam, OUT + f"step_{name}.png", arrows=arrows, labels=labels, curves=curves)


def step_screws():
    ms = base_set(BASE_OUT) + [lfr.bf_to_wf(m) for m in board_unit_bf(carrier_col=CARRIER)]
    lift = 10.0
    screws = lfr.tab_screws_wf(ORANGE, lift=lift)
    R = lfr.view_matrix(-115, 58)
    img, cam = lfr.raster(ms + screws, R=R, W=1000, pad=0.05)
    nrm = lfr.Rx(lfr.TILT) @ np.array([0, 0, 1.0])
    arrows = [(px(cam, t + nrm * (lift + 12)), px(cam, t + nrm * (lift + 3)), INK, {}) for t in lfr.tab_points_wf()]
    t0 = lfr.tab_points_wf()[0]
    p = px(cam, t0)
    save_img(img, cam, OUT + "step_screws.png", arrows=arrows,
             labels=[("3x M2.5 x 6, straight down,\nall outside the screen", p, (p[0] - 40, p[1] + 220), ORANGE)])


def step_shell():
    nrm = lfr.Rx(lfr.TILT) @ np.array([0, 0, 1.0])
    base = base_set(BASE_OUT) + [lfr.bf_to_wf(m) for m in board_unit_bf()]
    shell = P("top_shell").moved(None, nrm * 26)
    R = lfr.view_matrix(-125, 18)
    img, cam = lfr.raster(base + [shell], R=R, W=1000, pad=0.05)
    a0 = px(cam, (24, -10, 60) + nrm * 40)
    a1 = px(cam, (24, -10, 60) + nrm * 14)
    save_img(img, cam, OUT + "step_shell.png", arrows=[(a0, a1, INK, {})],
             labels=[("Straight down, square to\nthe screen; magnets pull it home", None, (cam.W / 2, 70), lfr.GOLD)])


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
    todo = sys.argv[1:] or ["hero", "parts", "layout_map", "exploded", "step_magnets", "step_switch", "step_battery",
                             "step_carrier", "step_board_in", "step_screws", "step_shell", "step_cleat"]
    for t in todo:
        globals()[t]()
        print("done", t, flush=True)
