"""Side-by-side section renders of the two glass-retention options (decision aid).

    python concepts/bezel_options.py     # writes out/concepts/bezel_*.png

A = PLAN 3.1 lip, board loads from behind, base tray clamps it forward on the
    three M2 standoff faces.
B = PrintSphere style (measured from printsphere_case.stl): glass loads from
    the front onto a ledge, small nubs click over the glass edge round, and
    screws into the standoffs pull the board back onto the ledge.
Only the region around the board is modelled; this is not the enclosure.
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))

import cadquery as cq  # noqa: E402

import params as P  # noqa: E402
from lfcase.board import load_board  # noqa: E402
from lfcase.render import Scene, save_labelled  # noqa: E402

OUT = HERE / "out" / "concepts"
R_OUT = 28.0                      # illustrative outer radius only
SEAM_Z = -1.0                     # illustrative

GR = P.GLASS_D / 2
SEAT_R = GR + P.BOARD_SEAT_DIAM_CLEAR / 2


def ring(r_in, r_out, z0, z1):
    return cq.Workplane("XY").workplane(offset=z0).circle(r_out).circle(r_in).extrude(z1 - z0)


def option_a():
    lip_overlap, lip_t = 1.0, 1.0
    top = ring(SEAT_R, R_OUT, SEAM_Z, P.GLASS_TOP_Z).union(
        ring(GR - lip_overlap, R_OUT, P.GLASS_TOP_Z, P.GLASS_TOP_Z + lip_t))
    base_floor = P.STANDOFF_Z[0] - 2.0
    base = ring(SEAT_R - 0.0, R_OUT, base_floor - 6, SEAM_Z).union(
        cq.Workplane("XY").workplane(offset=base_floor - 6).circle(R_OUT).extrude(2.0))
    cups = cq.Workplane("XY")
    for x, y in P.MOUNT_HOLES_XY:
        c = (cq.Workplane("XY").workplane(offset=base_floor - 4).center(x, y)
             .circle(P.STANDOFF_OD / 2 + 1.2).extrude(4 + 2.0 - 0.0 + (P.STANDOFF_Z[0] - base_floor))
             .faces(">Z").workplane().circle(P.STANDOFF_OD / 2 + 0.1).cutBlind(-1.5))
        cups = cups.union(c)
    base = base.union(cups)
    return top.val(), base.val()


def option_b():
    # PrintSphere numbers measured from its STL: ledge r 23.80, seat r 24.55,
    # rim 0.80 proud of the glass, nubs reach r 24.21 over the glass edge round.
    ledge_r, seat_r, proud = 23.80, 24.55, 0.80
    top = ring(seat_r, R_OUT, P.GLASS_BOTTOM_Z, P.GLASS_TOP_Z + proud).union(
        ring(ledge_r, R_OUT, SEAM_Z, P.GLASS_BOTTOM_Z))
    for ang in (45, 135, 225, 315):
        nub = (cq.Workplane("XY").workplane(offset=P.GLASS_TOP_Z - 0.3).center(24.21 + 0.6, 0)
               .rect(1.2, 2.0).extrude(0.25 + 0.3).rotate((0, 0, 0), (0, 0, 1), ang))
        top = top.union(nub)
    base_floor = P.STANDOFF_Z[0] - 2.0
    base = ring(ledge_r, R_OUT, base_floor - 6, SEAM_Z).union(
        cq.Workplane("XY").workplane(offset=base_floor - 6).circle(R_OUT).extrude(2.0))
    screws = cq.Workplane("XY")
    for x, y in P.MOUNT_HOLES_XY:
        base = base.union(cq.Workplane("XY").workplane(offset=base_floor - 4).center(x, y)
                          .circle(3.0).extrude(4 + (P.STANDOFF_Z[0] - base_floor)))
        screws = screws.union(cq.Workplane("XY").workplane(offset=base_floor - 6.6).center(x, y)
                              .circle(1.9).extrude(1.4).faces(">Z").workplane().circle(0.95).extrude(8))
    return top.val(), base.val(), screws.val()


def halfcut(shape, y=0.0):
    keep = cq.Solid.makeBox(120, 60, 80, pnt=cq.Vector(-60, y, -40))
    return shape.intersect(keep)


DETAIL = {
    "A": [((GR - 0.5, 0, P.GLASS_TOP_Z + 1.0), "lip: 1.0 over glass, 1.0 proud", (-330, -60), "black"),
          ((SEAT_R, 0, 2.0), "seat bore D49.26 (+0.3)", (260, 0), "tab:purple"),
          ((GR, 0, P.GLASS_BOTTOM_Z), "glass edge D48.96", (-330, 60), "tab:green")],
    "B": [((24.4, 0, P.GLASS_TOP_Z + 0.2), "nub over glass round, rim 0.8 proud", (-330, -80), "black"),
          ((24.1, 0, P.GLASS_BOTTOM_Z), "0.75 mm ledge under glass", (-330, 60), "tab:red"),
          ((23.8, 0, 2.0), "ledge bore D47.6", (330, 0), "tab:purple")],
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    g = load_board()
    board = [(k, halfcut(v)) for k, v in g.items()]
    col = {"pcb": (0.1, 0.42, 0.22), "display": (0.12, 0.12, 0.14), "usb_c": (0.8, 0.8, 0.84),
           "pmma_frame": (0.85, 0.85, 0.8), "standoffs": (0.85, 0.7, 0.3)}
    ta, ba = option_a()
    tb, bb, sc = option_b()
    gl = (-GR, 0, P.GLASS_TOP_Z)
    for name, parts, title, labels in [
        ("A", [(ta, (0.93, 0.93, 0.95)), (ba, (0.55, 0.75, 0.95))],
         "Option A - PLAN lip, board loads from behind (no screws)",
         [((-GR + 0.5, 0, P.GLASS_TOP_Z + 1.0), "1.0 mm lip over glass edge\nrim 1.0 mm proud", (-60, -170), "black"),
          ((-23.8, 0, -1.67), "USB-C (port wall on base tray)", (-40, 150), "tab:blue"),
          ((13.75, 0, -3.5), "base cups clamp the\nM2 standoff faces", (160, 130), "tab:orange"),
          ((0, 0, 4), "board drops in from behind ^", (0, -40), "tab:green")]),
        ("B", [(tb, (0.93, 0.93, 0.95)), (bb, (0.55, 0.75, 0.95)), (sc, (0.6, 0.6, 0.6))],
         "Option B - PrintSphere style, glass loads from the front",
         [((-24.3, 0, P.GLASS_TOP_Z + 0.4), "nubs 0.27 mm over glass round\nrim 0.8 mm proud", (-60, -170), "black"),
          ((-24.1, 0, P.GLASS_BOTTOM_Z), "0.75 mm ledge under glass", (-260, 60), "tab:red"),
          ((13.75, 0, -5.5), "3x M2 screws pull\nthe board back", (160, 130), "tab:orange"),
          ((0, 0, 4), "board drops in from the front v", (0, -40), "tab:green")]),
    ]:
        for view, zoom, suffix in [((( 0, -1, 0.35), (0, 0, 1)), 1.0, "section"),
                                   (((0, -1, 0.0), (0, 0, 1)), 3.2, "edge_detail")]:
            s = Scene(size=(1600, 1000))
            for k, sh in board:
                s.add(sh, col.get(k, (0.35, 0.35, 0.38)))
            for p, c in parts:
                s.add(halfcut(p), c)
            if suffix == "edge_detail":
                s.camera(view, target=(25.0, 0, 3.0), zoom=zoom)
                lab = DETAIL[name]
            else:
                s.camera(view, zoom=zoom)
                lab = labels
            save_labelled(s, OUT / f"bezel_{name}_{suffix}.png", labels=lab, title=title if suffix == "section" else title + " (edge detail)",
                          footer="Cut through the USB-C (Y=0). Board from Waveshare STEP. Light grey = top shell, blue = base tray.")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
