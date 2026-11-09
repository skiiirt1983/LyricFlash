"""Phase 1 fit-test coupons. Every coupon is built in its PRINT orientation
(bed = Z 0) and, where it relates to the board, also offers a board-frame
placement for the interference check.

All numbers come from params.py.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

import cadquery as cq

import params as P


@dataclass
class Coupon:
    name: str                      # file stem
    title: str
    parts: dict                    # {part name: cq.Shape} in print orientation
    notes: list = field(default_factory=list)
    board_check: object = None     # callable -> {part: shape in board frame} or None
    checks: dict = field(default_factory=dict)  # extra numbers for the checklist
    sections: list = field(default_factory=list)  # [(stem, cut angle deg, target xyz, zoom, labels)] assembled renders


def deboss(wp_solid: cq.Shape, text: str, at, size=None, depth=None, face_z=None, rot=0.0):
    """Cut text into the top face at Z = face_z (default: shape top)."""
    if not text:
        return wp_solid
    size = size or P.COUPON_LABEL_SIZE
    depth = depth or P.COUPON_LABEL_DEPTH
    bb = wp_solid.BoundingBox()
    z = bb.zmax if face_z is None else face_z
    t = (cq.Workplane("XY").workplane(offset=z - depth)
         .center(*at).transformed(rotate=(0, 0, rot))
         .text(text, size, depth + 0.01, kind="bold", halign="center", valign="center"))
    return wp_solid.cut(t.val())


# --------------------------------------------------------------- battery
def battery_tray() -> Coupon:
    L, W, _ = P.BATTERY_POCKET
    lead = P.BATTERY_LEAD_SPACE
    wall = P.WALL_MIN
    fl = P.COUPON_FLOOR
    h = fl + P.BATTERY_COUPON_WALL_H
    inner_L = L + lead
    ox, oy = inner_L + 2 * wall, W + 2 * wall
    r = P.BATTERY_POCKET_CORNER_R
    body = cq.Workplane("XY").box(ox, oy, h, centered=(True, True, False)).edges("|Z").fillet(r + wall)
    # pocket: cell area + lead space, one rounded rectangle (lead space is full width)
    pocket = (cq.Workplane("XY").workplane(offset=fl).rect(inner_L, W).extrude(h)
              .edges("|Z").fillet(r))
    body = body.cut(pocket)
    # step marking the cell end / lead-space boundary: two small ribs at the walls (0.6 high)
    x_cell_end = -inner_L / 2 + L
    for s in (-1, 1):
        rib = cq.Workplane("XY").workplane(offset=fl).center(x_cell_end + 0.6, s * (W / 2 - 1.0)).rect(1.2, 2.0).extrude(0.6)
        body = body.union(rib)
    # finger notch (long side, at the cell middle) and wire slot (lead end)
    notch = cq.Workplane("XY").workplane(offset=fl).center(-inner_L / 2 + L / 2, -(W / 2 + wall / 2)).rect(P.BATTERY_FINGER_NOTCH_W, wall * 3).extrude(h)
    body = body.cut(notch)
    slot = cq.Workplane("XY").workplane(offset=fl).center(inner_L / 2 + wall / 2, 0).rect(wall * 3, P.BATTERY_WIRE_SLOT_W).extrude(h)
    body = body.cut(slot)
    # thickness gauge: C-shape on the outside of the far end, gap = pocket thickness
    T = P.BATTERY_POCKET[2]
    g_len = P.BATTERY_GAUGE_LEN
    gx0 = -ox / 2
    gauge = (cq.Workplane("XY").center(gx0 - g_len / 2 - wall + 0.01, 0)
             .rect(g_len + 2 * wall, T + 2 * wall).extrude(h)
             .cut(cq.Workplane("XY").center(gx0 - g_len / 2 - wall - 0.01, 0).rect(g_len + 0.02, T).extrude(h)))
    # leave the gauge's outer (open) end open: cut the far wall
    gauge = gauge.cut(cq.Workplane("XY").center(gx0 - g_len - 1.5 * wall, 0).rect(wall * 1.2, T).extrude(h))
    body = body.union(gauge)
    shape = body.val()
    shape = deboss(shape, f"{L:.1f}x{W:.1f}", (-inner_L / 2 + L / 2, 0), face_z=fl, size=5)
    shape = deboss(shape, "LEADS", (inner_L / 2 - lead / 2, 0), face_z=fl, size=3, rot=90)
    return Coupon(
        "C3_battery_pocket", "Battery pocket tray", {"battery_tray": shape},
        notes=[f"Pocket {L} x {W} (cell {P.BATTERY_L} x {P.BATTERY_W}), lead space {lead} at one 34.5 end, "
               f"walls {P.BATTERY_COUPON_WALL_H} high. C-gauge on the end: {T} gap (cell {P.BATTERY_T} + swelling)."],
    )


# --------------------------------------------------------------- switch
def switch_pocket() -> Coupon:
    bl, bw, bh = P.SWITCH_BODY
    pl, pw = P.SWITCH_POCKET
    sl, sw = P.SWITCH_KNOB_SLOT
    wall = P.WALL_MIN
    cav_h = bh + P.SWITCH_POCKET_H_CLEAR
    pin = P.SWITCH_PIN_SPACE
    cells = []
    pitch = pl + 2 * wall + 2.0
    n = len(P.SWITCH_KNOB_HEIGHTS)
    for i, k in enumerate(P.SWITCH_KNOB_HEIGHTS):
        plate = k + P.SWITCH_KNOB_TIP_RECESS            # outer surface to body top
        H = plate + cav_h + pin                          # print height (outer face on bed)
        ox, oy = pl + 2 * wall, pw + wall + 4.0          # open on -Y side; 4 mm label land on +Y
        cx = (i - (n - 1) / 2) * pitch
        blk = cq.Workplane("XY").center(cx, 0).rect(ox, oy).extrude(H)
        y_closed = oy / 2 - 4.0                          # closed wall inner face is at +Y
        y_pocket_c = y_closed - wall - pw / 2
        # knob slot through the plate (outer face = bed)
        blk = blk.cut(cq.Workplane("XY").center(cx, y_pocket_c).rect(sl, sw).extrude(plate))
        # knob entry channel from the slot to the open side, leaving a skin on the outer face
        ch_y0 = -oy / 2 - 0.1
        ch_y1 = y_pocket_c
        blk = blk.cut(cq.Workplane("XY").workplane(offset=P.SWITCH_CHANNEL_SKIN)
                      .center(cx, (ch_y0 + ch_y1) / 2).rect(P.SWITCH_KNOB_CHANNEL_W, ch_y1 - ch_y0).extrude(plate))
        # body cavity: open to -Y (slide in sideways)
        cav_y0, cav_y1 = -oy / 2 - 0.1, y_pocket_c + pw / 2
        blk = blk.cut(cq.Workplane("XY").workplane(offset=plate)
                      .center(cx, (cav_y0 + cav_y1) / 2).rect(pl, cav_y1 - cav_y0).extrude(cav_h + pin + 0.1))
        # lips under both body ends (pins/solder pass between them). 45 deg wedge
        # underneath so they print without support; the body end rests on the wedge.
        z0 = plate + cav_h - P.SWITCH_LIP
        for sgn in (-1, 1):
            xw = cx + sgn * pl / 2
            xi = xw - sgn * P.SWITCH_LIP
            prof = [(xw, z0), (xi, z0 + P.SWITCH_LIP), (xi, H), (xw, H)]
            lip = (cq.Workplane("XZ", origin=(0, cav_y0, 0)).polyline(prof).close()
                   .extrude(-(cav_y1 - cav_y0)))
            blk = blk.union(lip)
        s_ = blk.val()
        s_ = deboss(s_, f"K{k:g}", (cx, oy / 2 - 2.0), size=2.8)
        cells.append(s_)
    shape = cells[0]
    for c in cells[1:]:
        shape = shape.fuse(c)
    # tie strip so the three print as one piece
    tie = cq.Workplane("XY").center(0, (pw + wall + 4.0) / 2 - 1.0).rect(pitch * (n - 1) + 4, 2.0).extrude(1.2).val()
    shape = shape.fuse(tie).clean()
    shape = cq.Shape.cast(shape.wrapped)
    return Coupon(
        "C4_switch_pocket", "Switch pocket + knob slot", {"switch_pocket": shape},
        notes=[f"Three pockets for knob length {P.SWITCH_KNOB_HEIGHTS} mm (labels K3/K4/K5). "
               f"Pocket {pl} x {pw}, body clearance +{P.SWITCH_POCKET_H_CLEAR} in height, {pin} mm under the body for pins/solder. "
               f"Knob slot {sl} x {sw}; knob tip should sit {P.SWITCH_KNOB_TIP_RECESS} below the outer face. "
               "Switch slides in sideways from the open side (knob through the entry channel), lips under its ends hold it."],
    )


# --------------------------------------------------------------- snap latches
def snap_strain(t=None, y=None, L=None):
    t = t or P.SNAP_BEAM_T
    y = y or P.SNAP_HOOK
    L = L or P.SNAP_BEAM_L
    return 1.5 * t * y / (L * L)


def _snap_pair(c: float, x_off: float, c_ax: float | None = None, label: str | None = None):
    """c = lateral clearance (spigot); c_ax = axial play at the catch (negative = preload)."""
    c_ax = c if c_ax is None else c_ax
    label = label or f"{c:.2f}"
    t, hk, L = P.SNAP_BEAM_T, P.SNAP_HOOK, P.SNAP_BEAM_L
    E = P.SNAP_EXTRUDE
    lead = hk / math.tan(math.radians(P.SNAP_LEADIN_DEG))
    ret = hk / math.tan(math.radians(P.SNAP_RETAIN_DEG))
    flat = 0.3
    # ---- male (profile in XY, beam pointing -Y, extruded in Z)
    spig_w, spig_h = 8.0, 1.5
    y_root = -spig_h
    y_tip = y_root - L
    y_ret_out = y_tip + lead + flat            # hook outer corner on the retention side
    y_ret_in = y_ret_out + ret
    beam = [(-t / 2, y_root + 0.01), (-t / 2, y_tip), (t / 2, y_tip), (t / 2 + hk, y_tip + lead),
            (t / 2 + hk, y_ret_out), (t / 2, y_ret_in), (t / 2, y_root + 0.01)]
    male = (cq.Workplane("XY").polyline(beam).close().extrude(E)
            .union(cq.Workplane("XY").center(0, -spig_h / 2).rect(spig_w, spig_h).extrude(E))
            .union(cq.Workplane("XY").center(0, 2.0).rect(14.0, 4.0).extrude(E))
            .edges("|Z and >Y").fillet(1.0))
    # root fillets where the beam meets the spigot
    # ---- female
    fw, fh = 16.0, L + spig_h + 3.5
    fem = cq.Workplane("XY").center(0, -fh / 2).rect(fw, fh).extrude(E)
    pocket = cq.Workplane("XY").center(0, -(spig_h + 0.3) / 2 + 0.01).rect(spig_w + 2 * c, spig_h + 0.3).extrude(E)
    y_lip_bot_out = y_ret_out + c_ax               # catch face, offset by the axial play
    y_lip_bot_in = y_ret_in + c_ax
    x_lip = t / 2 + c
    x_recess = t / 2 + hk + c
    x_far = -(t / 2 + hk + c + 0.2)
    y_floor = y_tip - 1.0
    slot = [(x_far, -spig_h - 0.29), (x_lip, -spig_h - 0.29), (x_lip, y_lip_bot_in - (x_lip - t / 2) * (ret / hk)),
            (x_recess, y_lip_bot_out), (x_recess, y_floor), (x_far, y_floor)]
    fem = fem.cut(pocket).cut(cq.Workplane("XY").polyline(slot).close().extrude(E))
    male_s = deboss(male.val(), label, (0, 2.0), size=2.6)
    fem_s = deboss(fem.val(), label, (-4.6, -fh + 3.0), size=2.6)
    fem_s = deboss(fem_s, label, (4.6, -fh + 3.0), size=2.6)
    return male_s.moved(cq.Location(cq.Vector(x_off, 16.0, 0))), fem_s.moved(cq.Location(cq.Vector(x_off, -2.0, 0)))


def snap_latches() -> Coupon:
    parts = {}
    for i, ca in enumerate(P.SNAP_AXIAL_VARIANTS):
        lab = {-0.10: "P1", 0.0: "P0", 0.10: "L1"}.get(round(ca, 2), f"{ca:+.2f}")
        m, f = _snap_pair(P.SNAP_LATERAL_CLEAR, (i - 1) * 20.0, c_ax=ca, label=lab)
        parts[f"snap_male_{lab}"] = m
        parts[f"snap_female_{lab}"] = f
    eps = snap_strain()
    return Coupon(
        "C7_snap_latches", "Snap-fit latch pairs", parts,
        notes=[f"Beam {P.SNAP_BEAM_L} long x {P.SNAP_BEAM_T} thick, hook {P.SNAP_HOOK}, lead-in {P.SNAP_LEADIN_DEG:g} deg, "
               f"retention face {P.SNAP_RETAIN_DEG:g} deg. v2: side fit {P.SNAP_LATERAL_CLEAR} for all; axial play at the catch "
               "P1 = 0.10 preload (pulls tight), P0 = zero play, L1 = 0.10 play.",
               "Printed as 2-D profiles standing on the bed, so the beams bend along the layers (strong direction)."],
        checks={"beam_strain": eps},
    )


# --------------------------------------------------------------- speaker
def speaker_pocket() -> Coupon:
    L, W, T = P.SPK_L, P.SPK_W, P.SPK_T
    c, wall = P.SPK_CLEAR, P.WALL_MIN
    pl, pw = L + 2 * c, W + 2 * c
    depth = T + P.SPK_DEPTH_CLEAR
    ox, oy = pl + 2 * wall, pw + 2 * wall
    H = wall + depth
    r = P.SPK_CORNER_R
    body = cq.Workplane("XY").rect(ox, oy).extrude(H).edges("|Z").fillet(r + wall)
    body = body.cut(cq.Workplane("XY").workplane(offset=wall).rect(pl, pw).extrude(H).edges("|Z").fillet(r + c))
    # front seal crush rib on the plate, under the speaker's front frame
    si = P.SPK_SEAL_INSET
    rw = P.SPK_SEAL_RIB_W
    rib = (cq.Workplane("XY").workplane(offset=wall).rect(L - 2 * si + rw, W - 2 * si + rw).extrude(P.SPK_SEAL_RIB_H)
           .edges("|Z").fillet(max(0.3, r - si + rw / 2))
           .cut(cq.Workplane("XY").workplane(offset=wall - 0.1).rect(L - 2 * si - rw, W - 2 * si - rw).extrude(1)
                .edges("|Z").fillet(max(0.2, r - si - rw / 2))))
    body = body.union(rib)
    # grille: hex pattern of holes inside the seal
    gm = P.SPK_GRILLE_MARGIN
    gx, gy = L / 2 - gm, W / 2 - gm
    pts = []
    p = P.SPK_GRILLE_PITCH
    j = 0
    y = -gy
    while y <= gy + 1e-6:
        x0 = -gx + (p / 2 if j % 2 else 0)
        x = x0
        while x <= gx + 1e-6:
            # stadium-shaped grille area (rounded ends like the cone)
            rr = gy
            if abs(x) <= gx - rr or math.hypot(abs(x) - (gx - rr), y) <= rr:
                pts.append((x, y))
            x += p
        y += p * math.sqrt(3) / 2
        j += 1
    body = body.cut(cq.Workplane("XY").pushPoints(pts).circle(P.SPK_GRILLE_HOLE_D / 2).extrude(wall + 0.1))
    # side grip ribs (two per long wall), vertical, interference SPK_GRIP_RIB
    for sx in (-L / 4, L / 4):
        for sy in (-1, 1):
            body = body.union(cq.Workplane("XY").workplane(offset=wall - 0.01)
                              .center(sx, sy * (pw / 2 - (c + P.SPK_GRIP_RIB) / 2 + 0.01))
                              .rect(0.8, c + P.SPK_GRIP_RIB).extrude(depth + 0.01))
    # lead slot in one short end, from the back
    body = body.cut(cq.Workplane("XY").workplane(offset=wall + 2.0).center(pl / 2 + wall / 2, 0)
                    .rect(wall * 3, P.SPK_LEAD_SLOT_W).extrude(H))
    shape = body.val()
    shape = deboss(shape, "C8", (-pl / 2 - wall / 2, 0), size=1.2, rot=90)
    return Coupon(
        "C8_speaker_pocket", "Speaker pocket + grille", {"speaker_pocket": shape},
        notes=[f"Speaker {L} x {W} x {T} (Paul's listing image). Pocket +{c}/side, {P.SPK_DEPTH_CLEAR} extra depth, "
               f"corner R{P.SPK_CORNER_R} (TODO, estimated). Front seal rib {P.SPK_SEAL_RIB_H} high on the speaker frame, "
               f"grille {len(pts)} x D{P.SPK_GRILLE_HOLE_D}. Four side ribs grip it ({P.SPK_GRIP_RIB} interference). "
               "Printed grille-down; holes are vertical, no support."],
    )


# --------------------------------------------------------------- C7 v3 (guided)
def id_notches(shape: cq.Shape, n: int, x_edge: float, y_c: float, side: int = -1):
    """Cut n countable notches into a vertical edge face at x = x_edge (side -1: notch goes +X)."""
    for i in range(n):
        y = y_c + (i - (n - 1) / 2) * P.ID_NOTCH_PITCH
        x0 = x_edge if side < 0 else x_edge - P.ID_NOTCH_D
        shape = shape.cut(cq.Solid.makeBox(P.ID_NOTCH_D, P.ID_NOTCH_W, 50, pnt=cq.Vector(x0 - (0.01 if side < 0 else 0), y - P.ID_NOTCH_W / 2, -10)))
    return shape


def _snap_pair_v3(c_ax: float, label: str, retain_deg: float | None = None):
    """Guided snap test. Male: base bar + two guide posts + beam with hook. Female: block with two
    guide channels and the beam slot; the slot only gives the beam room on the side it bends to."""
    t, hk, L = P.SNAP_BEAM_T, P.SNAP_HOOK, P.SNAP_BEAM_L
    E = P.SNAP_EXTRUDE
    gd, gw, gc = P.SNAP_GUIDE_DEPTH, P.SNAP_GUIDE_W, P.SNAP_GUIDE_CLEAR
    lead = hk / math.tan(math.radians(P.SNAP_LEADIN_DEG))
    ret = hk / math.tan(math.radians(retain_deg or P.SNAP_RETAIN_DEG))
    gx = 5.5                                     # guide post centres at +/- gx
    y_tip = -L
    y_ret_out = y_tip + lead + 0.3
    y_ret_in = y_ret_out + ret
    beam = [(-t / 2, 0.01), (-t / 2, y_tip), (t / 2, y_tip), (t / 2 + hk, y_tip + lead),
            (t / 2 + hk, y_ret_out), (t / 2, y_ret_in), (t / 2, 0.01)]
    male = cq.Workplane("XY").polyline(beam).close().extrude(E)
    for sx in (-gx, gx):
        male = male.union(cq.Workplane("XY").center(sx, -gd / 2).rect(gw, gd).extrude(E))
    male = male.union(cq.Workplane("XY").center(0, 2.5).rect(16.0, 5.0).extrude(E))
    # female
    fw, fh = 18.0, L + 4.0
    fem = cq.Workplane("XY").center(0, -fh / 2).rect(fw, fh).extrude(E)
    for sx in (-gx, gx):
        fem = fem.cut(cq.Workplane("XY").center(sx, -(gd + 0.4) / 2 + 0.01).rect(gw + 2 * gc, gd + 0.4).extrude(E))
    x_lip = t / 2 + gc                           # catch side: lip just clears the beam
    x_recess = t / 2 + hk + 0.2
    x_far = -(t / 2 + gc)                        # free side: no room, the beam can't drift away
    y_floor = y_tip - 1.0
    y_lip_in = y_ret_in + c_ax - (x_lip - t / 2) * (ret / hk)
    y_lip_out = y_ret_out + c_ax - 0.2 * (ret / hk)
    slot = [(x_far, 0.1), (x_lip + hk + 0.3, 0.1), (x_lip + hk + 0.3, -1.0), (x_lip, -2.0),
            (x_lip, y_lip_in), (x_recess, y_lip_out), (x_recess, y_floor), (x_far, y_floor)]
    fem = fem.cut(cq.Workplane("XY").polyline(slot).close().extrude(E))
    sz = P.LABEL_MIN_SIZE
    male_s = deboss(male.val(), label, (0, 2.5), size=sz)
    fem_s = deboss(fem.val(), label, (-5.5, -fh + 2.6), size=sz)
    return male_s, fem_s


def snap_latches_v3() -> Coupon:
    parts = {}
    for i, (ca, lab) in enumerate(zip(P.SNAP_AXIAL_VARIANTS, ("P1", "P0", "L1"))):
        m, f = _snap_pair_v3(ca, lab)
        parts[f"snap3_male_{lab}"] = m.moved(cq.Location(cq.Vector((i - 1) * 22.0, 16.0, 0)))
        parts[f"snap3_female_{lab}"] = f.moved(cq.Location(cq.Vector((i - 1) * 22.0, -2.0, 0)))
    eps = 1.5 * P.SNAP_BEAM_T * P.SNAP_HOOK / P.SNAP_BEAM_L ** 2
    return Coupon("C7v3_snap_latches", "Guided snap latch pairs", parts,
                  notes=[f"Guide posts {P.SNAP_GUIDE_DEPTH} deep stop the rocking; hook {P.SNAP_HOOK}, catch {P.SNAP_RETAIN_DEG:g} deg; "
                         "P1 = 0.1 preload, P0 = zero play, L1 = 0.1 play."],
                  checks={"beam_strain": eps})


def snap_latches_v4() -> Coupon:
    """Guided pairs with 0.3 guide clearance, 0.8 hook, L1 play; catch angle 45/55/65 deg.
    Pairs side by side; 1/2/3 notches on the left edge of both halves."""
    parts = {}
    for i, ang in enumerate(P.SNAP_RETAIN_VARIANTS):
        m, f = _snap_pair_v3(P.SNAP_AXIAL_PLAY_V4, "", retain_deg=ang)
        m = id_notches(m, i + 1, -8.0, 2.5)          # male bar spans x -8..8
        f = id_notches(f, i + 1, -9.0, -7.0)         # female block spans x -9..9
        x = i * 42.0
        parts[f"snap4_{i+1}notch_{int(ang)}deg_hook"] = m.moved(cq.Location(cq.Vector(x, 0, 0)))
        parts[f"snap4_{i+1}notch_{int(ang)}deg_socket"] = f.moved(cq.Location(cq.Vector(x + 20.0, 0, 0)))
    return Coupon("C7v4_snap_latches", "Guided snap latches, 3 release strengths", parts,
                  notes=["1 notch = 45 deg catch (easiest to pull apart), 2 = 55 deg, 3 = 65 deg (firmest). "
                         f"Guide clearance {P.SNAP_GUIDE_CLEAR}/side, hook {P.SNAP_HOOK}, play {P.SNAP_AXIAL_PLAY_V4}."])
