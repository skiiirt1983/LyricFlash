"""v10 assembly simulation (Paul's rules, DESIGN_RULES.md).

1. Board + carrier into the base: USB-C first, then swung down about the USB-C hinge line. This is the same
   relative motion as the old base swing; the top shell isn't on yet.
2. Top shell onto the base: straight down along the board's normal, last.

Each pose must keep >= 0.2 mm of air to the board, the screen and the carrier, except for designed contacts.
Printed-to-printed guides (the shell's rim inside the base wall, 0.15 mm) may come down to 0.1 mm.
Writes out/phase2/assembly_sim.md.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import numpy as np  # noqa: E402

import params as P  # noqa: E402
from lfcase import enclosure as E  # noqa: E402
from lfcase.board import load_board  # noqa: E402
from lfcase.meshsim import Cloud, clearance, rot_matrix  # noqa: E402

AIR, GUIDE = 0.2, 0.1
SWING = (0.0, 0.25, 0.5, 1, 1.5, 2, 3, 4, 5, 6, 8, 10, 12, 15, 18, 22, 26, 30)
DROP = (0.0, 0.05, 0.1, 0.2, 0.3, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0)
ELECTRONICS = ("pcb", "display", "pmma_frame", "usb_c", "header", "j1", "h3", "standoffs", "key1", "key2",
               "misc_back", "mic1", "mic2")


def _board(zb):
    g = load_board(verbose=False)
    return {k: E.to_wf(v, zb) for k, v in g.items() if k not in ("display_fpc", "misc_front")}


def swing_designed(m, f, where, a):
    if f in ("pcb", "pmma_frame") and where[0] < P.BOARD_X - 21.4:
        return "USB-C web on the PCB edge (zero gap by design, separating)"
    if f == "carrier" and a <= 4:
        return "plate tabs settling onto their posts beside the hinge (printed on printed, nothing on the board)"
    return None


def drop_designed(m, f, where, d):
    if f == "base" and d <= 0.2:
        return "seam faces closing"
    return None


def run(parts, zb, spacing=0.12, out=None):
    board = _board(zb)
    rows = []
    # 1. swing
    fixed = {k: Cloud(v, spacing) for k, v in board.items()}
    fixed["carrier"] = Cloud(parts["carrier"], spacing)
    mov = {"base": Cloud(parts["base"], spacing)}
    H = E.bf_point_to_wf((P.TILT_HINGE[0], 0, P.TILT_HINGE[1]), zb)
    ax = E.bf_point_to_wf((P.TILT_HINGE[0], 1, P.TILT_HINGE[1]), zb) - H
    swing_rows = []
    for a in SWING:
        R, t = rot_matrix(H, ax, a)
        res, tight, des = {}, {}, {}
        for mk, mc in mov.items():
            for fk, fc in fixed.items():
                d, where = clearance(mc, R, t, fc)
                if d < 1.0:
                    w = tuple(round(float(v), 1) for v in where)
                    res[(mk, fk)] = (round(d, 2), w)
                    if d < AIR:
                        why = swing_designed(mk, fk, where, a)
                        (des if why else tight)[(mk, fk)] = why or (round(d, 2), w)
        swing_rows.append((a, res, tight, des))
        print(f"swing {a:5.2f} deg tight={tight}", flush=True)
    # 2. shell drop along the normal
    n = E.bf_point_to_wf((0, 0, 1), zb) - E.bf_point_to_wf((0, 0, 0), zb)
    fixed2 = dict(fixed)
    fixed2["base"] = mov["base"]
    shell = Cloud(parts["top_shell"], spacing)
    drop_rows = []
    for dd in DROP:
        R, t = np.eye(3), n * dd
        res, tight, des = {}, {}, {}
        for fk, fc in fixed2.items():
            d, where = clearance(shell, R, t, fc)
            if d < 1.0:
                w = tuple(round(float(v), 1) for v in where)
                res[("top_shell", fk)] = (round(d, 2), w)
                lim = GUIDE if fk == "base" else AIR
                if d < lim:
                    why = drop_designed("top_shell", fk, where, dd)
                    if why:
                        des[("top_shell", fk)] = why
                    else:
                        tight[("top_shell", fk)] = (round(d, 2), w)
        drop_rows.append((dd, res, tight, des))
        print(f"drop {dd:5.2f} mm tight={tight}", flush=True)
    ok_s = all(not r[2] for r in swing_rows)
    ok_d = all(not r[2] for r in drop_rows)
    if out:
        L = ["# Phase 2 v10 assembly simulation", "",
             f"Surface-sample check, spacing {spacing} mm (good to about +/-{spacing / 2:.2f} mm). Rule: >= {AIR} mm "
             f"of air to the board, screen and carrier on every path; printed-to-printed guides >= {GUIDE} mm.", "",
             "## 1. Board + carrier into the base (USB-C first, swung down about the USB-C hinge)", "",
             "| Opened by | Closest (mm, < 1.0) | Designed | FAIL |", "|---|---|---|---|"]
        for a, res, tight, des in swing_rows:
            near = ", ".join(f"{f} {v[0]}" for (m, f), v in sorted(res.items(), key=lambda kv: kv[1][0])[:5]) or "all > 1.0"
            L.append(f"| {a:g} deg | {near} | {'; '.join(sorted(set(des.values()))) or '-'} | "
                     f"{', '.join(f'{f} {v[0]} at {v[1]}' for (m, f), v in tight.items()) or 'none'} |")
        L += ["", f"**Swing: {'PASS' if ok_s else 'FAIL'}**", "",
              "## 2. Top shell onto the base (straight down along the board normal)", "",
              "| Above closed by | Closest (mm, < 1.0) | Designed | FAIL |", "|---|---|---|---|"]
        for dd, res, tight, des in drop_rows:
            near = ", ".join(f"{f} {v[0]}" for (m, f), v in sorted(res.items(), key=lambda kv: kv[1][0])[:5]) or "all > 1.0"
            L.append(f"| {dd:g} mm | {near} | {'; '.join(sorted(set(des.values()))) or '-'} | "
                     f"{', '.join(f'{f} {v[0]} at {v[1]}' for (m, f), v in tight.items()) or 'none'} |")
        L += ["", f"**Shell drop: {'PASS' if ok_d else 'FAIL'}**"]
        Path(out).write_text("\n".join(L) + "\n")
    return ok_s, ok_d, swing_rows, drop_rows
