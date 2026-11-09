# Phase 1 coupon report

## C3_battery_pocket: Battery pocket tray

- Pocket 58.0 x 36.5 (cell 56.0 x 34.5), lead space 10.0 at one 34.5 end, walls 3.0 high. C-gauge on the end: 12.0 gap (cell 10.3 + swelling).
- `battery_tray`: 84.4 x 39.7 x 4.2 mm, volume 4.53 cm3, 10920 triangles, open mesh edges: 0, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- Interference with the STEP board: not applicable (coupon does not hold the board).

## C4_switch_pocket: Switch pocket + knob slot

- Three pockets for knob length (3.0, 4.0, 5.0) mm (labels K3/K4/K5). Pocket 9.3 x 4.3, body clearance +0.4 in height, 2.0 mm under the body for pins/solder. Knob slot 4.6 x 2.3; knob tip should sit 0.4 below the outer face. Switch slides in sideways from the open side (knob through the entry channel), lips under its ends hold it.
- `switch_pocket`: 41.5 x 10.0 x 11.3 mm, volume 3.00 cm3, 3260 triangles, open mesh edges: 0, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- Interference with the STEP board: not applicable (coupon does not hold the board).

## C7_snap_latches: Snap-fit latch pairs

- Beam 10.0 long x 1.2 thick, hook 0.8, lead-in 30 deg, retention face 70 deg. Clearance c = side fit of the spigot AND axial play at the catch.
- Printed as 2-D profiles standing on the bed, so the beams bend along the layers (strong direction).
- `snap_male_0.15`: 14.0 x 15.5 x 6.0 mm, volume 0.48 cm3, 2640 triangles, open mesh edges: 0, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_female_0.15`: 16.0 x 15.0 x 6.0 mm, volume 1.17 cm3, 4884 triangles, open mesh edges: 0, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_male_0.25`: 14.0 x 15.5 x 6.0 mm, volume 0.48 cm3, 3350 triangles, open mesh edges: 0, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_female_0.25`: 16.0 x 15.0 x 6.0 mm, volume 1.16 cm3, 6304 triangles, open mesh edges: 0, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_male_0.35`: 14.0 x 15.5 x 6.0 mm, volume 0.48 cm3, 4194 triangles, open mesh edges: 0, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_female_0.35`: 16.0 x 15.0 x 6.0 mm, volume 1.14 cm3, 7992 triangles, open mesh edges: 0, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- Latch beam strain at full hook deflection: 1.44 % (limit PLA 1.5 %: OK, PETG 2.5 %: OK)
- Interference with the STEP board: not applicable (coupon does not hold the board).

## Pending coupons

- C1_display_seat_ring: waits for the bezel choice (option A lip vs option B PrintSphere style)
- C2_usb_wall: waits for the bezel choice (it includes a slice of the glass seat)
- C5_mic_duct: waits for the mic sound-port side (top or bottom port)
- C6_pwr_flexure: waits for the bezel choice (decides which shell carries the flexure and its direction)
