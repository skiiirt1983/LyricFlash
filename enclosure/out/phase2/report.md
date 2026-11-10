# Phase 2 report: full case v4 (20 deg, layout B)

Tilt 20 deg, board centre 30.4 mm above the desk. Parts: top_shell, base, pwr_pin, cable_clip, clip_front, clip_back.

## Interference against the STEP board (boolean volume)

| Part | Solids | Valid | Overlaps |
|---|---|---|---|
| top_shell | 1 | True | none |
| base | 1 | True | battery_envelope 9.692 mm3, speaker_envelope 4.656 mm3 (speaker: the grip ribs, 0.15 mm by design, as C8) (battery: the case's rounded inner corners trim the pocket's 1 mm clearance at its four extreme corners by <= 0.5 mm; the 56 mm cell keeps >= 0.5 mm) |
| pwr_pin | 1 | True | none |
| cable_clip | 1 | True | none |
| clip_front | 1 | True | none |
| clip_back | 1 | True | none |

Part against part: none overlap.

## Assembly (tilt-in swing)

**PASS**: nothing comes within 0.2 mm on the way in except designed contacts. Details in `assembly_sim.md`.

## Printability (layer check, print orientation)

| Part | Orientation | Result |
|---|---|---|
| base | floor down | 33 flagged layers (see below) |
| top_shell | face-down (top face on the plate) | 5 flagged layers (see below) |
| pwr_pin | on its flat side | clean |
| clip_front | flat (profile on the plate) | clean |
| clip_back | flat (profile on the plate) | clean |
| cable_clip | as installed | clean |

Base: **no supports needed**. What the check still flags, all small:

- **USB-C opening, top edge** (visible): with the 12 deg tilt it's a sloped ceiling, not a flat 9.5 mm bridge. If the first print shows a droop, paint a support enforcer inside the opening only.
- PWR tube roof: bridged across the 3.5 mm bore as the roof climbs with the tilt (inside).
- Two specks under 2 mm2 (a lead-gap edge, the BOOT tube mouth): print as short overhangs.

The USB-C insert's underside is filled down to the wall at 45 deg, and the mic bars are gone (Paul), so nothing inside hangs over the battery any more.

Top shell: the latch catch ledges (0.9 deep) are anchored on the outer wall and bridged 6.6 mm between the pocket's side walls.

## Centre of gravity and tipping

Total 84.3 g (board 20 g and battery 38 g are estimates). CG at X -0.17, Y 0.89, Z 17.36 mm. Smallest tipping angle **55.9 deg** (towards 0 deg; PLAN asks > 30).

## Key numbers

- Mic holes: D2.0 teardrop straight through the USB recess wall, 6.1 / 6.3 mm from the mics (no collar, no duct: Paul).
- PWR pin: exits back-right at (26.45, 17.99, 35.44), rest gap 0.35 to the plunger.
- BOOT pinhole: D1.6, 3.0 mm guide, exits front-right at (26.44, -16.59, 23.39).
- Speaker: 53 x D1.5 grille holes (teardrop) in the right wall.
- Switch: C4 K3 pocket in the floor at (17.78, 22.7).
- Latches: 2 PETG clips (C7 v5 notch 1: hook 1.0, 70 deg) at X 12.5, front and back.

