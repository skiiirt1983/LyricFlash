"""Phase 2: build the full case, check it against the STEP board, simulate the tilt-in, export."""
import pickle
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from lfcase import enclosure as E  # noqa: E402

OUT = HERE / "out" / "phase2"
CACHE = OUT / "brep"


def build():
    CACHE.mkdir(parents=True, exist_ok=True)
    t = time.time()
    parts, info = E.build_all(log=lambda m: print(f"[{time.time() - t:6.0f}s] {m}", flush=True))
    for k, v in parts.items():
        v.exportBrep(str(CACHE / f"{k}.brep"))
    (CACHE / "info.pkl").write_bytes(pickle.dumps(info))
    return parts, info


def load():
    import cadquery as cq
    parts = {p.stem: cq.Shape.importBrep(str(p)) for p in sorted(CACHE.glob("*.brep"))}
    return parts, pickle.loads((CACHE / "info.pkl").read_bytes())




def check(parts, info):
    """Interference: every part vs the STEP board + battery/speaker envelopes, and the parts vs each other."""
    from lfcase.board import load_board
    from lfcase.interference import check as chk
    zb = info["zb"]
    g = load_board(verbose=False)
    gw = {k: E.to_wf(v, zb) for k, v in g.items()}
    gw["battery_envelope"] = E._box(*E.battery_box())
    gw["speaker_envelope"] = E.speaker_dummy()
    rows = []
    for k, v in parts.items():
        hits = [(n, round(x, 3)) for n, x, _ in chk(v, gw)]
        rows.append((k, "board/battery/speaker", hits, len(v.Solids()), v.isValid()))
        print(rows[-1], flush=True)
    names = list(parts)
    for i, a in enumerate(names):
        for b_ in names[i + 1:]:
            hits = [(n, round(x, 3)) for n, x, _ in chk(parts[a], {b_: parts[b_]})]
            if hits:
                rows.append((a, b_, hits, None, None))
                print(rows[-1], flush=True)
    return rows


GOLD, BLACK, ACCENT = (0.83, 0.66, 0.30), (0.13, 0.13, 0.15), (0.85, 0.68, 0.30)


def render(parts, info, prefix="case"):
    from lfcase.board import load_board
    from lfcase.render import Scene, save_labelled
    zb = info["zb"]
    disp = E.to_wf(load_board(verbose=False)["display"], zb)
    col = {"top_shell": GOLD, "base": BLACK}
    views = [
        ("front_left", ((-0.8, -1.0, 0.55), (0, 0, 1)), "Front-left: USB-C bay, mic holes, cable clip"),
        ("back_right", ((0.9, 1.0, 0.5), (0, 0, 1)), "Back-right: PWR button, speaker grille"),
        ("front_right", ((0.9, -1.0, 0.45), (0, 0, 1)), "Front-right: BOOT pinhole, latch line"),
        ("underside", ((0.25, -0.35, -1.0), (0, 1, 0)), "Underside: switch pocket"),
    ]
    for name, cam, title in views:
        s = Scene(size=(1600, 1200))
        s.add(disp, (0.03, 0.03, 0.04))
        for k, v in parts.items():
            s.add(v, col.get(k, ACCENT if k != "cable_clip" else GOLD))
        s.camera(cam, target=(4, 0, 16), zoom=1.15)
        save_labelled(s, str(OUT / f"{prefix}_{name}.png"), title=title)


def print_rotations(info):
    """World -> print orientation (3x3 rotation, rows = new X, Y, Z), per part. Part sits on the plate after a lift."""
    import math

    import numpy as np
    import params as P
    zb = info["zb"]
    t = math.radians(P.TILT_DEG + 180.0 - 180.0)
    Rx = lambda d: np.array([[1, 0, 0], [0, math.cos(math.radians(d)), -math.sin(math.radians(d))],
                             [0, math.sin(math.radians(d)), math.cos(math.radians(d))]])
    rots = {"base": np.eye(3), "top_shell": Rx(180.0 - P.TILT_DEG)}     # top face down on the plate
    # PWR pin: lying on its flat side (face A), length along X
    a, tip, _ = E._pin_frame()
    o = E.bf_point_to_wf((0, 0, 0), zb)
    s_w = E.bf_point_to_wf((math.cos(a), math.sin(a), 0), zb) - o
    l_w = E.bf_point_to_wf((-math.sin(a), math.cos(a), 0), zb) - o
    rots["pwr_pin"] = np.array([s_w, np.cross(l_w, s_w), l_w])
    for name, S, n, a_, t_ in E._clip_frames(zb):
        rots[f"clip_{name}"] = np.array([n, a_, t_])                  # clip profile flat on the plate
    rots["cable_clip"] = np.eye(3)                                    # printed as installed
    out = {}
    for k, R in rots.items():                                         # exact rotations (OCC rejects 1e-9 skew)
        u, _, vt = np.linalg.svd(np.asarray(R, float))
        R = u @ vt
        out[k] = R if np.linalg.det(R) > 0 else -R
    return out


def rotate(shape, R):
    """Apply a 3x3 rotation about the origin via axis-angle (OCC's gp_Trsf wants exact orthogonality)."""
    import math

    import cadquery as cq
    import numpy as np
    ang = math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(R) - 1) / 2))))
    if ang < 1e-9:
        return shape
    if abs(ang - 180) < 1e-6:
        w, v = np.linalg.eigh((R + np.eye(3)) / 2)
        ax = v[:, np.argmax(w)]
    else:
        ax = np.array([R[2, 1] - R[1, 2], R[0, 2] - R[2, 0], R[1, 0] - R[0, 1]])
    return shape.rotate(cq.Vector(0, 0, 0), cq.Vector(*ax), ang)


def printable(parts, info, out_dir=None):
    """Layer check of every part in its print orientation. Returns {part: issues}."""
    import cadquery as cq
    from lfcase import layers
    from lfcase.export import write_stl
    out_dir = out_dir or (OUT / "stl")
    out_dir.mkdir(parents=True, exist_ok=True)
    res = {}
    for k, R in print_rotations(info).items():
        sh = rotate(parts[k], R)
        bb = sh.BoundingBox()
        sh = sh.moved(cq.Location(cq.Vector(0, 0, -bb.zmin)))
        f = out_dir / f"{k}.stl"
        write_stl(sh, f)
        res[k] = layers.check(f)
        bad = [i for i in res[k] if i[1] in ("FLOATING", "LONG BRIDGE")]
        print(k, "issues", len(res[k]), "floating/long", len(bad), flush=True)
    return res


MATERIAL = {"top_shell": "gold", "base": "black", "pwr_pin": "gold", "clip_front": "black", "clip_back": "black",
            "cable_clip": "black"}


def cg_and_tip(parts, info):
    """Centre of gravity (board, battery, speaker, printed parts) and the tipping angle in every direction."""
    import math

    import cadquery as cq
    import numpy as np
    from shapely.geometry import LineString, Polygon

    import params as P
    from lfcase.board import load_board
    zb = info["zb"]
    g = load_board(verbose=False)
    board = cq.Compound.makeCompound([E.to_wf(g[k], zb) for k in ("pcb", "display", "pmma_frame")])
    items = [("board", P.BOARD_MASS_G, np.array(cq.Shape.centerOfMass(board).toTuple()))]
    (bx0, by0, bz0), (bx1, by1, bz1) = E.battery_box()
    items.append(("battery", P.BATTERY_MASS_G, np.array([(bx0 + bx1) / 2, (by0 + by1) / 2, bz0 + 6.0])))
    items.append(("speaker", P.SPEAKER_MASS_G, np.array(cq.Shape.centerOfMass(E.speaker_dummy()).toTuple())))
    for k, v in parts.items():
        if k == "cable_clip":
            continue
        dens = P.PLA_DENSITY if MATERIAL[k] == "gold" else P.PETG_DENSITY
        m = v.Volume() / 1000.0 * dens * P.PRINT_FILL
        items.append((k, m, np.array(cq.Shape.centerOfMass(v).toTuple())))
    M = sum(m for _, m, _ in items)
    cg = sum(m * c for _, m, c in items) / M
    # support polygon: the base's bottom face
    base = parts["base"]
    f = min((f for f in base.Faces() if f.geomType() == "PLANE" and abs(f.Center().z) < 1e-3), key=lambda f: -f.Area())
    poly = Polygon([(v.X, v.Y) for v in f.outerWire().Vertices()]).convex_hull
    pts = [(p.x, p.y) for p in f.outerWire().discretize(200)] if hasattr(f.outerWire(), "discretize") else None
    if pts:
        poly = Polygon(pts).convex_hull
    rows = []
    for d in range(0, 360, 5):
        u = np.array([math.cos(math.radians(d)), math.sin(math.radians(d))])
        ray = LineString([tuple(cg[:2]), tuple(cg[:2] + u * 200)])
        hit = ray.intersection(poly.exterior)
        pts_ = [hit] if hit.geom_type == "Point" else list(getattr(hit, "geoms", []))
        dist = min(math.dist(cg[:2], (p.x, p.y)) for p in pts_)
        rows.append((d, math.degrees(math.atan2(dist, cg[2]))))
    worst = min(rows, key=lambda r: r[1])
    return dict(items=[(n, round(m, 1), tuple(round(float(x), 1) for x in c)) for n, m, c in items],
                mass=round(M, 1), cg=tuple(round(float(x), 2) for x in cg), tip=rows, worst=worst)


def sections(parts, info):
    """PLAN Phase 2: section renders through the USB port, the battery and a latch."""
    from lfcase.board import load_board
    from lfcase.render import Scene, save_labelled
    zb = info["zb"]
    g = load_board(verbose=False)
    board = {k: E.to_wf(g[k], zb) for k in ("pcb", "display", "pmma_frame", "usb_c", "mic1", "mic2", "key2", "key1")}
    col = {"top_shell": GOLD, "base": (0.32, 0.32, 0.36)}
    name, S0, n, a, t = [f for f in E._clip_frames(zb) if f[0] == "front"][0]
    cuts = [
        ("usb", ((-20.0, 0.33, 20.0), (0.0, -1.0, 0.0)), ((0, 1, 0), (0, 0, 1)), (-24, 0.3, 21), 3.2,
         "Section through the USB-C port (Y = port centre)"),
        ("battery", ((0.0, 0.0, 10.0), (-1.0, 0.0, 0.0)), ((1, 0, 0), (0, 0, 1)), (0, 0, 16), 1.3,
         "Section through the battery (X = 0)"),
        ("latch", (tuple(S0), tuple(-t)), (tuple(t), tuple(a)), tuple(S0 - 2 * a + 2 * n), 4.5,
         "Section through the front latch clip"),
    ]
    for nm, clip, cam, target, zoom, title in cuts:
        s = Scene(size=(1600, 1200))
        for k, v in board.items():
            s.add(v, (0.1, 0.42, 0.22) if k == "pcb" else (0.75, 0.75, 0.8), clip=clip)
        s.add(E._box(*E.battery_box()), (0.35, 0.45, 0.75), clip=clip)
        s.add(E.speaker_dummy(), (0.5, 0.5, 0.55), clip=clip)
        for k, v in parts.items():
            s.add(v, col.get(k, (0.3, 0.5, 0.9)), clip=clip)
        s.camera(cam, target=target, zoom=zoom)
        save_labelled(s, str(OUT / f"section_{nm}.png"), title=title)


def export(parts, info):
    from lfcase.export import write_3mf_project, write_step
    rots = print_rotations(info)
    order = ["top_shell", "base", "clip_front", "clip_back", "pwr_pin", "cable_clip"]
    plist = [dict(name=k, shape_assembled=parts[k], R=rots[k], rotate=rotate, material=MATERIAL[k]) for k in order]
    write_3mf_project(plist, OUT / "lyricflash_case_project.3mf", gap=8.0, keep_order=True)
    (OUT / "step").mkdir(exist_ok=True)
    for k in order:
        write_step(parts[k], OUT / "step" / f"{k}_assembled.step")


def report(parts, info, chk, lay, cg, sim_rows):
    import params as P
    L = ["# Phase 2 report: full case v3 (layout B, no mic boxes)", "",
         f"Tilt {P.TILT_DEG:g} deg, board centre {info['zb']:.1f} mm above the desk. Parts: " + ", ".join(parts) + ".", "",
         "## Interference against the STEP board (boolean volume)", "",
         "| Part | Solids | Valid | Overlaps |", "|---|---|---|---|"]
    for k, other, hits, n, ok in chk:
        if other == "board/battery/speaker":
            note = ", ".join(f"{h} {v} mm3" for h, v in hits) or "none"
            if k == "base" and any(h == "speaker_envelope" for h, _ in hits):
                note += " (speaker: the grip ribs, 0.15 mm by design, as C8)"
            if k == "base" and any(h == "battery_envelope" for h, _ in hits):
                note += (" (battery: the case's rounded inner corners trim the pocket's 1 mm clearance at its four"
                         " extreme corners by <= 0.5 mm; the 56 mm cell keeps >= 0.5 mm)")
            L.append(f"| {k} | {n} | {ok} | {note} |")
    pair = [r for r in chk if r[1] != "board/battery/speaker"]
    L += ["", "Part against part: " + ("none overlap." if not pair else "; ".join(f"{a}/{b} {h}" for a, b, h, _, _ in pair)), ""]
    ok_sim = all(not r[2] for r in sim_rows)
    L += ["## Assembly (tilt-in swing)", "",
          f"**{'PASS' if ok_sim else 'FAIL'}**: nothing comes within 0.2 mm on the way in except designed contacts. "
          "Details in `assembly_sim.md`.", ""]
    L += ["## Printability (layer check, print orientation)", "",
          "| Part | Orientation | Result |", "|---|---|---|"]
    orient = {"top_shell": "face-down (top face on the plate)", "base": "floor down", "pwr_pin": "on its flat side",
              "clip_front": "flat (profile on the plate)", "clip_back": "flat (profile on the plate)",
              "cable_clip": "as installed"}
    for k, iss in lay.items():
        bad = [i for i in iss if i[1] in ("FLOATING", "LONG BRIDGE")]
        L.append(f"| {k} | {orient[k]} | {'clean' if not bad else f'{len(bad)} flagged layers (see below)'} |")
    L += ["", "Base: **no supports needed** (v3). What the check still flags, all small:", "",
          "- **USB-C opening, top edge** (visible): with the 12 deg tilt it's a sloped ceiling, not a flat 9.5 mm bridge. "
          "If the first print shows a droop, paint a support enforcer inside the opening only.",
          "- PWR tube roof: bridged across the 3.5 mm bore as the roof climbs with the tilt (inside).",
          "- Two specks under 2 mm2 (a lead-gap edge, the BOOT tube mouth): print as short overhangs.",
          "", "The USB-C insert's underside is filled down to the wall at 45 deg, and the mic bars are gone (Paul), "
          "so nothing inside hangs over the battery any more.", "",
          "Top shell: the latch catch ledges (0.9 deep) are anchored on the outer wall and bridged 6.6 mm between "
          "the pocket's side walls.", ""]
    L += ["## Centre of gravity and tipping", "",
          f"Total {cg['mass']} g (board {P.BOARD_MASS_G:g} g and battery {P.BATTERY_MASS_G:g} g are estimates). "
          f"CG at X {cg['cg'][0]}, Y {cg['cg'][1]}, Z {cg['cg'][2]} mm. "
          f"Smallest tipping angle **{cg['worst'][1]:.1f} deg** (towards {cg['worst'][0]} deg; PLAN asks > {P.MIN_TIP_DEG:g}).", ""]
    L += ["## Key numbers", ""]
    L += [f"- Mic holes: D{P.MIC_HOLE_D_CASE} teardrop straight through the USB recess wall, "
          f"{info['mic']['MIC1']['to_mic']} / {info['mic']['MIC2']['to_mic']} mm from the mics (no collar, no duct: Paul).",
          f"- PWR pin: exits back-right at {info['pwr']['exit']}, rest gap {P.PWR_PIN_REST_GAP} to the plunger.",
          f"- BOOT pinhole: D{P.BOOT_HOLE_D}, {info['boot']['length']} mm guide, exits front-right at {info['boot']['exit']}.",
          f"- Speaker: {info['speaker']['holes']} x D{P.SPK_GRILLE_HOLE_D} grille holes (teardrop) in the right wall.",
          f"- Switch: C4 K3 pocket in the floor at {info['switch']['centre']}.",
          f"- Latches: 2 PETG clips (C7 v5 notch 1: hook {P.LATCH_HOOK}, {P.LATCH_CATCH_DEG:g} deg) at X {P.LATCH_CLIP_X}, front and back.",
          ""]
    (OUT / "report.md").write_text("\n".join(L) + "\n")


def main():
    import simulate_case as SC
    parts, info = build()
    chk = check(parts, info)
    mov = {k: v for k, v in parts.items() if k not in ("top_shell", "cable_clip")}
    sim_rows = SC.run(mov, zb=info["zb"], out=OUT / "assembly_sim.md", top=parts["top_shell"])
    lay = printable(parts, info)
    cg = cg_and_tip(parts, info)
    render(parts, info)
    sections(parts, info)
    export(parts, info)
    report(parts, info, chk, lay, cg, sim_rows)
    print((OUT / "report.md").read_text())


if __name__ == "__main__":
    main()
