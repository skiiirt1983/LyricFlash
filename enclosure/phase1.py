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

import gc  # noqa: E402

import cadquery as cq  # noqa: E402

import params as P  # noqa: E402
from lfcase import coupons as C  # noqa: E402
from lfcase import coupons_a as A  # noqa: E402
from lfcase.board import load_board  # noqa: E402
from lfcase.interference import check as interference  # noqa: E402
from lfcase.export import mesh_report, pack, write_3mf, write_step, write_stl  # noqa: E402
from lfcase.render import Scene, save_labelled  # noqa: E402

OUT = HERE / "out" / "phase1"
BUILDERS = [A.c1_display_seat, A.c2_usb, C.battery_tray, C.switch_pocket, A.c5_mic, A.c6_flexure, C.snap_latches, C.speaker_pocket]
PENDING = {}
BOARD_COLORS = {"pcb": (0.10, 0.42, 0.22), "display": (0.12, 0.12, 0.14), "usb_c": (0.80, 0.80, 0.84),
                "pmma_frame": (0.85, 0.85, 0.80), "standoffs": (0.85, 0.70, 0.30), "key1": (0.95, 0.55, 0.1),
                "key2": (0.95, 0.55, 0.1), "mic1": (0.85, 0.15, 0.15), "mic2": (0.85, 0.15, 0.15)}
PART_COLORS = [(0.93, 0.93, 0.95), (0.55, 0.75, 0.95)]

# Intended contacts: (coupon, part, board group) -> why the overlap is expected
INTENDED = {
    ("C5_mic_duct", "mic_base", "pcb"): f"crush ribs, {P.MIC_CRUSH} mm into the PCB by design",
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
    cen = (a + b + c) / 3
    return float(area[sel].sum()), cen[sel]


def overhang_clusters(cen, link=1.5):
    """Group overhang triangle centroids into clusters -> list of bboxes (print frame)."""
    import numpy as np
    pts = [tuple(p) for p in cen]
    clusters = []
    for p in pts:
        for cl in clusters:
            if any(abs(p[0] - q[0]) < link and abs(p[1] - q[1]) < link and abs(p[2] - q[2]) < link for q in cl):
                cl.append(p)
                break
        else:
            clusters.append([p])
    out = []
    for cl in clusters:
        a = np.array(cl)
        out.append((a.min(0), a.max(0), len(cl)))
    return out


def main(only=None):
    OUT.mkdir(parents=True, exist_ok=True)
    groups = load_board()
    all_parts, lines = {}, ["# Phase 1 coupon report", ""]
    for build in BUILDERS:
        cp = build()
        if only and cp.name not in only:
            continue
        d = OUT / cp.name
        d.mkdir(exist_ok=True)
        lines += [f"## {cp.name}: {cp.title}", ""] + [f"- {n}" for n in cp.notes]
        board_parts = cp.board_check() if cp.board_check else {}
        for pn, sh in list(cp.parts.items()):
            if pn in board_parts:
                sh = A.print_orientation(pn, sh)
                cp.parts[pn] = sh
            assert sh.isValid(), f"{pn} is not a valid solid"
            write_stl(sh, d / f"{pn}.stl")
            write_step(sh, d / f"{pn}.step")
            import trimesh
            _m = trimesh.load(d / f"{pn}.stl")
            ntri, bad = len(_m.faces), (0 if _m.is_watertight else "NOT WATERTIGHT")
            ov_area, ov_cen = overhang_report(sh)
            bb = sh.BoundingBox()
            lines.append(f"- `{pn}`: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm, volume {sh.Volume()/1000:.2f} cm3, "
                         f"{ntri} triangles, mesh: {"watertight" if bad == 0 else bad}, unsupported overhang (>45 deg, off-bed): {ov_area:.1f} mm2")
            from lfcase.layers import check as layer_check
            lay = layer_check(d / f"{pn}.stl")
            fl = [x for x in lay if x[1] not in ("bridge", "short overhang")]
            br = [x for x in lay if x[1] in ("bridge", "short overhang")]
            lines.append(f"- `{pn}` layer check: {'**no floating regions**' if not fl else f'**{len(fl)} FLOATING / long-bridge patches**'}"
                         f", {len(br)} bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)")
            for z, kind, area, ln, bb, reach in (fl + br)[:8]:
                lines.append(f"    - {kind} at print z {z}: {area} mm2, {ln} mm long, reaches {reach} mm past support, XY {bb}")
            if ov_area > 0:
                for lo, hi, n in overhang_clusters(ov_cen):
                    lines.append(f"    - overhang at print X[{lo[0]:.1f},{hi[0]:.1f}] Y[{lo[1]:.1f},{hi[1]:.1f}] Z {lo[2]:.1f}..{hi[2]:.1f}")
            all_parts[f"{cp.name}__{pn}"] = sh
        for k, v in cp.checks.items():
            if k in ("latch_strain", "flex_strain"):
                lines.append(f"- {k.replace('_', ' ')}: {v*100:.2f} % (PLA limit {P.STRAIN_LIMIT['PLA']*100:.1f} %, PETG {P.STRAIN_LIMIT['PETG']*100:.1f} %)")
            if k == "beam_strain":
                ok = {m: v <= lim for m, lim in P.STRAIN_LIMIT.items()}
                lines.append(f"- Latch beam strain at full hook deflection: {v*100:.2f} % "
                             f"(limit PLA {P.STRAIN_LIMIT['PLA']*100:.1f} %: {'OK' if ok['PLA'] else 'TOO HIGH'}, "
                             f"PETG {P.STRAIN_LIMIT['PETG']*100:.1f} %: {'OK' if ok['PETG'] else 'TOO HIGH'})")
        if not board_parts:
            lines.append("- Interference with the STEP board: not applicable (coupon does not hold the board).")
        for pn, bsh in board_parts.items():
            hits = interference(bsh, groups)
            if not hits:
                lines.append(f"- Interference `{pn}` vs STEP board: **0 mm3** (no contact with any board part)")
            for g, vol, bb in hits:
                why = INTENDED.get((cp.name, pn, g))
                tag = f"intended: {why}" if why else "**UNEXPECTED**"
                lines.append(f"- Interference `{pn}` vs `{g}`: {vol:.3f} mm3 at X[{bb[0]:.2f},{bb[3]:.2f}] "
                             f"Y[{bb[1]:.2f},{bb[4]:.2f}] Z[{bb[2]:.2f},{bb[5]:.2f}], {tag}")
        lines.append("")
        # assembled section renders with the STEP board
        for stem, phi, target, zoom, labs in cp.sections:
            import math as _m
            keep = cq.Solid.makeBox(140, 70, 80, pnt=cq.Vector(-70, 0, -40)).rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), phi)
            s = Scene(size=(1600, 1100))
            nrm = (-_m.sin(_m.radians(phi)), _m.cos(_m.radians(phi)), 0.0)
            for gname, gshape in groups.items():
                s.add(gshape, BOARD_COLORS.get(gname, (0.35, 0.35, 0.38)), tol=0.05, clip=((0, 0, 0), nrm))
            for i, (pn, bsh) in enumerate(board_parts.items()):
                s.add(bsh.intersect(keep), PART_COLORS[i % len(PART_COLORS)])
            vd = (_m.sin(_m.radians(phi)), -_m.cos(_m.radians(phi)), 0.18)
            s.camera((vd, (0, 0, 1)), target=target, zoom=zoom)
            save_labelled(s, d / f"{cp.name}_{stem}.png", title=f"{cp.name}: assembled on the STEP board (cut)",
                          labels=[(p3, t, off, "black") for p3, t, off in labs],
                          footer="Board from Waveshare STEP. Light grey = top shell, blue = base tray, orange = single part.")
        gc.collect()
        # renders
        comp = cq.Compound.makeCompound(list(pack(dict(cp.parts), gap=4.0).values()))
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
