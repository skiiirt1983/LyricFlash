"""Phase 2 tilt-in simulation: the base (with the PWR pin and latch clips) swings closed about the
USB-C hinge line onto the board + top shell. Every pose must keep >= 0.2 mm of air (Paul's rule),
except designed contacts in the closed pose (ledges and the port web on the PCB edge, seam faces).
Overlaps show up as ~0 mm (crossing surfaces), so a pose with nothing under 0.2 mm is also overlap-free.
Writes out/phase2/assembly_sim.md.
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import numpy as np  # noqa: E402

import params as P  # noqa: E402
from lfcase import enclosure as E  # noqa: E402
from lfcase.board import load_board  # noqa: E402
from lfcase.meshsim import Cloud, clearance, rot_matrix  # noqa: E402

ANGLES = (0.0, 0.25, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 8, 10, 12, 15, 18, 22, 26, 30)
AIR = 0.2


def run(moving_shapes: dict, zb=None, spacing=0.12, out=None, top=None):
    zb = zb or E.board_z()
    g = load_board(verbose=False)
    obst = {k: E.to_wf(v, zb) for k, v in g.items() if k not in ("display_fpc", "misc_front")}
    obst["top_shell"] = top if top is not None else E.top_shell(zb)
    fixed = {k: Cloud(v, spacing) for k, v in obst.items()}
    mov = {k: Cloud(v, spacing) for k, v in moving_shapes.items()}
    H = E.bf_point_to_wf((P.TILT_HINGE[0], 0, P.TILT_HINGE[1]), zb)
    ax = E.bf_point_to_wf((P.TILT_HINGE[0], 1, P.TILT_HINGE[1]), zb) - H
    rows, worst = [], {}
    for a in ANGLES:
        R, t = rot_matrix(H, ax, a)
        res = {}
        for mk, mc in mov.items():
            for fk, fc in fixed.items():
                d, where = clearance(mc, R, t, fc)
                if d < 1.0:
                    res[(mk, fk)] = (round(d, 2), tuple(round(float(v), 1) for v in where))
        tight = {k: v for k, v in res.items() if v[0] < AIR}
        rows.append((a, res, tight))
        print(f"{a:5.2f} deg  tight={tight}", flush=True)
    if out:
        L = ["# Phase 2 assembly simulation (tilt-in)", "",
             f"Base swings about the USB-C hinge line (board frame X {P.TILT_HINGE[0]}, Z {P.TILT_HINGE[1]}, along Y). "
             f"Surface-sample check, spacing {spacing} mm (distances good to about +/-{spacing / 2:.2f} mm). "
             f"Rule: >= {AIR} mm of air on the way in.", "",
             "| Opened by | Closest pairs (mm, < 1.0 shown) | Below 0.2 (world X, Y, Z of the spot) |", "|---|---|---|"]
        for a, res, tight in rows:
            near = ", ".join(f"{m}/{f} {v[0]}" for (m, f), v in sorted(res.items(), key=lambda kv: kv[1][0])[:6]) or "all > 1.0"
            tl = ", ".join(f"{m}/{f} {v[0]} at {v[1]}" for (m, f), v in tight.items()) or "none"
            L.append(f"| {a:g} deg | {near} | {tl} |")
        Path(out).write_text("\n".join(L) + "\n")
    return rows
