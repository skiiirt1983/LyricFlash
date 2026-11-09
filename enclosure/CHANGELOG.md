# Enclosure changelog

## Phase 1 (coupons), 2026-11-09
- Bezel option A chosen (lip, board loads from behind, base clamps it on the M2 standoffs).
- Board facts from Paul: PWR = KEY2, BOOT = KEY1, BAT = J1, SPK = H3, both mics top-port.
- Coupons C1-C8 built from params.py, checked against the STEP board, exported (STL/STEP/3MF).
- USB-C: port wall, web, guide land, pad and backstop all on the base tray (Paul's conditions 1-6).
  Seam at Z 3.0 (2.8 mm above the opening); spigot top 0.2 below the display flex folds.
  Mouth target +0.15 (see USB_STACKUP.md).
- Speaker added (30 x 20 x 6.8, Paul's listing image): pocket + grille coupon C8.
- Speaker: leads exit mid 20 mm end (matches C8 slot); placement free, no mic keep-out (plays only when mics idle).

## Phase 0, 2026-11-09
- Board STEP imported; section 4 values extracted into params.py with sources; labelled renders.
