"""Phase 1: build the fit-test coupons, check them, export STL/STEP/3MF, render.

    python phase1.py           # (Linux headless: xvfb-run -a python phase1.py)

Writes out/phase1/<coupon>/*.stl|*.step|*.png, out/phase1/lyricflash_coupons.3mf
and out/phase1/report.md (checks + interference results).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import cadquery as cq  # noqa: E402

import params as P  # noqa: E402
from lfcase import coupons as C  # noqa: E402
from lfcase.export import mesh_report, pack, write_3mf, write_step, write_stl  # noqa: E402
from lfcase.render import Scene, save_labelled  # noqa: E402

OUT = HERE / "out" / "phase1"
BUILDERS = [C.battery_tray, C.switch_pocket, C.snap_latches]

# Phase 1 coupons still waiting on a decision (see README / report)
PENDING = {
    "C1_display_seat_ring": "waits for the bezel choice (option A lip vs option B PrintSphere style)",
    "C2_usb_wall": "waits for the bezel choice (it includes a slice of the glass seat)",
    "C5_mic_duct": "waits for the mic sound-port side (top or bottom port)",
    "C6_pwr_flexure": "waits for the bezel choice (decides which shell carries the flexure and its direction)",
}


def overhang_report(shape: cq.Shape, max_deg=45.0):
    """Downward-facing area steeper than max_deg from vertical, excluding the bed face."""
    from lfcase.export import to_mesh
    import numpy as np
    v, t = to_mesh(shape, tol=0.02)
    a, b, c = v[t[:, 0]], v[t[:, 1]], v[t[:, 2]]
    n = np.cross(b - a, c - a)
    area = np.linalg.norm(n, axis=1) / 2
    n = n / (2 * area[:, None] + 1e-12)
    zc = (a[:, 2] + b[:, 2] + c[:, 2]) / 3
    zmin = v[:, 2].min()
    down = n[:, 2] < -math.cos(math.radians(90 - max_deg)) - 0.01   # strictly steeper than 45 deg
    off_bed = zc > zmin + 0.05
    sel = down & off_bed
    return float(area[sel].sum()), zc[sel]


def main(only=None):
    OUT.mkdir(parents=True, exist_ok=True)
    all_parts, lines = {}, ["# Phase 1 coupon report", ""]
    for build in BUILDERS:
        cp = build()
        if only and cp.name not in only:
            continue
        d = OUT / cp.name
        d.mkdir(exist_ok=True)
        lines += [f"## {cp.name}: {cp.title}", ""] + [f"- {n}" for n in cp.notes]
        for pn, sh in cp.parts.items():
            assert sh.isValid(), f"{pn} is not a valid solid"
            write_stl(sh, d / f"{pn}.stl")
            write_step(sh, d / f"{pn}.step")
            ntri, bad = mesh_report(sh)
            ov_area, _ = overhang_report(sh)
            bb = sh.BoundingBox()
            lines.append(f"- `{pn}`: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm, volume {sh.Volume()/1000:.2f} cm3, "
                         f"{ntri} triangles, open mesh edges: {bad}, unsupported overhang (>45 deg, off-bed): {ov_area:.1f} mm2")
            all_parts[f"{cp.name}__{pn}"] = sh
        for k, v in cp.checks.items():
            if k == "beam_strain":
                ok = {m: v <= lim for m, lim in P.STRAIN_LIMIT.items()}
                lines.append(f"- Latch beam strain at full hook deflection: {v*100:.2f} % "
                             f"(limit PLA {P.STRAIN_LIMIT['PLA']*100:.1f} %: {'OK' if ok['PLA'] else 'TOO HIGH'}, "
                             f"PETG {P.STRAIN_LIMIT['PETG']*100:.1f} %: {'OK' if ok['PETG'] else 'TOO HIGH'})")
        if cp.board_check is None:
            lines.append("- Interference with the STEP board: not applicable (coupon does not hold the board).")
        lines.append("")
        # renders
        comp = cq.Compound.makeCompound(list(cp.parts.values()))
        for view, zoom in (("iso", 1.0), ("front", 1.0)):
            s = Scene(size=(1400, 1000))
            s.add(comp, (0.93, 0.62, 0.20))
            s.camera(view if view != "front" else ((0, 0, 1), (0, 1, 0)), zoom=zoom)
            save_labelled(s, d / f"{cp.name}_{'iso' if view == 'iso' else 'top'}.png",
                          title=f"{cp.name}: {cp.title}", footer="As printed (bed = bottom). Orange = print.")
    lines += ["## Pending coupons", ""] + [f"- {k}: {v}" for k, v in PENDING.items()] + [""]
    plate = pack(all_parts)
    write_3mf(plate, OUT / "lyricflash_coupons.3mf")
    (OUT / "report.md").write_text("\n".join(lines))
    print("\n".join(lines))


if __name__ == "__main__":
    main(sys.argv[1:] or None)
