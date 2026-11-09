# Round 2 coupons

## C1_display_seat: Display seat ring + base locator
- Seat ring: bore D49.26 (glass D48.96 +0.3), lip 1.0 over the glass edge (opening D46.96), lip 1.0 thick so the rim stands 1.0 proud of the glass. Prints lip-down.
- Base locator: floor with the three standoff cups (ID 3.9 on the D3.5 standoffs, seat at the standoff face). The cup pattern only fits one way round, so it also checks orientation.
- `seat_ring`: layers OK, interference 0 mm3
- `base_locator`: layers OK, interference 0 mm3

## C5_mic_duct: Mic ducts with crush-rib collars (base)
- Top-port mics (Paul). Collar 0.8 wall, 0.2/side around each package, 0.4 air gap over the lid, 0.35 wide crush rib 0.35 into the PCB.
- Duct D1.2 teardrop, radial; length MIC1 4.9 mm, MIC2 4.7 mm (PLAN <= 8).
- Uses the C2 top slice to clamp the board (same sector and latches). The USB-C gets a plain relief here.
- `mic_base`: layers OK, interference 0 mm3

## C6_pwr_flexure: PWR flexure button (base)
- PWR = KEY2 (Paul's photo). Tangential beam 10.0 long x 0.9 thick (PLAN 3.5), standing on the plate, bends along the layers. Nub 0.2 from the plunger at rest; switch travel 0.25 (TODO, not in the STEP). Pad sits 1.5 below the outer surface so it can't be bumped in a pocket.
- Beam bottom = bed face, gap behind it, nothing above it, so no part of it is printed in mid-air. Test on the bare board: board glass-down on the table, press the coupon onto its back (cups on the standoffs).
- `pwr_base`: layers OK, interference 0 mm3

## C9_usb_gauge: USB-C fit gauge (3 clearances)
- A: guide land 0.2/side, pad 0.25 under, backstop 0.1 behind, opening 0.35/side
- B: guide land 0.3/side, pad 0.45 under, backstop 0.1 behind, opening 0.45/side
- C: guide land 0.4/side, pad 0.7 under, backstop 0.15 behind, opening 0.55/side
- Bare board glass-down on the table; press each gauge straight down over the USB-C (floor up). It should slide on by hand and sit with its inner face against the PCB edge.
- `usb_gauge_A`: layers OK, interference 0 mm3
- `usb_gauge_B`: layers OK, interference 0 mm3
- `usb_gauge_C`: layers OK, interference 0 mm3

## C7_snap_latches: Snap-fit latch pairs
- Beam 10.0 long x 1.2 thick, hook 0.8, lead-in 30 deg, retention face 70 deg. v2: side fit 0.15 for all; axial play at the catch P1 = 0.10 preload (pulls tight), P0 = zero play, L1 = 0.10 play.
- Printed as 2-D profiles standing on the bed, so the beams bend along the layers (strong direction).
- `snap_male_P1`: layers OK
- `snap_female_P1`: layers OK
- `snap_male_P0`: layers OK
- `snap_female_P0`: layers OK
- `snap_male_L1`: layers OK
- `snap_female_L1`: layers OK

## C8_speaker_pocket: Speaker pocket + grille
- Speaker 30.0 x 20.0 x 6.8 (Paul's listing image). Pocket +0.4/side, 0.4 extra depth, corner R3.0 (TODO, estimated). Front seal rib 0.3 high on the speaker frame, grille 53 x D1.5. Four side ribs grip it (0.15 interference). Printed grille-down; holes are vertical, no support.
- `speaker_pocket`: layers OK
