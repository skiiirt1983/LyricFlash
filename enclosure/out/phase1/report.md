# Phase 1 coupon report

## C1_display_seat: Display seat ring + base locator

- Seat ring: bore D49.26 (glass D48.96 +0.3), lip 1.0 over the glass edge (opening D46.96), lip 1.0 thick so the rim stands 1.0 proud of the glass. Prints lip-down.
- Base locator: floor with the three standoff cups (ID 3.9 on the D3.5 standoffs, seat at the standoff face). The cup pattern only fits one way round, so it also checks orientation.
- `seat_ring`: 55.0 x 55.0 x 3.9 mm, volume 2.01 cm3, 1008 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `seat_ring` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- `base_locator`: 46.7 x 47.0 x 5.7 mm, volume 2.61 cm3, 4964 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `base_locator` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- Interference `seat_ring` vs STEP board: **0 mm3** (no contact with any board part)
- Interference `base_locator` vs STEP board: **0 mm3** (no contact with any board part)

## C2_usb_wall: USB-C port wall (base) + glass-seat slice (top)

- Mouth target +0.15 proud of the flat port face. Opening 8.94x3.16 +0.3/side, 0.4 lead-in on sides + bottom only.
- Web above the opening: 0.63 mm at the centre, 1.13 mm at the opening corners, touching the PCB/PMMA edge with zero gap, from Z 0.21 up to the seam at Z 3.0.
- Guide land 0.1/side behind the opening, support pad 0.1 under the receptacle, backstop 0.05 behind it.
- Two latches (top-shell beams into base pockets) and the spigot hold the halves; the spigot takes the plug's shear.
- Use with the real board: board glass-first into the top slice, then the base onto the back (cup on the standoff).
- `usb_top_slice`: 16.5 x 48.6 x 10.5 mm, volume 0.88 cm3, 546 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `usb_top_slice` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- `usb_base`: 23.8 x 48.6 x 11.7 mm, volume 3.06 cm3, 3582 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 7.5 mm2
- `usb_base` layer check: **no floating regions**, 3 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
    - short overhang at print z 7.9: 0.06 mm2, 0.7 mm long, reaches 0.09 mm past support, XY (-23.6, -4.5, -22.9, -4.4)
    - short overhang at print z 7.9: 0.06 mm2, 0.7 mm long, reaches 0.09 mm past support, XY (-23.6, 4.3, -22.9, 4.4)
    - bridge at print z 8.1: 6.36 mm2, 8.7 mm long, reaches 3.6 mm past support, XY (-23.6, -4.4, -22.6, 4.3)
    - overhang at print X[-23.4,-22.7] Y[2.2,4.6] Z 0.1..0.3
    - overhang at print X[-23.6,-23.4] Y[4.9,4.9] Z -1.7..-1.7
    - overhang at print X[-23.6,-23.4] Y[-4.9,-4.9] Z -1.7..-1.7
    - overhang at print X[-23.4,-22.7] Y[-4.6,-2.2] Z 0.1..0.3
    - overhang at print X[-23.4,-23.4] Y[-0.2,-0.2] Z 0.2..0.2
    - overhang at print X[-23.2,-23.2] Y[1.4,1.4] Z 0.2..0.2
- latch strain: 0.90 % (PLA limit 1.5 %, PETG 2.5 %)
- Interference `usb_top_slice` vs STEP board: **0 mm3** (no contact with any board part)
- Interference `usb_base` vs STEP board: **0 mm3** (no contact with any board part)

## C3_battery_pocket: Battery pocket tray

- Pocket 58.0 x 36.5 (cell 56.0 x 34.5), lead space 10.0 at one 34.5 end, walls 3.0 high. C-gauge on the end: 12.0 gap (cell 10.3 + swelling).
- `battery_tray`: 84.4 x 39.7 x 4.2 mm, volume 4.53 cm3, 10920 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `battery_tray` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- Interference with the STEP board: not applicable (coupon does not hold the board).

## C4_switch_pocket: Switch pocket + knob slot

- Three pockets for knob length (3.0, 4.0, 5.0) mm (labels K3/K4/K5). Pocket 9.3 x 4.3, body clearance +0.4 in height, 2.0 mm under the body for pins/solder. Knob slot 4.6 x 2.3; knob tip should sit 0.4 below the outer face. Switch slides in sideways from the open side (knob through the entry channel), lips under its ends hold it.
- `switch_pocket`: 41.5 x 10.0 x 11.3 mm, volume 3.00 cm3, 3260 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `switch_pocket` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- Interference with the STEP board: not applicable (coupon does not hold the board).

## C5_mic_duct: Mic ducts with crush-rib collars (base)

- Top-port mics (Paul). Collar 0.8 wall, 0.2/side around each package, 0.4 air gap over the lid, 0.4 wide crush rib 0.12 into the PCB.
- Duct D1.2 teardrop, radial; length MIC1 4.9 mm, MIC2 4.7 mm (PLAN <= 8).
- Uses the C2 top slice to clamp the board (same sector and latches). The USB-C gets a plain relief here.
- `mic_base`: 23.7 x 48.6 x 11.8 mm, volume 3.11 cm3, 3856 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `mic_base` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- Interference `mic_base` vs `pcb`: 1.559 mm3 at X[-20.61,-14.20] Y[-14.91,14.17] Z[-0.00,0.12], intended: crush ribs, 0.12 mm into the PCB by design

## C6_pwr_flexure: PWR flexure button (base)

- PWR = KEY2 (Paul's photo). Tangential beam 10.0 long x 1.2 thick (PLAN 3.5), standing on the plate, bends along the layers. Nub 0.3 from the plunger at rest; switch travel 0.25 (TODO, not in the STEP). Pad sits 1.5 below the outer surface so it can't be bumped in a pocket.
- Beam bottom = bed face, gap behind it, nothing above it, so no part of it is printed in mid-air. Test on the bare board: board glass-down on the table, press the coupon onto its back (cups on the standoffs).
- `pwr_base`: 35.1 x 51.6 x 11.7 mm, volume 4.71 cm3, 6164 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 10.4 mm2
- `pwr_base` layer check: **no floating regions**, 1 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
    - bridge at print z 8.3: 9.34 mm2, 3.95 mm long, reaches 1.25 mm past support, XY (19.8, 12.3, 23.9, 16.8)
    - overhang at print X[20.9,22.8] Y[12.9,16.1] Z 0.6..0.6
    - overhang at print X[22.0,22.5] Y[6.0,7.6] Z -6.1..-6.1
    - overhang at print X[18.8,21.9] Y[8.1,13.8] Z -6.1..-6.1
- flex strain: 1.37 % (PLA limit 1.5 %, PETG 2.5 %)
- Interference `pwr_base` vs STEP board: **0 mm3** (no contact with any board part)

## C7_snap_latches: Snap-fit latch pairs

- Beam 10.0 long x 1.2 thick, hook 0.8, lead-in 30 deg, retention face 70 deg. Clearance c = side fit of the spigot AND axial play at the catch.
- Printed as 2-D profiles standing on the bed, so the beams bend along the layers (strong direction).
- `snap_male_0.15`: 14.0 x 15.5 x 6.0 mm, volume 0.48 cm3, 2640 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_male_0.15` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- `snap_female_0.15`: 16.0 x 15.0 x 6.0 mm, volume 1.17 cm3, 4884 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_female_0.15` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- `snap_male_0.25`: 14.0 x 15.5 x 6.0 mm, volume 0.48 cm3, 3350 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_male_0.25` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- `snap_female_0.25`: 16.0 x 15.0 x 6.0 mm, volume 1.16 cm3, 6304 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_female_0.25` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- `snap_male_0.35`: 14.0 x 15.5 x 6.0 mm, volume 0.48 cm3, 4194 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_male_0.35` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- `snap_female_0.35`: 16.0 x 15.0 x 6.0 mm, volume 1.14 cm3, 7992 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `snap_female_0.35` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- Latch beam strain at full hook deflection: 1.44 % (limit PLA 1.5 %: OK, PETG 2.5 %: OK)
- Interference with the STEP board: not applicable (coupon does not hold the board).

## C8_speaker_pocket: Speaker pocket + grille

- Speaker 30.0 x 20.0 x 6.8 (Paul's listing image). Pocket +0.3/side, 0.4 extra depth, corner R3.0 (TODO, estimated). Front seal rib 0.3 high on the speaker frame, grille 53 x D1.5. Four side ribs grip it (0.25 interference). Printed grille-down; holes are vertical, no support.
- `speaker_pocket`: 33.8 x 23.8 x 8.8 mm, volume 2.29 cm3, 22920 triangles, mesh: watertight, unsupported overhang (>45 deg, off-bed): 0.0 mm2
- `speaker_pocket` layer check: **no floating regions**, 0 bridge / short-overhang patches (bridges <= 10 mm supported both ends, overhangs <= 0.3 mm past 45 deg)
- Interference with the STEP board: not applicable (coupon does not hold the board).

## Pending coupons

