# Enclosure changelog

## Phase 1 print results, round 1 (Paul 2026-11-09, gold PLA Metal)
- C2: USB-C receptacle does not fit in its base pocket; the support pad under it is not deep
  enough (C2 clearances 0.10 side / 0.10 under / 0.05 behind were too tight as printed).
  -> C9 USB fit gauge, three variants A/B/C (see params USB_GAUGE_VARIANTS).
- C2: one latch beam broke ("pretty flimsy"): upright PLA beam, bending across the layers.
  -> Phase 2: latches become separate clips printed flat in PETG (layers along the beam).

## Acceptance criterion (Paul 2026-11-09)
- A ~100 g device can't take a 5-20 N USB-C insertion one-handed on a desk. C2 test now plugs in
  while holding the coupon. Proposed PLAN 7 wording: "USB-C plugs in fully while the case is held;
  once plugged in, a gentle tug on the cable from any direction doesn't slide or tip the case on
  the desk" (the tug is what the cable clip + low exit handle). Pending Paul's OK.

## Materials, 2026-11-09
- Top shell: Iridium Gold PLA Metal. Base tray: black PETG. Coupons printed per plate in those.
- Phase 2: latch beams move to the PETG base (no flexing features in PLA).

## Phase 1 v2, 2026-11-09 (Paul: non-manifold 3MF, C6 floating regions)
- 3MF writer welds each part into one connected mesh; every object checked: 0 non-manifold edges.
- C6 flexure redesigned: beam stands on the plate (full-height inner wall layer, gap behind,
  nothing above, slot through the floor); pad and nub grow out with 45 deg undersides.
- New layer-by-layer check in report.md (floating islands, cantilevered overhangs, bridges);
  it flags the old C6 and passes all 15 v2 parts.
- USB opening top corners R0.5 (Paul's web-corner fillet); the top edge is one 8.7 mm bridge.

## Phase 1 (coupons), 2026-11-09
- Bezel option A chosen (lip, board loads from behind, base clamps it on the M2 standoffs).
- Board facts from Paul: PWR = KEY2, BOOT = KEY1, BAT = J1, SPK = H3, both mics top-port.
- Coupons C1-C8 built from params.py, checked against the STEP board, exported (STL/STEP/3MF).
- USB-C: port wall, web, guide land, pad and backstop all on the base tray (Paul's conditions 1-6).
  Seam at Z 3.0 (2.8 mm above the opening); spigot top 0.2 below the display flex folds.
  Mouth target +0.15 (see USB_STACKUP.md).
- Speaker added (30 x 20 x 6.8, Paul's listing image): pocket + grille coupon C8.
- Speaker: leads exit dead centre of a 20 mm end, mid-thickness (C8 slot spans it); placement free, no mic keep-out (plays only when mics idle).

## Phase 0, 2026-11-09
- Board STEP imported; section 4 values extracted into params.py with sources; labelled renders.
