"""Look-and-colour preview of the tilted puck (PLAN 3.1). NOT the Phase 2 design:
the outer shape is approximate; the board, bezel lip and port positions are real.

    python concepts/look.py     # writes out/concepts/look_*.png
"""
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

import cadquery as cq  # noqa: E402
import numpy as np  # noqa: E402

import params as P  # noqa: E402
from lfcase.board import load_board  # noqa: E402
from lfcase.render import Scene  # noqa: E402

OUT = HERE / "out" / "concepts"
SIZE, CORNER = 62.0, 14.0
T = math.radians(P.TILT_DEG)
H_C = 25.0                 # top surface height at the centre
SEAM_DROP = 9.0            # top shell thickness measured down from the top surface
N = np.array([0, -math.sin(T), math.cos(T)])


def plane_cut(solid, h, keep_below=True):
    """Cut with the tilted plane z = h + y*tan(T)."""
    big = cq.Solid.makeBox(200, 200, 200, pnt=cq.Vector(-100, -100, 0 if keep_below else -200))
    big = big.rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), P.TILT_DEG).moved(cq.Location(cq.Vector(0, 0, h)))
    return solid.cut(big) if keep_below else solid.cut(big)


def to_world(p):
    """Board frame -> world: tilt about X, lip top on the top surface at the centre."""
    x, y, z = p
    c, s = math.cos(T), math.sin(T)
    wy, wz = y * c - z * s, y * s + z * c
    base = np.array([0, 0, H_C]) - N * (P.GLASS_TOP_Z + P.LIP_T)
    return np.array([x, wy, wz]) + base


def board_loc():
    base = np.array([0, 0, H_C]) - N * (P.GLASS_TOP_Z + P.LIP_T)
    return cq.Location(cq.Vector(*base)) * cq.Location(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), P.TILT_DEG)


def build():
    body = cq.Workplane("XY").sketch().rect(SIZE, SIZE).vertices().fillet(CORNER).finalize().extrude(45).val()
    top_plane = cq.Solid.makeBox(200, 200, 100, pnt=cq.Vector(-100, -100, 0)).rotate(
        cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), P.TILT_DEG).moved(cq.Location(cq.Vector(0, 0, H_C)))
    body = body.cut(top_plane)
    try:
        body = cq.Workplane().add(body).edges("not |Z").fillet(3.5).val()
    except Exception:
        pass
    # display: bezel opening (lip) + bore for the board, along the tilted normal
    loc = board_loc()
    lip_hole = cq.Solid.makeCylinder(P.GLASS_D / 2 - P.LIP_OVERLAP, 30, pnt=cq.Vector(0, 0, P.GLASS_TOP_Z - 0.01)).moved(loc)
    bore = cq.Solid.makeCylinder(P.GLASS_D / 2 + 0.15, P.GLASS_TOP_Z + 6, pnt=cq.Vector(0, 0, -6)).moved(loc)
    body = body.cut(lip_hole).cut(bore)
    # USB-C port bay on the left (flush mouth), mic holes beside it
    m = to_world((P.USB_MOUTH_X, P.USB_CENTER_Y, P.USB_CENTER_Z))
    bay = cq.Workplane("YZ", origin=(-SIZE / 2 - 3, 0, 0)).center(m[1], m[2]).rect(12.5, 6.5).extrude(m[0] + SIZE / 2 + 3 - 0.15)
    body = body.cut(bay.edges("|X").fillet(2.0).val())
    for mic in ("MIC1", "MIC2"):
        mx, my = getattr(P, f"{mic}_CENTER")
        w = to_world((mx, my, -2.0))
        body = body.cut(cq.Solid.makeCylinder(0.6, 12, pnt=cq.Vector(-SIZE / 2 - 2, w[1], w[2]), dir=cq.Vector(1, 0, 0)))
    # PWR pad on the right
    k = to_world(P.KEY2_TIP_CENTER)
    body = body.cut(cq.Solid.makeBox(4, 7, 6, pnt=cq.Vector(SIZE / 2 - 1.5, k[1] - 3.5, k[2] - 3)))
    body = body.fuse(cq.Solid.makeBox(1.2, 3, 2.4, pnt=cq.Vector(SIZE / 2 - 1.6, k[1] - 1.5, k[2] - 1.2)))
    # speaker grille on the back
    for i in range(9):
        for j in range(3):
            body = body.cut(cq.Solid.makeCylinder(0.8, 6, pnt=cq.Vector(-10 + i * 2.5, SIZE / 2 + 2, 6 + j * 2.4), dir=cq.Vector(0, -1, 0)))
    # split into top shell and base tray
    seam = cq.Solid.makeBox(200, 200, 100, pnt=cq.Vector(-100, -100, 0)).rotate(
        cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), P.TILT_DEG).moved(cq.Location(cq.Vector(0, 0, H_C - SEAM_DROP / math.cos(T))))
    top = body.intersect(seam)
    base = body.cut(seam)
    groups = load_board()
    board = {k_: v.moved(loc) for k_, v in groups.items()}
    return top, base, board


COMBOS = [
    ("Black / Black", (0.08, 0.08, 0.09), (0.08, 0.08, 0.09)),
    ("Black top / Grey base", (0.08, 0.08, 0.09), (0.55, 0.56, 0.58)),
    ("Black top / White base", (0.08, 0.08, 0.09), (0.92, 0.92, 0.90)),
    ("White top / Black base", (0.92, 0.92, 0.90), (0.08, 0.08, 0.09)),
    ("Black top / Orange base", (0.08, 0.08, 0.09), (0.95, 0.45, 0.10)),
    ("Black top / Blue base", (0.08, 0.08, 0.09), (0.10, 0.35, 0.75)),
]
BCOL = {"display": (0.02, 0.02, 0.03), "pcb": (0.1, 0.3, 0.6)}


def scene_for(top, base, board, ctop, cbase, view, size=(1100, 900), zoom=1.0):
    s = Scene(size=size, bg=(0.96, 0.96, 0.96))
    s.add(top, ctop, tol=0.05)
    s.add(base, cbase, tol=0.05)
    for k_, v in board.items():
        if k_ == "display":
            s.add(v, (0.16, 0.20, 0.32), tol=0.05)          # screen shown switched on
        elif k_ == "usb_c":
            s.add(v, (0.78, 0.78, 0.82), tol=0.05)
    s.camera(view, zoom=zoom)
    return s


def main():
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    OUT.mkdir(parents=True, exist_ok=True)
    top, base, board = build()
    views = {"front": ((-0.55, -1.0, 0.75), (0, 0, 1)), "back": ((0.7, 1.0, 0.45), (0, 0, 1)),
             "left": ((-1.0, -0.25, 0.35), (0, 0, 1))}
    # grid of colour combos (front view)
    fig, axs = plt.subplots(2, 3, figsize=(16.5, 10.6), dpi=100)
    for ax, (name, ct, cb) in zip(axs.flat, COMBOS):
        s = scene_for(top, base, board, ct, cb, views["front"], zoom=1.05)
        ax.imshow(s.image())
        s.close()
        ax.set_title(name, fontsize=18, fontweight="bold")
        ax.axis("off")
    fig.suptitle("LyricFlash case - colour preview (shape is a concept, Phase 2 refines it)", fontsize=20)
    fig.tight_layout()
    fig.savefig(OUT / "look_colours.png")
    plt.close(fig)
    # three views of one combo
    fig, axs = plt.subplots(1, 3, figsize=(18, 6.6), dpi=100)
    for ax, (vn, title) in zip(axs, [("front", "Front: screen tilted back 17.5 deg"),
                                     ("left", "Left: flush USB-C + 2 mic holes"),
                                     ("back", "Back: speaker grille, PWR button on the right")]):
        s = scene_for(top, base, board, COMBOS[1][1], COMBOS[1][2], views[vn], zoom=1.05)
        ax.imshow(s.image())
        s.close()
        ax.set_title(title, fontsize=15, fontweight="bold")
        ax.axis("off")
    fig.suptitle("Two printed parts: top shell (bezel) + base tray, so two colours max, no AMS needed", fontsize=17)
    fig.tight_layout()
    fig.savefig(OUT / "look_views.png")
    plt.close(fig)
    # gold / black (Paul: Iridium Gold PLA Metal + black PETG)
    gold, black = (0.80, 0.62, 0.30), (0.08, 0.08, 0.09)
    combos = [("Gold top / Black base", gold, black), ("Black top / Gold base", black, gold)]
    fig, axs = plt.subplots(2, 3, figsize=(18, 12.4), dpi=100)
    for r, (name, ct, cb) in enumerate(combos):
        for c, vn in enumerate(("front", "left", "back")):
            s = scene_for(top, base, board, ct, cb, views[vn], zoom=1.05)
            axs[r, c].imshow(s.image())
            s.close()
            axs[r, c].axis("off")
            axs[r, c].set_title(f"{name} - {vn}", fontsize=15, fontweight="bold")
    fig.suptitle("Iridium Gold + Black (concept shape)", fontsize=20)
    fig.tight_layout()
    fig.savefig(OUT / "look_gold_black.png")
    plt.close(fig)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
