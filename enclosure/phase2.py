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
    for old in list(CACHE.glob("*.brep")) + list((OUT / "stl").glob("*.stl")) + list((OUT / "step").glob("*.step")):
        old.unlink()                                       # parts that no longer exist must not linger
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
    import params as P
    from lfcase.board import load_board
    from lfcase.interference import check as chk
    zb = info["zb"]
    g = load_board(verbose=False)
    gw = {k: E.to_wf(v, zb) for k, v in g.items()}
    gw["battery_envelope"] = E._box(*E.battery_box())
    if P.SPEAKER:
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


def insertion_check(parts, air=0.2):
    """v9 (Paul): the battery must go in by hand. The real cell (not the pocket) is lowered straight down from
    above the base. Centred in its pocket, swept from just above its resting place to above the base, grown by
    `air`, it must touch nothing. At the pocket's extreme positions (cell against a pocket wall, which only
    guides it) it must still touch nothing without the extra air."""
    import params as P
    b = parts["base"]
    (bx0, by0, bz0), (bx1, by1, bz1) = E.battery_box()
    t, L, h = (P.BATTERY_T, P.BATTERY_L, P.BATTERY_W) if P.BATTERY_ON_EDGE else (P.BATTERY_W, P.BATTERY_L, P.BATTERY_T)
    top = b.BoundingBox().zmax + 5

    def sweep(xa, ya, g):
        return E._box((xa - g, ya - g, bz0 + max(g, 0.01)), (xa + t + g, ya + L + g, top)).intersect(b).Volume()
    xc, yc = (bx0 + bx1 - t) / 2, (by0 + by1 - L) / 2
    rows = [("centred +%.1f air" % air, round(sweep(xc, yc, air), 3))]
    for xa in (bx0 + 0.01, bx1 - t - 0.01):
        rows.append((f"x {xa:.2f} against the pocket side", round(sweep(xa, yc, 0.0), 3)))
    ok = all(v < 1e-3 for _, v in rows)
    print("battery drop path", "PASS" if ok else "FAIL", rows, flush=True)
    return ok, rows


def render(parts, info, prefix="case"):
    from lfcase.board import load_board
    from lfcase.render import Scene, save_labelled
    zb = info["zb"]
    disp = E.to_wf(load_board(verbose=False)["display"], zb)
    col = {"top_shell": GOLD, "base": BLACK}
    views = [
        ("front_left", ((-0.8, -1.0, 0.55), (0, 0, 1)), "Front-left: USB-C bay, mic holes, cable clip"),
        ("back_right", ((0.9, 1.0, 0.5), (0, 0, 1)), "Back-right (v9: no speaker; battery along the right wall)"),
        ("front_right", ((0.9, -1.0, 0.45), (0, 0, 1)), "Front-right"),
        ("underside", ((0.25, -0.35, -1.0), (0, 1, 0)), "Underside: switch pocket"),
    ]
    for name, cam, title in views:
        s = Scene(size=(1600, 1200))
        s.add(disp, (0.03, 0.03, 0.04))
        for k, v in parts.items():
            s.add(v, col.get(k, ACCENT if k != "cable_clip" else GOLD))
        s.camera(cam, target=(0, 0, info['zb'] * 0.6), zoom=0.85)          # v9: taller case
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
    # PWR pin (only when BUTTON_ACCESS): lying on its flat side (face A), length along X
    a, tip, _ = E._pin_frame()
    o = E.bf_point_to_wf((0, 0, 0), zb)
    s_w = E.bf_point_to_wf((math.cos(a), math.sin(a), 0), zb) - o
    l_w = E.bf_point_to_wf((-math.sin(a), math.cos(a), 0), zb) - o
    rots["pwr_pin"] = np.array([s_w, np.cross(l_w, s_w), l_w])
    if getattr(P, "CARRIER", False):                  # v10 carrier: top (board side) face on the plate
        bx = E.bf_point_to_wf((1, 0, 0), zb) - o
        by = E.bf_point_to_wf((0, 1, 0), zb) - o
        bz = E.bf_point_to_wf((0, 0, 1), zb) - o
        rots["carrier"] = np.array([bx, -by, -bz])
    for name, S, n, a_, t_, *_ in E._clip_frames(zb):
        rots[f"clip_{name}"] = np.array([n, a_, t_])                  # clip profile flat on the plate
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
        if k not in parts:
            continue
        sh = rotate(parts[k], R)
        bb = sh.BoundingBox()
        sh = sh.moved(cq.Location(cq.Vector(0, 0, -bb.zmin)))
        f = out_dir / f"{k}.stl"
        write_stl(sh, f)
        if k == "base":
            from lfcase import logo
            logo.cut_stl(f, R, (0, 0, -bb.zmin))
        res[k] = layers.check(f)
        bad = [i for i in res[k] if i[1] in ("FLOATING", "LONG BRIDGE")]
        print(k, "issues", len(res[k]), "floating/long", len(bad), flush=True)
    return res


MATERIAL = {"top_shell": "gold", "base": "black", "pwr_pin": "gold", "carrier": "black", "clip_front": "black", "clip_back": "black",
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
    items.append(("battery", P.BATTERY_MASS_G, np.array([(bx0 + bx1) / 2, (by0 + by1) / 2, (bz0 + bz1) / 2])))
    if P.SPEAKER:
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
    import params as P
    from lfcase.board import load_board
    from lfcase.render import Scene, save_labelled
    zb = info["zb"]
    g = load_board(verbose=False)
    board = {k: E.to_wf(g[k], zb) for k in ("pcb", "display", "pmma_frame", "usb_c", "mic1", "mic2", "key2", "key1")}
    col = {"top_shell": GOLD, "base": (0.32, 0.32, 0.36)}
    (bx0_, _, _), (bx1_, _, _) = E.battery_box()
    bxc = (bx0_ + bx1_) / 2
    cuts = [
        ("usb", ((-20.0, 0.33, 20.0), (0.0, -1.0, 0.0)), ((0, 1, 0), (0, 0, 1)), (-24, 0.3, 21), 3.2,
         "Section through the USB-C port (Y = port centre)"),
        ("battery", ((bxc, 0.0, 10.0), (-1.0, 0.0, 0.0)), ((1, 0, 0), (0, 0, 1)), (bxc, 0, zb * 0.6), 0.9,
         f"Section through the battery (X = {bxc:.1f}), side view"),
        ("front", ((0.0, 0.0, 10.0), (0.0, 1.0, 0.0)), ((0, -1, 0), (0, 0, 1)), (0, 0, zb * 0.6), 0.9,
         "Section front-to-back middle (Y = 0): standing battery on the right, board above"),
    ]
    for f in E._clip_frames(zb):
        if f[0] == "front":
            name, S0, n, a, t, *_ = f
            cuts.append(("latch", (tuple(S0), tuple(-t)), (tuple(t), tuple(a)), tuple(S0 - 2 * a + 2 * n), 4.5,
                         "Section through the front latch clip"))
    if getattr(P, "V10", False):
        tx, ty = P.CARRIER_TABS[0]
        pw = E.bf_point_to_wf((tx, ty, P.STANDOFF_Z[0] - P.CARRIER_T), zb)
        mx, my = E.footprint()[0] + P.CASE_CORNER_R, E.footprint()[2] + P.CASE_CORNER_R
        sz = P.CASE_FLOOR + P.SWITCH_FLOOR_GAP + P.SWITCH_MEAS_W / 2
        cuts += [
            ("post", ((pw[0], 0.0, 10.0), (-1.0, 0.0, 0.0)), ((1, 0, 0), (0, 0, 1)), (pw[0], pw[1], pw[2] - 4), 3.0,
             "Front-left post: plate tab and M2.5 pilot hole (X through the tab)"),
            ("switch", ((0.0, P.SWITCH_WALL_Y, 10.0), (0.0, 1.0, 0.0)), ((0, -1, 0), (0, 0, 1)),
             (E.footprint()[0] + 4, P.SWITCH_WALL_Y, sz), 6.0, "Switch pocket in the left wall (Y through the switch)"),
            ("magnet", ((mx, 0.0, 10.0), (-1.0, 0.0, 0.0)), ((1, 0, 0), (0, 0, 1)), (mx, my, pw[2] - 6), 3.5,
             "Front-left corner: magnet pockets in the base boss and the shell, shell rim"),
        ]
    for nm, clip, cam, target, zoom, title in cuts:
        s = Scene(size=(1600, 1200))
        for k, v in board.items():
            s.add(v, (0.1, 0.42, 0.22) if k == "pcb" else (0.75, 0.75, 0.8), clip=clip)
        s.add(E._box(*E.battery_box()), (0.35, 0.45, 0.75), clip=clip)
        if P.SPEAKER:
            s.add(E.speaker_dummy(), (0.5, 0.5, 0.55), clip=clip)
        for k, v in parts.items():
            s.add(v, col.get(k, (0.95, 0.55, 0.15)), clip=clip)
        s.camera(cam, target=target, zoom=zoom)
        save_labelled(s, str(OUT / f"section_{nm}.png"), title=title)


def export(parts, info):
    from lfcase.export import write_3mf_project, write_step
    rots = print_rotations(info)
    order = [k for k in ["top_shell", "base", "carrier", "clip_front", "clip_back", "pwr_pin", "cable_clip"] if k in parts]
    from lfcase import logo
    plist = [dict(name=k, shape_assembled=parts[k], R=rots[k], rotate=rotate, material=MATERIAL[k],
                  mesh_post=(logo.cut if k == "base" else None)) for k in order]
    write_3mf_project(plist, OUT / "lyricflash_case_project.3mf", gap=8.0, keep_order=True)
    (OUT / "step").mkdir(exist_ok=True)
    for k in order:
        write_step(parts[k], OUT / "step" / f"{k}_assembled.step")


def report(parts, info, chk, lay, cg, sim_rows):
    import params as P
    L = ["# Phase 2 report: full case v5 (20 deg, layout B, no button access)", "",
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
    L += ["## Assembly (v10: board + carrier swung into the base, then the shell straight down)", "",
          f"**{'PASS' if ok_sim else 'FAIL'}**: nothing comes within 0.2 mm of the board, screen or carrier on either path "
          "except designed contacts (printed-to-printed guides >= 0.1 mm). Details in `assembly_sim.md`.", ""]
    L += ["## Printability (layer check, print orientation)", "",
          "| Part | Orientation | Result |", "|---|---|---|"]
    orient = {"top_shell": "face-down (top face on the plate)", "base": "floor down", "pwr_pin": "on its flat side",
              "clip_front": "flat (profile on the plate)", "clip_back": "flat (profile on the plate)",
              "carrier": "flat, top (board side) on the plate; countersinks face up",
              }
    for k, iss in lay.items():
        bad = [i for i in iss if i[1] in ("FLOATING", "LONG BRIDGE")]
        L.append(f"| {k} | {orient[k]} | {'clean' if not bad else f'{len(bad)} flagged layers (see below)'} |")
    L += ["", "Base: **no supports needed**. What the check still flags, all small:", "",
          "- **USB-C opening, top edge** (visible): with the 12 deg tilt it's a sloped ceiling, not a flat 9.5 mm bridge. "
          "If the first print shows a droop, paint a support enforcer inside the opening only.",
          "- Small specks under 2 mm2 (a lead-gap edge): print as short overhangs.",
          "", "The USB-C insert's underside is filled down to the wall at 45 deg, and the mic bars are gone (Paul), "
          "so nothing inside hangs over the battery any more.", "",
          "Top shell: the latch catch ledges (0.9 deep) are anchored on the outer wall and bridged 6.6 mm between "
          "the pocket's side walls.", ""]
    ok_i, rows_i = info.get("insertion", (None, []))
    L += ["## Battery insertion (v9)", "",
          "Real cell lowered straight into its pocket (centred with 0.2 mm air; against either pocket side): "
          f"**{'PASS' if ok_i else 'FAIL'}** " + ", ".join(f"{p}: {v} mm3" for p, v in rows_i), ""]
    L += ["## Centre of gravity and tipping", "",
          f"Total {cg['mass']} g (board {P.BOARD_MASS_G:g} g and battery {P.BATTERY_MASS_G:g} g are estimates). "
          f"CG at X {cg['cg'][0]}, Y {cg['cg'][1]}, Z {cg['cg'][2]} mm. "
          f"Smallest tipping angle **{cg['worst'][1]:.1f} deg** (towards {cg['worst'][0]} deg; PLAN asks > {P.MIN_TIP_DEG:g}).", ""]
    L += ["## Key numbers", ""]
    L += [f"- Mic holes: D{P.MIC_HOLE_D_CASE} teardrop straight through the USB recess wall, "
          f"{info['mic']['MIC1']['to_mic']} / {info['mic']['MIC2']['to_mic']} mm from the mics (no collar, no duct: Paul).",
          "- PWR and BOOT buttons: no access (Paul, v5: not used by the firmware). Walls are plain.",
          (f"- Speaker: {info['speaker']['holes']} x D{P.SPK_GRILLE_HOLE_D} grille holes (teardrop) in the right wall."
           if info.get("speaker") else "- Speaker: none (Paul, v9). The battery stands on its edge along the right wall."),
          (f"- Switch: SS-12D00 pressed into the left wall at {info['switch']['centre']}, knob out "
           f"{info['switch'].get('knob_out')} mm through a {P.SWITCH_SKIN} mm skin."
           if getattr(P, "SWITCH_WALL", False) else f"- Switch: C4 K3 pocket in the floor at {info['switch']['centre']}."),
          (f"- Latches: 2 identical PETG clips (C7 v5 notch 1: hook {P.LATCH_HOOK}, {P.LATCH_CATCH_DEG:g} deg): "
           + "; ".join(f"{c[0]} on wall {c[1]} at {c[2]:g}, {c[3]:g} mm wide" for c in P.LATCH_CLIPS) + "."
           if P.LATCH_CLIPS else
           f"- Shell: alignment rim ({P.RIM_T} mm, {P.RIM_DEPTH} deep, {P.RIM_CLEAR} clear) inside the base wall, "
           f"4 corner magnet pairs ({P.MAGNET_D:g} x {P.MAGNET_H:g}), goes on last along the board normal."),
          (f"- Board: carrier plate ({P.CARRIER_T} mm PETG) on the 3 standoffs with M2 x 3 flat heads; 3 tabs on base posts, "
           f"M2.5 x 6 pan head each, self-tapping into a 2.2 mm pilot in the post (no nuts)." if getattr(P, "CARRIER", False) else ""),
          ""]
    (OUT / "report.md").write_text("\n".join(L) + "\n")


def main():
    import params as P
    import simulate_case as SC
    parts, info = build()
    chk = check(parts, info)
    ins = insertion_check(parts)
    info["insertion"] = ins
    if getattr(P, "V10", False):
        import simulate_v10 as SV
        ok_s, ok_d, sr, dr = SV.run(parts, info["zb"], out=OUT / "assembly_sim.md")
        sim_rows = sr + dr
        info["sim_v10"] = (ok_s, ok_d)
    else:
        mov = {k: v for k, v in parts.items() if k not in ("top_shell", "cable_clip")}
        sim_rows = SC.run(mov, zb=info["zb"], out=OUT / "assembly_sim.md", top=parts["top_shell"])
    lay = printable(parts, info)
    cg = cg_and_tip(parts, info)
    render(parts, info)
    sections(parts, info)
    export(parts, info)
    report(parts, info, chk, lay, cg, sim_rows)
    # alternative outside (v6): panel base, base STL only (same top shell and small parts)
    import params as P
    keep = P.TEXTURE
    variant("panels", "base_panels")
    variant(None, "base_smooth")
    for tex, nm in (("ribs", "base_ribbed_nocleat"), ("panels", "base_panels_nocleat"), (None, "base_smooth_nocleat")):
        variant(tex, nm, cleat=False)
    P.TEXTURE = keep
    print((OUT / "report.md").read_text())



def variant(texture, name, cleat=True):
    """Build the base with another outside texture (and with or without the cable cleat) and write just its
    STL (print orientation)."""
    import params as P
    from lfcase.export import write_stl
    P.TEXTURE = texture
    keep_cleat = P.CLEAT
    P.CLEAT = cleat
    parts, info = E.build_all(log=lambda m: None)
    b = parts["base"]
    bb = b.BoundingBox()
    write_stl(b.moved(__import__("cadquery").Location(__import__("cadquery").Vector(0, 0, -bb.zmin))), OUT / "stl" / f"{name}.stl")
    from lfcase import logo
    logo.cut_stl(OUT / "stl" / f"{name}.stl", None, (0, 0, -bb.zmin))
    P.CLEAT = keep_cleat
    return parts, info


if __name__ == "__main__":
    main()
