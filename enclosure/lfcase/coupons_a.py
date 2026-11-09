"""Option A coupons (lip bezel, board loads from behind, base tray clamps it).

Everything is modelled in the BOARD FRAME first (so it can be checked against
the STEP board), then turned to its print orientation for export:
  * top shell parts print lip-down (front face on the plate, clean bezel)
  * base tray parts print floor-down (USB web is then a horizontal-layer wall
    bridged over the opening, see report)
"""
from __future__ import annotations

import math

import cadquery as cq

import params as P
from lfcase.coupons import Coupon, deboss

GR = P.GLASS_D / 2                                   # 24.48
BORE_R = GR + P.BOARD_SEAT_DIAM_CLEAR / 2            # 24.63
LIP_R = GR - P.LIP_OVERLAP                           # 23.48
LIP_TOP = P.GLASS_TOP_Z + P.LIP_T                    # 7.90
R_OUT = P.SHELL_R_OUT
BASE_IN = P.PCB_D / 2 + P.BASE_INNER_CLEAR           # 23.31
SPIG_OUT = BORE_R - P.SPIGOT_CLEAR                   # 24.33
FLOOR_TOP = P.BOARD_MIN_Z - P.FLOOR_CLEAR            # -6.1
FLOOR_BOT = FLOOR_TOP - P.FLOOR_T
SEAT_Z = P.STANDOFF_Z[0]                             # -3.5, standoff screw face
CUP_ID = P.STANDOFF_OD + P.CUP_ID_CLEAR
CUP_TOP = SEAT_Z + P.CUP_DEPTH

# -------------------------------------------------------------- helpers


def _wp(z=0.0):
    return cq.Workplane("XY").workplane(offset=z)


def cyl(r, z0, z1, x=0.0, y=0.0):
    return cq.Solid.makeCylinder(r, z1 - z0, pnt=cq.Vector(x, y, z0))


def ring(r0, r1, z0, z1):
    return cyl(r1, z0, z1).cut(cyl(r0, z0 - 1, z1 + 1))


def wedge(a0, a1, z0, z1, R=60.0):
    """Prism covering angles a0..a1 (deg, CCW from +X)."""
    n = max(4, int(abs(a1 - a0) / 3) + 1)
    pts = [(0, 0)] + [(R * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
                       R * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
    return _wp(z0).polyline(pts).close().extrude(z1 - z0).val()


def rz_profile_solid(pts_rz, a0, a1):
    """Revolve an (r, z) polygon about Z between angles a0..a1 (deg)."""
    wp = cq.Workplane("XZ").polyline(pts_rz).close()
    s = wp.revolve(a1 - a0, (0, 0, 0), (0, 1, 0)).val()   # XZ plane: revolve about its local Y = global Z
    return s.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), a0)


def at_angle(shape, deg):
    return shape.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), deg)


def flip_for_print(shape):
    """Top-shell parts: lip down on the plate (board +Z -> print -Z)."""
    return shape.rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), 180)


# -------------------------------------------------------------- latches


def _latch_beam_local():
    """Beam + hook on the top shell, centred on +X (angle 0)."""
    root = LIP_TOP - 0.5
    tip = root - P.LATCH_L
    r_in, r_out = R_OUT - P.LATCH_T, R_OUT - 0.02   # 0.02 under the skin: avoids coincident faces in the boolean
    h = P.LATCH_HOOK
    lead = h / math.tan(math.radians(30))
    hw = P.LATCH_W / 2
    ang = math.degrees(hw / r_in)
    beam = rz_profile_solid([(r_in, tip), (r_out, tip), (r_out, root + 0.3), (r_in, root + 0.3)], -ang, ang)
    hook_rz = [(r_in + 0.01, tip), (r_in - h, tip + lead), (r_in - h, tip + lead + 0.2),
               (r_in + 0.01, tip + lead + 0.2 + h)]
    hook = rz_profile_solid(hook_rz, -ang, ang)
    return beam.fuse(hook), tip, lead


def _latch_channel_local():
    """Cut in the top shell around the beam (U channel, open towards the seam)."""
    root = LIP_TOP - 0.5
    r_in = R_OUT - P.LATCH_T - P.LATCH_HOOK          # gap behind the beam = hook depth
    hw = P.LATCH_W / 2 + P.LATCH_SIDE_GAP
    ang = math.degrees(hw / r_in)
    return rz_profile_solid([(r_in, P.SEAM_Z - 5), (R_OUT + 1, P.SEAM_Z - 5), (R_OUT + 1, root), (r_in, root)], -ang, ang)


def _latch_base_cut_local():
    """Channel the beam slides down + pocket the hook drops into (base side)."""
    _, tip, lead = _latch_beam_local()
    h, c = P.LATCH_HOOK, P.LATCH_CLEAR
    r_beam_in = R_OUT - P.LATCH_T
    r_ch = r_beam_in - 0.1                           # channel floor the hook rides on
    r_pk = r_beam_in - h - 0.2                       # pocket floor
    hw = P.LATCH_W / 2 + 0.4
    ang = math.degrees(hw / r_pk)
    chan = rz_profile_solid([(r_ch, tip - 0.5), (R_OUT + 1, tip - 0.5), (R_OUT + 1, P.SEAM_Z + 5), (r_ch, P.SEAM_Z + 5)], -ang, ang)
    # pocket ceiling parallel to the 45 deg retention face, offset by c
    z_ret_in = tip + lead + 0.2                      # retention face at r = r_beam_in - h
    ceil = lambda r: z_ret_in + c + (r - (r_beam_in - h))
    pocket = rz_profile_solid([(r_pk, tip - 0.5), (r_ch + 0.01, tip - 0.5), (r_ch + 0.01, ceil(r_ch + 0.01)), (r_pk, ceil(r_pk))], -ang, ang)
    return chan.fuse(pocket)


def latch_strain():
    defl = P.LATCH_HOOK - 0.1
    return 1.5 * P.LATCH_T * defl / P.LATCH_L ** 2


# -------------------------------------------------------------- shells


def solidify(shape):
    shape = shape.clean()
    if not shape.isValid():
        shape = shape.fix()
    if not shape.isValid():
        raise RuntimeError("could not build a valid solid")
    return shape


def top_shell_slice(a0, a1, latch_angles=()):
    body = cyl(R_OUT, P.SEAM_Z, LIP_TOP).cut(cyl(BORE_R, P.SEAM_Z - 1, P.GLASS_TOP_Z)).cut(cyl(LIP_R, P.GLASS_TOP_Z - 1, LIP_TOP + 1))
    body = body.intersect(wedge(a0, a1, P.SEAM_Z - 20, LIP_TOP + 1))
    beam, *_ = _latch_beam_local()
    for la in latch_angles:
        b = at_angle(beam, la)
        body = body.cut(at_angle(_latch_channel_local(), la).cut(b)).fuse(b)
    return solidify(body)


def plug_keepout():
    """J1 (battery) and H3 (speaker) sockets with plugs + wires in: XY grown, from the PCB down
    through the floor."""
    out = None
    g = P.PLUG_KEEPOUT_GROW
    for bb in (P.J1_BBOX, P.H3_BBOX):
        (x0, y0, _), (x1, y1, _) = bb
        b = cq.Solid.makeBox(x1 - x0 + 2 * g, y1 - y0 + 2 * g, 30, pnt=cq.Vector(x0 - g, y0 - g, FLOOR_BOT - 5))
        out = b if out is None else out.fuse(b)
    return out


def fold_reliefs(z0, z1):
    """Notches in the top-shell bore for the display flex folds."""
    out = None
    for a in P.FOLD_RELIEF_ANGLES:
        hw = P.FOLD_RELIEF_W / 2
        n = cq.Solid.makeBox(P.FOLD_RELIEF_DEPTH + 2.0, 2 * hw, z1 - z0, pnt=cq.Vector(BORE_R - 2.0, -hw, z0))
        n = at_angle(n, a)
        out = n if out is None else out.fuse(n)
    return out


def base_sector(a0, a1, cups, latch_angles=(), inner_r=BASE_IN):
    w = wedge(a0, a1, FLOOR_BOT - 1, P.SEAM_Z + P.SPIGOT_H + 1)
    floor = cyl(R_OUT, FLOOR_BOT, FLOOR_TOP)
    wall = ring(inner_r, R_OUT, FLOOR_TOP - 0.01, P.SEAM_Z)
    spig = ring(inner_r, SPIG_OUT, P.SEAM_Z - 0.01, P.SEAM_Z + P.SPIGOT_H)
    body = floor.fuse(wall).fuse(spig).intersect(w)
    for x, y in cups:
        ped = cyl(CUP_ID / 2 + P.CUP_WALL, FLOOR_BOT, CUP_TOP, x, y).cut(cyl(CUP_ID / 2, SEAT_Z, CUP_TOP + 1, x, y))
        body = body.fuse(ped)
    for la in latch_angles:
        body = body.cut(at_angle(_latch_base_cut_local(), la))
    body = body.cut(plug_keepout())
    return body.clean()


# -------------------------------------------------------------- C1 seat ring


def c1_display_seat() -> Coupon:
    z0 = P.GLASS_BOTTOM_Z - 1.8
    ring_ = cyl(R_OUT, z0, LIP_TOP).cut(cyl(BORE_R, z0 - 1, P.GLASS_TOP_Z)).cut(cyl(LIP_R, z0, LIP_TOP + 1))
    ring_ = ring_.cut(fold_reliefs(z0 - 1, P.GLASS_BOTTOM_Z - 0.05))      # round 2: flex folds
    # base floor locator: floor disc with three standoff cups and viewing windows
    loc = cyl(BASE_IN, FLOOR_BOT, FLOOR_TOP)
    for i, a in enumerate((30, 150, 270)):
        loc = loc.cut(cyl(5.5, FLOOR_BOT - 1, FLOOR_TOP + 1, 9 * math.cos(math.radians(a)), 9 * math.sin(math.radians(a))))
    for x, y in P.MOUNT_HOLES_XY:
        loc = loc.fuse(cyl(CUP_ID / 2 + P.CUP_WALL, FLOOR_BOT, CUP_TOP, x, y).cut(cyl(CUP_ID / 2, SEAT_Z, CUP_TOP + 1, x, y)))
    loc = loc.cut(plug_keepout())                                         # round 2: plugs made it rock
    loc = deboss(loc, "C1v2", (0, -2), size=3.0, face_z=FLOOR_TOP)
    return Coupon(
        "C1_display_seat", "Display seat ring + base locator",
        {"seat_ring": ring_, "base_locator": loc},
        board_check=lambda: {"seat_ring": ring_, "base_locator": loc},
        sections=[("section_edge", 90.0, (0, 24.0, 2.0), 3.2,
                   [((0, LIP_R + 0.5, LIP_TOP), "lip 1.0 over glass", (-260, -120)),
                    ((0, BORE_R, 5.0), "bore D49.26", (260, -60)),
                    ((0, 20.5, SEAT_Z), "cup seats on standoff", (260, 120))]),
                  ("section_full", 0.0, None, 1.0, [])],
        notes=[f"Seat ring: bore D{2*BORE_R:.2f} (glass D{P.GLASS_D} +{P.BOARD_SEAT_DIAM_CLEAR}), lip {P.LIP_OVERLAP} over the glass edge "
               f"(opening D{2*LIP_R:.2f}), lip {P.LIP_T} thick so the rim stands {P.LIP_T} proud of the glass. Prints lip-down.",
               f"Base locator: floor with the three standoff cups (ID {CUP_ID:.1f} on the D{P.STANDOFF_OD} standoffs, seat at the standoff "
               "face). The cup pattern only fits one way round, so it also checks orientation."],
    )


# -------------------------------------------------------------- C2 USB-C


USB_A0, USB_A1 = 118.0, 242.0
USB_LATCHES = (128.0, 232.0)
USB_CUPS = [xy for xy in P.MOUNT_HOLES_XY if xy[0] < -5]          # (-13.75, -14.70)


def _stadium(w, h, r):
    return cq.Sketch().rect(w, h).vertices().fillet(r)


def usb_geometry():
    W, H = P.USB_SHELL_W, P.USB_SHELL_H
    yc, zc = P.USB_CENTER_Y, P.USB_CENTER_Z
    face_x = P.USB_MOUTH_X + P.USB_PROUD_TARGET
    return dict(W=W, H=H, yc=yc, zc=zc, face_x=face_x,
                z_bot=zc - H / 2, z_top=zc + H / 2, r_shell=1.28)


def _usb_port_block(base, relief_only=False):
    g = usb_geometry()
    W, H, yc, zc, fx = g["W"], g["H"], g["yc"], g["zc"], g["face_x"]
    c = P.USB_OPENING_CLEAR
    rear = P.USB_SHELL_REAR_X
    # 1. flat port face: everything outside X = face_x goes
    base = base.cut(cq.Solid.makeBox(20, 80, 40, pnt=cq.Vector(fx - 20, -40, FLOOR_BOT - 5)))
    # 2. web / contact: inside the flat-face region the base comes in to the PCB outline (zero gap)
    hw = P.USB_FACE_HALF_W
    contact = (ring(P.USB_PCB_CONTACT_R, BASE_IN + 0.01, FLOOR_TOP - 0.01, P.SEAM_Z + P.SPIGOT_H)
               .intersect(cq.Solid.makeBox(10, 2 * hw, 30, pnt=cq.Vector(-30, yc - hw, FLOOR_BOT - 1))))
    if not relief_only:
        base = base.fuse(contact)
    # 3. solid port block under the PCB: from the face back to the backstop
    blk_x1 = rear + P.USB_BACKSTOP_GAP + 1.6
    blk = cq.Solid.makeBox(blk_x1 - fx, 2 * (W / 2 + c + P.WALL_MIN), -0.05 - FLOOR_TOP,
                           pnt=cq.Vector(fx, yc - (W / 2 + c + P.WALL_MIN), FLOOR_TOP))
    base = base.fuse(blk)
    # behind the wall the block stays below the small parts beside the receptacle
    base = base.cut(cq.Solid.makeBox(blk_x1 - (fx + P.WALL_MIN) + 1, 2 * (W / 2 + c + P.WALL_MIN) + 1, 5,
                                     pnt=cq.Vector(fx + P.WALL_MIN, yc - (W / 2 + c + P.WALL_MIN) - 0.5, P.USB_BLOCK_TOP_Z)))
    if relief_only:
        # generous pocket + opening, no flush/backstop features (used by the mic coupon)
        base = base.cut(cq.Solid.makeBox(blk_x1 - fx + 1, W + 2.0, H + 2.0 + 5, pnt=cq.Vector(fx - 0.5, yc - W / 2 - 1.0, zc - H / 2 - 1.0)))
        return base.clean()
    # 4. pocket for the receptacle body (legs/tabs region wider), open to the PCB above
    env_w = 2 * max(abs(P.USB_ENVELOPE[0][1]), abs(P.USB_ENVELOPE[1][1]))
    pocket = cq.Solid.makeBox(rear + P.USB_BACKSTOP_GAP - (fx + P.WALL_MIN), env_w + 2 * c, 10,
                              pnt=cq.Vector(fx + P.WALL_MIN, yc - env_w / 2 - c, g["z_bot"] - P.USB_UNDER_GAP))
    base = base.cut(pocket)
    # backstop: only up to USB_BACKSTOP_TOP_Z (parts on the PCB behind the receptacle)
    base = base.cut(cq.Solid.makeBox(4, env_w + 2 * c, 5, pnt=cq.Vector(rear + P.USB_BACKSTOP_GAP - 0.01, yc - env_w / 2 - c, P.USB_BACKSTOP_TOP_Z)))
    # 5. guide land (tight) right behind the opening part of the wall
    land_x0 = fx + P.WALL_MIN - P.USB_GUIDE_LEN
    gc = P.USB_GUIDE_CLEAR
    land = (cq.Workplane("YZ", origin=(land_x0, 0, 0)).center(yc, zc)
            .placeSketch(_stadium(W + 2 * gc, H + 2 * gc, g["r_shell"] + gc)).extrude(P.USB_GUIDE_LEN + 0.01).val()
            .fuse(cq.Solid.makeBox(P.USB_GUIDE_LEN + 0.01, W + 2 * gc,
                                   max(2.0, g["z_top"] + max(gc, c) + 0.01 - zc),   # never lower than the opening top
                                   pnt=cq.Vector(land_x0, yc - W / 2 - gc, zc))))
    base = base.cut(land)
    # 6. opening through the outer part of the wall (+0.3 per side, stadium)
    # bottom corners follow the shell (R1.28 + c); top corners = Paul's 0.3-0.5 fillet where the web meets
    # the side walls (a small radius also keeps the overhang at the top corners tiny)
    osk = (cq.Sketch().rect(W + 2 * c, H + 2 * c).vertices(">Y").fillet(P.USB_WEB_CORNER_R)
           .reset().vertices("<Y").fillet(g["r_shell"] + c))
    opening = (cq.Workplane("YZ", origin=(fx - 0.1, 0, 0)).center(yc, zc)
               .placeSketch(osk).extrude(P.WALL_MIN - P.USB_GUIDE_LEN + 0.2).val())
    base = base.cut(opening)
    # 7. 45 deg lead-in on the sides and bottom only (top edge = web stays full thickness)
    li = P.USB_LEADIN
    z_mid = zc
    outer = cq.Workplane("YZ", origin=(fx - 0.01, 0, 0)).center(yc, (z_mid + g["z_bot"] - c - li) / 2).rect(W + 2 * c + 2 * li, z_mid - (g["z_bot"] - c - li))
    inner = cq.Workplane("YZ", origin=(fx + li, 0, 0)).center(yc, (z_mid + g["z_bot"] - c) / 2).rect(W + 2 * c, z_mid - (g["z_bot"] - c))
    lead = cq.Solid.makeLoft([outer.wires().val(), inner.wires().val()], True)
    base = base.cut(lead)
    return base.clean()


def c2_usb() -> Coupon:
    top = top_shell_slice(USB_A0, USB_A1, USB_LATCHES)
    base = base_sector(USB_A0, USB_A1, USB_CUPS, USB_LATCHES)
    base = _usb_port_block(base)
    g = usb_geometry()
    web_center = g["face_x"] * -1 - P.USB_PCB_CONTACT_R
    y_edge = P.USB_SHELL_W / 2 + P.USB_OPENING_CLEAR
    web_corner = -g["face_x"] - math.sqrt(P.USB_PCB_CONTACT_R ** 2 - y_edge ** 2)
    base_label = deboss(base, "C2", (-12.0, 6.0), size=3.0, face_z=FLOOR_TOP)
    return Coupon(
        "C2_usb_wall", "USB-C port wall (base) + glass-seat slice (top)",
        {"usb_top_slice": top, "usb_base": base_label},
        notes=[f"Mouth target +{P.USB_PROUD_TARGET} proud of the flat port face. Opening {P.USB_SHELL_W:.2f}x{P.USB_SHELL_H:.2f} "
               f"+{P.USB_OPENING_CLEAR}/side, {P.USB_LEADIN} lead-in on sides + bottom only.",
               f"Web above the opening: {web_center:.2f} mm at the centre, {web_corner:.2f} mm at the opening corners, "
               f"touching the PCB/PMMA edge with zero gap, from Z {g['z_top'] + P.USB_OPENING_CLEAR:.2f} up to the seam at Z {P.SEAM_Z}.",
               f"Guide land {P.USB_GUIDE_CLEAR}/side behind the opening, support pad {P.USB_UNDER_GAP} under the receptacle, "
               f"backstop {P.USB_BACKSTOP_GAP} behind it.",
               "Two latches (top-shell beams into base pockets) and the spigot hold the halves; the spigot takes the plug's shear.",
               "Use with the real board: board glass-first into the top slice, then the base onto the back (cup on the standoff)."],
        checks={"latch_strain": latch_strain(), "web_min": web_center, "web_corner": web_corner},
        board_check=lambda: {"usb_top_slice": top, "usb_base": base},
        sections=[("section_port", 0.0, (-21.0, 0, 0.5), 3.0,
                   [((P.USB_MOUTH_X + P.USB_PROUD_TARGET, 0, 1.8), "web 0.63, zero gap to PCB", (-300, -200)),
                    ((P.USB_SHELL_REAR_X + 0.6, 0, -2.5), "backstop", (250, 180)),
                    ((-19.0, 0, -3.3), "support pad", (0, 220)),
                    ((-24.5, 0, P.SEAM_Z), "seam (2.8 above opening)", (-300, 40))]),
                  ("section_wide", 0.0, (-14.0, 0, 1.0), 1.3, [])],
    )


# -------------------------------------------------------------- C5 mic ducts


def _mic_collar_and_duct(base, mic):
    cx, cy = getattr(P, f"{mic}_CENTER")
    a, b, h = getattr(P, f"{mic}_SIZE")
    ang = getattr(P, f"{mic}_ANGLE")
    z_lid = getattr(P, f"{mic}_Z")[0]                       # lid face (package bottom in board frame)
    # package edges: the extracted angle is the diagonal; recover the edge direction from the STEP footprint
    rot = _mic_edge_angle(mic)
    cl, wl = P.MIC_COLLAR_CLEAR, P.MIC_COLLAR_WALL
    z_cav = z_lid - P.MIC_CAVITY_H
    outer = (cq.Workplane("XY").workplane(offset=z_cav - 1.4).center(cx, cy).transformed(rotate=(0, 0, rot))
             .rect(b + 2 * (cl + wl), a + 2 * (cl + wl)).extrude(0.0 - (z_cav - 1.4)).val())
    inner = (cq.Workplane("XY").workplane(offset=z_cav).center(cx, cy).transformed(rotate=(0, 0, rot))
             .rect(b + 2 * cl, a + 2 * cl).extrude(1.0 - z_cav).val())
    # crush rib: thin rim on top of the collar, MIC_CRUSH into the PCB
    rib_o = (cq.Workplane("XY").workplane(offset=-0.01).center(cx, cy).transformed(rotate=(0, 0, rot))
             .rect(b + 2 * (cl + wl / 2 + P.MIC_CRUSH_RIB_W / 2), a + 2 * (cl + wl / 2 + P.MIC_CRUSH_RIB_W / 2)).extrude(P.MIC_CRUSH + 0.01).val())
    rib_i = (cq.Workplane("XY").workplane(offset=-0.1).center(cx, cy).transformed(rotate=(0, 0, rot))
             .rect(b + 2 * (cl + wl / 2 - P.MIC_CRUSH_RIB_W / 2), a + 2 * (cl + wl / 2 - P.MIC_CRUSH_RIB_W / 2)).extrude(1).val())
    collar = outer.fuse(rib_o.cut(rib_i))
    # pillar under the collar down to the floor
    pillar = (cq.Workplane("XY").workplane(offset=FLOOR_TOP - 0.01).center(cx, cy).transformed(rotate=(0, 0, rot))
              .rect(b + 2 * (cl + wl), a + 2 * (cl + wl)).extrude(z_cav - 1.4 - FLOOR_TOP + 0.02).val())
    base = base.fuse(collar).fuse(pillar)
    # radial duct: teardrop section, from the cavity out through the wall
    phi = math.degrees(math.atan2(cy, cx))
    r0 = math.hypot(cx, cy)
    d = P.MIC_DUCT_D
    zc = z_cav - d / 2 + 0.05
    tear = (cq.Workplane("YZ").center(0, zc).circle(d / 2).extrude(R_OUT + 2 - r0).val()
            .fuse(cq.Workplane("YZ").center(0, zc + d / 2 * math.sqrt(2) / 2).rect(d / 2 * math.sqrt(2), d / 2 * math.sqrt(2))
                  .extrude(R_OUT + 2 - r0).val().rotate(cq.Vector(0, 0, zc + d / 2 * math.sqrt(2) / 2), cq.Vector(1, 0, zc + d / 2 * math.sqrt(2) / 2), 45)))
    tear = tear.moved(cq.Location(cq.Vector(r0, 0, 0)))
    tear = at_angle(tear, phi)
    # solid bar around the duct between the collar and the wall, so it can't leak into the case
    bar_top = zc + d / 2 + 0.6
    bar = (cq.Workplane("XY").workplane(offset=FLOOR_TOP - 0.01).center(0, 0)
           .rect(R_OUT - r0, d + 2 * 0.8).extrude(bar_top - FLOOR_TOP).val()
           .moved(cq.Location(cq.Vector(r0 + (R_OUT - r0) / 2, 0, 0))))
    base = base.fuse(at_angle(bar, phi))
    base = base.cut(inner).cut(tear)
    duct_len = R_OUT - (r0 + max(a, b) / 2 + cl)
    return base, duct_len


def _mic_edge_angle(mic):
    import lfcase.board as B
    g = B.load_board(verbose=False)
    s = g[mic.lower()]
    vs = sorted({(round(v.X, 3), round(v.Y, 3)) for v in s.Vertices()})
    # pick the corner with min x and its nearest neighbour: that edge's angle
    p0 = vs[0]
    others = sorted(vs[1:], key=lambda p: math.dist(p0, p))
    e_short, e_long = others[0], others[1]
    # rect(b, a): local X = long edge
    return math.degrees(math.atan2(e_long[1] - p0[1], e_long[0] - p0[0]))


def c5_mic() -> Coupon:
    base = base_sector(USB_A0, USB_A1, USB_CUPS, USB_LATCHES)
    base = _usb_port_block(base, relief_only=True)
    lens = {}
    for m in ("MIC1", "MIC2"):
        base, lens[m] = _mic_collar_and_duct(base, m)
    # trim the duct bars to the outer surface and the flat port face
    base = base.intersect(cyl(R_OUT, FLOOR_BOT - 1, P.SEAM_Z + 5))
    base = base.cut(cq.Solid.makeBox(20, 80, 40, pnt=cq.Vector(usb_geometry()["face_x"] - 20, -40, FLOOR_BOT - 5)))
    base = solidify(base)
    base = deboss(base, "C5", (-12.0, 6.0), size=3.0, face_z=FLOOR_TOP)
    return Coupon(
        "C5_mic_duct", "Mic ducts with crush-rib collars (base)",
        {"mic_base": base},
        notes=[f"Top-port mics (Paul). Collar {P.MIC_COLLAR_WALL} wall, {P.MIC_COLLAR_CLEAR}/side around each package, "
               f"{P.MIC_CAVITY_H} air gap over the lid, {P.MIC_CRUSH_RIB_W} wide crush rib {P.MIC_CRUSH} into the PCB.",
               f"Duct D{P.MIC_DUCT_D} teardrop, radial; length MIC1 {lens['MIC1']:.1f} mm, MIC2 {lens['MIC2']:.1f} mm (PLAN <= 8).",
               "Uses the C2 top slice to clamp the board (same sector and latches). The USB-C gets a plain relief here."],
        board_check=lambda: {"mic_base": base},
        sections=[("section_mic1", math.degrees(math.atan2(P.MIC1_CENTER[1], P.MIC1_CENTER[0])),
                   (P.MIC1_CENTER[0] * 1.12, P.MIC1_CENTER[1] * 1.12, -1.5), 3.4,
                   [((P.MIC1_CENTER[0], P.MIC1_CENTER[1], 0.06), "crush rib on PCB", (-280, -150)),
                    ((P.MIC1_CENTER[0] * 1.2, P.MIC1_CENTER[1] * 1.2, -2.05), "duct D1.2", (250, 160))])],
        checks={"duct_len": lens},
    )


# -------------------------------------------------------------- C6 PWR flexure


def c6_flexure() -> Coupon:
    key = P.PWR_IS                                   # 'KEY2'
    tip = getattr(P, f"{key}_TIP_CENTER")
    tip_dist = getattr(P, f"{key}_TIP_DIST")
    ang_out = getattr(P, f"{key}_PRESS_ANGLE")
    tz0, tz1 = getattr(P, f"{key}_TIP_Z")
    a0, a1 = ang_out - 95, ang_out + 72
    cups = [xy for xy in P.MOUNT_HOLES_XY if not (xy[0] < -5)]     # (13.75,-14.70) and (0, 20.50)
    base = base_sector(a0, a1, cups)
    r_in, t = BASE_IN, P.FLEXURE_THK
    r_out_b = r_in + t
    g = P.FLEX_GAP
    L = P.FLEXURE_LEN
    r_mid = r_in + t / 2
    nub_from_free = 1.5
    free_a = ang_out + math.degrees(nub_from_free / r_mid)
    root_a = free_a - math.degrees(L / r_mid)
    s = P.FLEX_SLOT
    slot_a = math.degrees(s / r_mid)
    z_beam_top = P.FLEX_BEAM_TOP_Z
    z_all = (FLOOR_BOT - 1, P.SEAM_Z + P.SPIGOT_H + 1)
    # The beam is the inner 1.2 mm of the wall. It stands on the build plate (its bottom IS the
    # bed face), so nothing is ever printed in mid-air:
    #   gap behind it (full height), radial slot at the free end (full height), the inner layer
    #   removed above the beam (open to the seam, so no bridge), a slot through the floor in front.
    cuts = [
        ([(r_out_b, z_all[0]), (r_out_b + g, z_all[0]), (r_out_b + g, z_all[1]), (r_out_b, z_all[1])], root_a, free_a + slot_a),
        ([(r_in - 0.7, z_all[0]), (r_out_b + g, z_all[0]), (r_out_b + g, z_all[1]), (r_in - 0.7, z_all[1])], free_a, free_a + slot_a),
        ([(r_in - 0.7, z_beam_top), (r_out_b + 0.01, z_beam_top), (r_out_b + 0.01, z_all[1]), (r_in - 0.7, z_all[1])], root_a, free_a + slot_a),
        ([(r_in - P.FLEX_FLOOR_SLOT, z_all[0]), (r_in + 0.001, z_all[0]), (r_in + 0.001, FLOOR_TOP + 0.01), (r_in - P.FLEX_FLOOR_SLOT, FLOOR_TOP + 0.01)],
         root_a, free_a + slot_a),
    ]
    for prof, x0, x1 in cuts:
        base = base.cut(rz_profile_solid(prof, x0, x1))
    # plunger tip, polar
    tip_r = math.hypot(tip[0], tip[1])
    tip_a = math.degrees(math.atan2(tip[1], tip[0]))
    zc = (tz0 + tz1) / 2
    # pad: grows out of the beam's outer face with a 45 deg underside, through a window in the skin
    pad_out = R_OUT - P.FLEX_PAD_RECESS
    pl = pad_out - (r_out_b - 0.05)
    pw_a = math.degrees(P.FLEX_PAD_W / 2 / r_out_b)
    pad_prof = [(r_out_b - 0.05, zc - 1.0 - pl), (pad_out, zc - 1.0), (pad_out, zc + 1.0), (r_out_b - 0.05, zc + 1.0)]
    win_a = math.degrees((P.FLEX_PAD_W / 2 + P.FLEX_WINDOW_CLEAR) / (r_out_b + g))
    win = rz_profile_solid([(r_out_b + g - 0.01, zc - 1.0 - pl - P.FLEX_WINDOW_CLEAR), (R_OUT + 1, zc - 1.0 - pl - P.FLEX_WINDOW_CLEAR),
                            (R_OUT + 1, zc + 1.0 + P.FLEX_WINDOW_CLEAR), (r_out_b + g - 0.01, zc + 1.0 + P.FLEX_WINDOW_CLEAR)],
                           tip_a - win_a, tip_a + win_a)
    base = base.cut(win)
    base = base.fuse(rz_profile_solid(pad_prof, tip_a - pw_a, tip_a + pw_a))
    # nub on the inner face, 45 deg underside, stops FLEX_REST_GAP short of the plunger tip
    s0 = tip_r + P.FLEX_REST_GAP
    nh = (tz1 - tz0 - 0.1) / 2
    nub_prof = [(s0, zc - nh), (r_in + 0.2, zc - nh - (r_in + 0.2 - s0)), (r_in + 0.2, zc + nh), (s0, zc + nh)]
    nw_a = math.degrees(P.FLEX_NUB_W / 2 / r_in)
    base = base.fuse(rz_profile_solid(nub_prof, tip_a - nw_a, tip_a + nw_a))
    base = solidify(base)
    base = deboss(base, "C6 PWR", (BASE_IN * 0.55 * math.cos(math.radians(ang_out - 5)), BASE_IN * 0.55 * math.sin(math.radians(ang_out - 5))),
                  size=2.8, face_z=FLOOR_TOP)
    a_load = L - nub_from_free
    defl = P.FLEX_REST_GAP + P.KEY_TRAVEL
    strain = 1.5 * t * defl / a_load ** 2
    return Coupon(
        "C6_pwr_flexure", "PWR flexure button (base)",
        {"pwr_base": base.clean()},
        notes=[f"PWR = {key} (Paul's photo). Tangential beam {L} long x {t} thick (PLAN 3.5), standing on the plate, bends along the layers. "
               f"Nub {P.FLEX_REST_GAP} from the plunger at rest; switch travel {P.KEY_TRAVEL} (TODO, not in the STEP). "
               f"Pad sits {P.FLEX_PAD_RECESS} below the outer surface so it can't be bumped in a pocket.",
               "Beam bottom = bed face, gap behind it, nothing above it, so no part of it is printed in mid-air. Test on the bare board: "
               "board glass-down on the table, press the coupon onto its back (cups on the standoffs)."],
        checks={"flex_strain": strain},
        board_check=lambda: {"pwr_base": base},
        sections=[("section_pwr", ang_out, (tip[0] * 1.12, tip[1] * 1.12, -1.2), 3.4,
                   [((tip[0], tip[1], tip[2]), "KEY2 (PWR) plunger", (-280, -170)),
                    ((tip[0] * 1.045, tip[1] * 1.045, -3.5), "beam 1.2, stands on plate", (240, -200)),
                    ((tip[0] * 1.115, tip[1] * 1.115, -1.6), "pad (1.5 recessed)", (260, 170))])],
    )


def print_orientation(name, shape):
    """Board frame -> print frame for each part."""
    if name in ("seat_ring", "usb_top_slice"):
        return flip_for_print(shape)
    return shape


# -------------------------------------------------------------- C9 USB fit gauge


def c9_usb_gauge():
    """Just the port block (flush wall, web, opening, guide land, pad, backstop) at three
    clearances. Press it onto the back of the bare board over the USB-C."""
    import importlib
    parts, notes = {}, []
    saved = (P.USB_GUIDE_CLEAR, P.USB_UNDER_GAP, P.USB_BACKSTOP_GAP, P.USB_OPENING_CLEAR)
    keep = cq.Solid.makeBox(12.5, 22.0, P.SEAM_Z - FLOOR_BOT, pnt=cq.Vector(-24.5, P.USB_CENTER_Y - 11.0, FLOOR_BOT))
    try:
        for tag, (g, u, b, o) in P.USB_GAUGE_VARIANTS.items():
            P.USB_GUIDE_CLEAR, P.USB_UNDER_GAP, P.USB_BACKSTOP_GAP, P.USB_OPENING_CLEAR = g, u, b, o
            base = base_sector(USB_A0, USB_A1, [])
            base = _usb_port_block(base).intersect(keep)
            base = solidify(base)
            base = deboss(base, tag, (-15.0, P.USB_CENTER_Y + 8.0), size=3.0, face_z=FLOOR_TOP)
            parts[f"usb_gauge_{tag}"] = base
            notes.append(f"{tag}: guide land {g}/side, pad {u} under, backstop {b} behind, opening {o}/side")
    finally:
        P.USB_GUIDE_CLEAR, P.USB_UNDER_GAP, P.USB_BACKSTOP_GAP, P.USB_OPENING_CLEAR = saved
    return Coupon("C9_usb_gauge", "USB-C fit gauge (3 clearances)", parts,
                  notes=notes + ["Bare board glass-down on the table; press each gauge straight down over the USB-C "
                                 "(floor up). It should slide on by hand and sit with its inner face against the PCB edge."],
                  board_check=lambda: dict(parts))
