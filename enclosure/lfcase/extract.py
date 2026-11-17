"""Measure the PLAN.md section 4 values from the board STEP (board frame, mm).

Every value is returned with the STEP feature it was measured on, so the
generated block in params.py documents its own source. Nothing here is typed
in by hand from a datasheet or drawing.
"""
from __future__ import annotations

import math
from dataclasses import dataclass

import cadquery as cq


@dataclass
class Value:
    name: str
    value: object
    source: str
    unit: str = "mm"


def _bb(shape):
    b = shape.BoundingBox()
    return b.xmin, b.ymin, b.zmin, b.xmax, b.ymax, b.zmax


def _slab(shape, zmin=-50, zmax=50, x=None):
    """Intersect with a slab (Z range) or a thin X slice."""
    if x is not None:
        box = cq.Solid.makeBox(0.01, 200, 200, pnt=cq.Vector(x, -100, -100))
    else:
        box = cq.Solid.makeBox(200, 200, zmax - zmin, pnt=cq.Vector(-100, -100, zmin))
    return shape.intersect(box)


def _cyl_faces(shape):
    for f in shape.Faces():
        if f.geomType() == "CYLINDER":
            c = f._geomAdaptor().Cylinder()
            yield f, c.Radius(), c.Location(), c.Axis().Direction()


def _side_planes(shape):
    """Planar faces whose normal lies in the XY plane: (face, nx, ny, centroid)."""
    for f in shape.Faces():
        if f.geomType() != "PLANE":
            continue
        n = f.normalAt()
        if abs(n.z) < 0.05:
            yield f, n.x, n.y, f.Center()


def _fit_circle(pts):
    """Algebraic least-squares circle through 2-D points -> (cx, cy, r)."""
    import numpy as np

    p = np.asarray(pts, float)
    A = np.c_[2 * p[:, 0], 2 * p[:, 1], np.ones(len(p))]
    b = (p ** 2).sum(1)
    cx, cy, c = np.linalg.lstsq(A, b, rcond=None)[0]
    return cx, cy, math.sqrt(c + cx * cx + cy * cy)


def r3(v):
    if isinstance(v, (list, tuple)):
        return type(v)(r3(x) for x in v)
    return round(float(v), 3)


def extract(groups: dict[str, cq.Shape]) -> list[Value]:
    V: list[Value] = []
    add = lambda n, v, s, u="mm": V.append(Value(n, r3(v) if not isinstance(v, str) else v, s, u))

    # ---------------------------------------------------------------- PCB
    pcb = groups["pcb"]
    x0, y0, z0, x1, y1, z1 = _bb(pcb)
    rad = [math.hypot(v.X, v.Y) for v in pcb.Vertices()]
    add("PCB_D", 2 * max(rad), "STEP 'BOARD' solid: max vertex radius x2 (outline is a 64-gon)")
    add("PCB_THK", z1 - z0, "STEP 'BOARD' solid: Z extent")
    add("PCB_BACK_Z", z0, "STEP 'BOARD' solid: back face (defines board-frame Z=0)")
    add("PCB_FRONT_Z", z1, "STEP 'BOARD' solid: front face")

    # ------------------------------------------------------- display + glass
    disp = groups["display"]
    dx0, dy0, dz0, dx1, dy1, dz1 = _bb(disp)
    cyls = sorted(_cyl_faces(disp), key=lambda t: -t[1])
    glass_face, glass_r = cyls[0][0], cyls[0][1]
    glass_bottom = glass_face.BoundingBox().zmin
    add("GLASS_D", 2 * glass_r, "STEP 'H0175Y003AMT003': largest cylindrical face (cover glass edge)")
    add("GLASS_TOP_Z", dz1, "STEP 'H0175Y003AMT003': top face")
    add("GLASS_BOTTOM_Z", glass_bottom, "STEP 'H0175Y003AMT003': bottom of the glass edge cylinder")
    add("GLASS_THK", dz1 - glass_bottom, "GLASS_TOP_Z - GLASS_BOTTOM_Z")
    tor = [f for f in disp.Faces() if f.geomType() == "TORUS"]
    if tor:
        minor = tor[0]._geomAdaptor().Torus().MinorRadius()
        add("GLASS_EDGE_R", minor, "STEP 'H0175Y003AMT003': torus face on the glass top edge (edge round)")
        add("GLASS_FLAT_D", 2 * (glass_r - minor), "GLASS_D - 2*GLASS_EDGE_R (flat part of the glass face)")
    panel = [c for c in cyls if abs(c[1] - glass_r) > 0.5][0]
    pb = panel[0].BoundingBox()
    add("PANEL_D", 2 * panel[1], "STEP 'H0175Y003AMT003': AMOLED panel cylinder under the glass")
    add("PANEL_BOTTOM_Z", pb.zmin, "STEP 'H0175Y003AMT003': bottom of the panel cylinder")
    # Display flex folds that stick out past the panel below the glass
    under = _slab(disp, zmax=glass_bottom - 0.01)
    folds = []
    for s in under.Solids() if under.Solids() else [under]:
        b = s.BoundingBox()
        folds.append(b)
    ux0, uy0, uz0, ux1, uy1, uz1 = _bb(under)
    add("DISPLAY_FPC_FOLD_YMAX", uy1, "STEP 'H0175Y003AMT003' below the glass: +Y extent (display flex fold)")
    add("DISPLAY_FPC_FOLD_YMIN", uy0, "STEP 'H0175Y003AMT003' below the glass: -Y extent (display flex fold)")
    fold_r = 0.0
    for v in under.Vertices():
        fold_r = max(fold_r, math.hypot(v.X, v.Y))
    add("DISPLAY_UNDER_GLASS_RMAX", fold_r, "STEP 'H0175Y003AMT003' below the glass: max vertex radius (flex fold corners)")
    add("ACTIVE_AREA_D", "TODO", "NOT IN STEP: the display is one solid, no active-area face. Ask Paul.", "")

    pm = groups["pmma_frame"]
    px0, py0, pz0, px1, py1, pz1 = _bb(pm)
    add("PMMA_D", px1 - px0, "STEP 'ESP32-S3-TOUCH-AMOLED-1_75-PMMA': X extent (frame between PCB and panel)")
    add("PMMA_Z", (pz0, pz1), "STEP 'ESP32-S3-TOUCH-AMOLED-1_75-PMMA': Z range")
    add("PMMA_FLAT_Y", py0, "STEP 'ESP32-S3-TOUCH-AMOLED-1_75-PMMA': flat cut on the header side")

    # ------------------------------------------------------- overall stack
    allz = min(_bb(g)[2] for g in groups.values())
    tallest = min(groups, key=lambda g: _bb(groups[g])[2])
    add("BOARD_MIN_Z", allz, f"STEP: lowest point of any part ('{tallest}')")
    add("BOARD_TOTAL_THK", dz1 - allz, "GLASS_TOP_Z - BOARD_MIN_Z (glass face to tallest back part)")
    no_hdr = min(_bb(g)[2] for k, g in groups.items() if k != "header")
    add("BOARD_MIN_Z_NO_HEADER", no_hdr, "STEP: lowest point excluding the 8-pin header")

    # ------------------------------------------------ mounting holes/standoffs
    holes = sorted(
        {(round(l.X(), 3), round(l.Y(), 3), round(r, 3)) for f, r, l, d in _cyl_faces(pcb) if 1.0 < r < 1.2}
    )
    add("MOUNT_HOLES_XY", [(h[0], h[1]) for h in holes], "STEP 'BOARD': three R1.1 cylindrical holes")
    add("MOUNT_HOLE_D", 2 * holes[0][2], "STEP 'BOARD': hole cylinder radius x2")
    so = groups["standoffs"].Solids()[0]
    sx0, sy0, sz0, sx1, sy1, sz1 = _bb(so)
    add("STANDOFF_OD", sx1 - sx0, "STEP 'SMTSO-M2-3_5ET': X extent (M2 SMT standoff)")
    add("STANDOFF_Z", (sz0, sz1), "STEP 'SMTSO-M2-3_5ET': Z range (bottom = screw face)")

    # ------------------------------------------------------------ edge notches
    side = [(f, nx, ny, c) for f, nx, ny, c in _side_planes(pcb) if math.hypot(c.x, c.y) < 22.9]
    # cluster face centroids
    clusters: list[list] = []
    for item in side:
        c = item[3]
        for cl in clusters:
            if any(math.hypot(c.x - o[3].x, c.y - o[3].y) < 1.6 for o in cl):
                cl.append(item)
                break
        else:
            clusters.append([item])
    # merge transitively
    merged = True
    while merged:
        merged = False
        for i in range(len(clusters)):
            for j in range(i + 1, len(clusters)):
                if any(math.hypot(a[3].x - b[3].x, a[3].y - b[3].y) < 1.6 for a in clusters[i] for b in clusters[j]):
                    clusters[i] += clusters.pop(j)
                    merged = True
                    break
            if merged:
                break
    round_notches, chord_cuts = [], []
    for cl in clusters:
        pts = []
        for f, *_ in cl:
            pts += [(v.X, v.Y) for v in f.Vertices()]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        span = math.hypot(max(xs) - min(xs), max(ys) - min(ys))
        if span < 3.0:
            cx, cy, r = _fit_circle(pts)
            # only round bites in the outline (centre close to the rim)
            if math.hypot(cx, cy) > 21.5 and 0.5 < r < 1.5:
                round_notches.append((cx, cy, r))
        else:
            big = max(cl, key=lambda t: t[0].Area())
            f, nx, ny, c = big
            d = nx * c.x + ny * c.y
            if d > 15:  # straight chord on the outline
                ends = sorted(((v.X, v.Y) for v in f.Vertices()), key=lambda p: p[1])
                chord_cuts.append(dict(normal=(nx, ny), dist=d, ends=(ends[0], ends[-1])))
    round_notches.sort(key=lambda t: t[1])
    add("EDGE_NOTCHES", [(n[0], n[1]) for n in round_notches],
        "STEP 'BOARD': outline arcs fitted to circles (half-round locating notches)")
    add("EDGE_NOTCH_D", 2 * sum(n[2] for n in round_notches) / max(1, len(round_notches)),
        "STEP 'BOARD': fitted notch circle diameter (mean)")
    chord_cuts.sort(key=lambda c: c["normal"][1])
    for i, c in enumerate(chord_cuts, 1):
        ang = math.degrees(math.atan2(c["normal"][1], c["normal"][0]))
        add(f"BUTTON_FLAT{i}_ANGLE", ang, "STEP 'BOARD': outward normal of the straight outline cut, deg from +X", "deg")
        add(f"BUTTON_FLAT{i}_DIST", c["dist"], "STEP 'BOARD': distance of that cut from board centre")

    # ----------------------------------------------------------------- USB-C
    usb = groups["usb_c"]
    a = _bb(usb)
    ux0 = a[0]
    mouth = _slab(usb, x=ux0 + 0.3)      # just behind the chamfered lip
    mx0, my0, mz0, mx1, my1, mz1 = _bb(mouth)
    add("USB_DIR", "-X", "STEP 'H1 / 698B116-264-211': receptacle mouth faces -X")
    add("USB_MOUTH_X", ux0, "STEP 'H1': front face of the shell (min X of the receptacle)")
    add("USB_MOUTH_PROUD_OF_PCB", -ux0 - max(rad), "-(USB_MOUTH_X) - PCB_D/2")
    add("USB_SHELL_W", my1 - my0, "STEP 'H1': shell section 0.3 mm behind the mouth, Y extent")
    add("USB_SHELL_H", mz1 - mz0, "STEP 'H1': same section, Z extent")
    add("USB_CENTER_Y", (my0 + my1) / 2, "STEP 'H1': mouth section centre Y")
    add("USB_CENTER_Z", (mz0 + mz1) / 2, "STEP 'H1': mouth section centre Z")
    add("USB_SHELL_REAR_X", a[3], "STEP 'H1': rear end of the receptacle (backstop goes here)")
    a = _bb(usb)
    add("USB_ENVELOPE", ((a[0], a[1], a[2]), (a[3], a[4], a[5])), "STEP 'H1': bbox of all receptacle solids incl. legs/tabs")
    add("USB_GLASS_OVERHANG", glass_r + ux0, "GLASS_D/2 + USB_MOUTH_X: glass edge sticks out past the mouth by this")

    # --------------------------------------------------------------- buttons
    for key in ("key1", "key2"):
        k = groups[key]
        kx0, ky0, kz0, kx1, ky1, kz1 = _bb(k)
        planes = list(_side_planes(k))
        # outward-facing planes (normal roughly radial)
        outward = [p for p in planes if (p[1] * p[3].x + p[2] * p[3].y) / max(1e-6, math.hypot(p[3].x, p[3].y)) > 0.9]
        tip = max(outward, key=lambda p: p[1] * p[3].x + p[2] * p[3].y)
        nx, ny = tip[1], tip[2]
        dtip = nx * tip[3].x + ny * tip[3].y
        same = [p for p in planes if p[1] * nx + p[2] * ny > 0.999]
        body = max(same, key=lambda p: p[0].Area())
        dbody = nx * body[3].x + ny * body[3].y
        tb = tip[0].BoundingBox()
        K = key.upper()
        add(f"{K}_BBOX", ((kx0, ky0, kz0), (kx1, ky1, kz1)), f"STEP '{K}_ASM / SWITCH-TS24CA': bbox")
        add(f"{K}_PRESS_DIR", (-nx, -ny), f"STEP '{K}': inward normal of the plunger tip face (side-push, in board plane)", "")
        add(f"{K}_PRESS_ANGLE", math.degrees(math.atan2(ny, nx)), f"STEP '{K}': outward plunger normal, deg from +X", "deg")
        add(f"{K}_TIP_CENTER", (tip[3].x, tip[3].y, tip[3].z), f"STEP '{K}': centre of the plunger tip face")
        add(f"{K}_TIP_DIST", dtip, f"STEP '{K}': tip face distance from board centre along the press axis")
        add(f"{K}_PLUNGER_PROUD", dtip - dbody, f"STEP '{K}': tip face to switch body face")
        add(f"{K}_TIP_Z", (tb.zmin, tb.zmax), f"STEP '{K}': plunger tip Z range")
        add(f"{K}_TIP_W", math.hypot(tb.xmax - tb.xmin, tb.ymax - tb.ymin), f"STEP '{K}': plunger tip width in the board plane")
    add("KEY_TRAVEL", 0.25, "TODO verify by test print: not in the STEP (static model); 0.25 mm default for a side tact switch")
    add("PWR_IS", "TODO", "NOT IN STEP: no silkscreen/ref-des text for PWR vs BOOT. Ask Paul (KEY1 or KEY2).", "")

    # ------------------------------------------------------------ microphones
    for m in ("mic1", "mic2"):
        s = groups[m]
        b = _bb(s)
        corners = sorted({(round(v.X, 3), round(v.Y, 3)) for v in s.Vertices()})
        cx = sum(c[0] for c in corners) / len(corners)
        cy = sum(c[1] for c in corners) / len(corners)
        e = sorted(math.dist(corners[0], c) for c in corners[1:])
        M = m.upper()
        add(f"{M}_CENTER", (cx, cy), f"STEP '{M}_ASM': centre of the package footprint")
        add(f"{M}_SIZE", (e[0], e[1], b[5] - b[2]), f"STEP '{M}_ASM': package edge lengths and height")
        add(f"{M}_Z", (b[2], b[5]), f"STEP '{M}_ASM': Z range (on the PCB back)")
        add(f"{M}_ANGLE", math.degrees(math.atan2(corners[-1][1] - corners[0][1], corners[-1][0] - corners[0][0])),
            f"STEP '{M}_ASM': footprint rotation (diag. corner to corner)", "deg")
    add("MIC_PORT_SIDE", "TODO", "NOT IN STEP: mic is a plain box and the PCB has no hole under it. Ask Paul.", "")

    # ------------------------------------------------------- other connectors
    for g, label in (("j1", "J1 MX1.25 2P vertical"), ("h3", "H3 MX1.25 2P vertical"),
                     ("microsd", "microSD socket (front side, under display)"), ("header", "8-pin 2.54 female header"),
                     ("display_fpc", "display FPC")):
        b = _bb(groups[g])
        add(f"{g.upper()}_BBOX", ((b[0], b[1], b[2]), (b[3], b[4], b[5])), f"STEP: {label} bbox")
    add("BATTERY_CONNECTOR", "TODO", "NOT IN STEP: J1 and H3 are identical MX1.25 parts. Ask Paul which is BAT.", "")
    hdr_holes = sorted({(round(l.X(), 3), round(l.Y(), 3)) for f, r, l, d in _cyl_faces(pcb) if 0.4 < r < 0.5})
    add("HEADER_PITCH", (hdr_holes[-1][0] - hdr_holes[0][0]) / (len(hdr_holes) - 1), "STEP 'BOARD': spacing of the 8 header holes")
    return V
