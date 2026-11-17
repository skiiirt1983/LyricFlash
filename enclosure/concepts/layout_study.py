"""Phase 2 layout study: does everything fit, and how big/tall/stable is the case?

Uses the STEP board (back-side parts as boxes), the plug + wire zone (Paul: 7 mm), the battery
pocket (58.2 x 36.6 x 12 incl. swelling), the speaker (30.8 x 20.8 x 7.2 with clearance), the top
shell (lip to seam) and 2 mm walls / 1.6 mm floor. For each layout it finds the lowest board
position that keeps >= 0.5 mm air to the battery, then reports footprint, front/back height and
tipping angles (CG from part masses).
"""
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))
import numpy as np  # noqa: E402

import params as P  # noqa: E402
from lfcase.board import load_board  # noqa: E402

WALL, FLOOR, AIR = 2.0, 1.6, 0.5
BAT = (58.2, 36.6, 12.0)
SPK = (P.SPK_L + 0.8, P.SPK_W + 0.8, P.SPK_T + 0.4)
M_BAT, M_BOARD, M_SPK = 40.0, 22.0, 8.0           # g (board estimate incl. glass)
RHO_PETG, RHO_PLA = 1.27e-3, 1.24e-3              # g/mm3


def board_boxes(g):
    """Board-frame boxes: every back-side group + the plug/wire zone + the glass/top-shell slab."""
    boxes = []
    for k, s in g.items():
        b = s.BoundingBox()
        boxes.append(((b.xmin, b.ymin, b.zmin), (b.xmax, b.ymax, b.zmax)))
    for bb in (P.J1_BBOX, P.H3_BBOX):
        (x0, y0, _), (x1, y1, _) = bb
        boxes.append(((x0 - 1, y0 - 1, -P.MIN_DEPTH_BELOW_PCB), (x1 + 1, y1 + 1, 0)))
    r = P.GLASS_D / 2 + P.BOARD_SEAT_DIAM_CLEAR / 2 + 1.6
    boxes.append(((-r, -r, P.SEAM_Z), (r, r, P.GLASS_TOP_Z + P.LIP_T)))      # top shell around the glass
    return boxes


def corners(box):
    (x0, y0, z0), (x1, y1, z1) = box
    return np.array([[x, y, z] for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)])


def to_world(pts, tilt, pos):
    t = math.radians(tilt)
    R = np.array([[1, 0, 0], [0, math.cos(t), -math.sin(t)], [0, math.sin(t), math.cos(t)]])
    return pts @ R.T + np.array(pos)


def study(name, tilt, bat_center_xy, spk="right", board_x=None):
    g = load_board()
    boxes = board_boxes(g)
    pts = np.vstack([corners(b) for b in boxes])
    bx = board_x if board_x is not None else -4.6          # USB side close to the left wall
    # battery box (world, flat on the floor)
    bcx, bcy = bat_center_xy
    bat_lo = np.array([bcx - BAT[0] / 2, bcy - BAT[1] / 2, FLOOR])
    bat_hi = np.array([bcx + BAT[0] / 2, bcy + BAT[1] / 2, FLOOR + BAT[2]])
    # lowest board height: every board point inside the battery XY must be above battery top + AIR
    by = 0.0
    lo, hi = 0.0, 80.0
    for _ in range(40):
        zb = (lo + hi) / 2
        w = to_world(pts, tilt, (bx, by, zb))
        # conservative: test each box's world AABB against the battery AABB
        ok = True
        for b in boxes:
            c = to_world(corners(b), tilt, (bx, by, zb))
            mn, mx = c.min(0), c.max(0)
            if (mn[0] < bat_hi[0] + AIR and mx[0] > bat_lo[0] - AIR and mn[1] < bat_hi[1] + AIR and mx[1] > bat_lo[1] - AIR
                    and mn[2] < bat_hi[2] + AIR):
                ok = False
                break
        if ok:
            hi = zb
        else:
            lo = zb
    zb = hi
    w = to_world(pts, tilt, (bx, by, zb))
    if w[:, 2].min() < FLOOR:                                  # board must also clear the floor
        zb += FLOOR - w[:, 2].min() + AIR
        w = to_world(pts, tilt, (bx, by, zb))
    # speaker: standing on its long edge against the right wall
    xs = [w[:, 0].min(), w[:, 0].max(), bat_lo[0], bat_hi[0]]
    ys = [w[:, 1].min(), w[:, 1].max(), bat_lo[1], bat_hi[1]]
    if spk == "right":
        spk_lo = np.array([max(w[:, 0].max(), bat_hi[0]) + 0.0, -SPK[0] / 2, FLOOR])
        # can the speaker hide in the gap beside the board, above the battery? fall back to beside
        spk_lo = np.array([w[:, 0].max() + AIR, -SPK[0] / 2, FLOOR + (BAT[2] + AIR if w[:, 0].max() + AIR < bat_hi[0] else 0)])
        spk_hi = spk_lo + np.array([SPK[2], SPK[0], SPK[1]])
        xs += [spk_lo[0], spk_hi[0]]
        ys += [spk_lo[1], spk_hi[1]]
    x0, x1 = min(xs) - WALL, max(xs) + WALL
    y0, y1 = min(ys) - WALL, max(ys) + WALL
    # top surface = the lip plane; heights at front (y0) and back (y1)
    t = math.radians(tilt)
    lip_c = to_world(np.array([[0, 0, P.GLASS_TOP_Z + P.LIP_T]]), tilt, (bx, by, zb))[0]
    top = lambda y: lip_c[2] + (y - lip_c[1]) * math.tan(t)
    h_front, h_back = top(y0), top(y1)
    # CG
    shell_vol = 2 * ((x1 - x0) * (y1 - y0)) * 1.8 + 2 * (x1 - x0 + y1 - y0) * WALL * (h_front + h_back) / 2
    m_shell = shell_vol * RHO_PETG
    parts = [(M_BAT, (bat_lo + bat_hi) / 2), (M_BOARD, to_world(np.array([[0, 0, 2.0]]), tilt, (bx, by, zb))[0]),
             (m_shell, np.array([(x0 + x1) / 2, (y0 + y1) / 2, (h_front + h_back) / 4]))]
    if spk == "right":
        parts.append((M_SPK, (spk_lo + spk_hi) / 2))
    M = sum(m for m, _ in parts)
    cg = sum(m * c for m, c in parts) / M
    tip = {"front": math.degrees(math.atan2(cg[1] - y0, cg[2])), "back": math.degrees(math.atan2(y1 - cg[1], cg[2])),
           "left": math.degrees(math.atan2(cg[0] - x0, cg[2])), "right": math.degrees(math.atan2(x1 - cg[0], cg[2]))}
    return dict(name=name, tilt=tilt, footprint=(round(x1 - x0, 1), round(y1 - y0, 1)), h_front=round(h_front, 1),
                h_back=round(h_back, 1), mass=round(M), cg_h=round(cg[2], 1), tip={k: round(v) for k, v in tip.items()},
                board_z=round(zb, 1))


if __name__ == "__main__":
    rows = [
        study("A  PLAN puck, 17.5 deg, battery flat under the board", 17.5, (0.0, 0.0)),
        study("B  PLAN puck, 12 deg", 12.0, (0.0, 0.0)),
        study("C  PLAN puck, 17.5 deg, battery shifted to the front", 17.5, (0.0, -6.0)),
        study("D  Flat 'coaster' (PLAN's comparison), battery under the board", 0.0, (0.0, 0.0)),
    ]
    for r in rows:
        print(r)


def render(rows_cfg, out_png):
    import cadquery as cq
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from lfcase.render import Scene
    g = load_board()
    fig, axs = plt.subplots(1, len(rows_cfg), figsize=(6.2 * len(rows_cfg), 6.6), dpi=100)
    for ax, (title, tilt, bat) in zip(axs, rows_cfg):
        r = study(title, tilt, bat)
        zb = r["board_z"]
        loc = cq.Location(cq.Vector(-4.6, 0, zb)) * cq.Location(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), tilt)
        (fx, fy) = r["footprint"]
        # hull: rounded rectangle, top = lip plane
        t = math.radians(tilt)
        lip = to_world(np.array([[0, 0, P.GLASS_TOP_Z + P.LIP_T]]), tilt, (-4.6, 0, zb))[0]
        cx = -4.6 + 1.3
        hull = cq.Workplane("XY").center(cx, 0).sketch().rect(fx, fy).vertices().fillet(10).finalize().extrude(80).val()
        cut = cq.Solid.makeBox(300, 300, 200, pnt=cq.Vector(-150, -150, 0)).rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), tilt)
        hull = hull.cut(cut.moved(cq.Location(cq.Vector(0, 0, lip[2] - lip[1] * math.tan(t)))))
        s = Scene(size=(1000, 900), bg=(0.97, 0.97, 0.97))
        s.add(hull, (0.80, 0.62, 0.30), opacity=0.25, tol=0.1)
        for k, v in g.items():
            s.add(v.moved(loc), (0.1, 0.42, 0.22) if k == "pcb" else (0.12, 0.12, 0.14) if k == "display" else (0.5, 0.5, 0.52), tol=0.08)
        bat_s = cq.Solid.makeBox(*BAT, pnt=cq.Vector(bat[0] - BAT[0] / 2, bat[1] - BAT[1] / 2, FLOOR))
        s.add(bat_s, (0.25, 0.45, 0.85))
        s.camera(((-1.0, -0.9, 0.45), (0, 0, 1)), zoom=1.05)
        ax.imshow(s.image()); s.close(); ax.axis("off")
        ax.set_title(f"{title}\nfront {r['h_front']} mm -> back {r['h_back']} mm, {r['footprint'][0]} x {r['footprint'][1]}", fontsize=13)
    fig.suptitle("Layout study: real board (STEP) + battery (blue) + plug/wire room. Gold = outer shape.", fontsize=16)
    fig.tight_layout(); fig.savefig(out_png); plt.close(fig)
