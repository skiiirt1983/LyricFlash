# Phase 2 report: full case v5 (20 deg, layout B, no button access)

Tilt 20 deg, board centre 50.9 mm above the desk. Parts: top_shell, base, carrier.

## Interference against the STEP board (boolean volume)

| Part | Solids | Valid | Overlaps |
|---|---|---|---|
| top_shell | 1 | True | none |
| base | 1 | True | none |
| carrier | 1 | True | none |

Part against part: none overlap.

## Assembly (v10: board + carrier swung into the base, then the shell straight down)

**PASS**: nothing comes within 0.2 mm of the board, screen or carrier on either path except designed contacts (printed-to-printed guides >= 0.1 mm). Details in `assembly_sim.md`.

## Printability (layer check, print orientation)

| Part | Orientation | Result |
|---|---|---|
| base | floor down | 62 flagged layers (see below) |
| top_shell | face-down (top face on the plate) | clean |
| carrier | flat, top (board side) on the plate; countersinks face up | clean |

Base: **no supports needed**. What the check still flags, all small:

- **USB-C opening, top edge** (visible): with the 12 deg tilt it's a sloped ceiling, not a flat 9.5 mm bridge. If the first print shows a droop, paint a support enforcer inside the opening only.
- Small specks under 2 mm2 (a lead-gap edge): print as short overhangs.

The USB-C insert's underside is filled down to the wall at 45 deg, and the mic bars are gone (Paul), so nothing inside hangs over the battery any more.

Top shell: the latch catch ledges (0.9 deep) are anchored on the outer wall and bridged 6.6 mm between the pocket's side walls.

## Battery insertion (v9)

Real cell lowered straight into its pocket (centred with 0.2 mm air; against either pocket side): **PASS** centred +0.2 air: 0.0 mm3, x 5.34 against the pocket side: 0.0 mm3, x 6.32 against the pocket side: 0.0 mm3

## Centre of gravity and tipping

Total 111.5 g (board 20 g and battery 38 g are estimates). CG at X 3.76, Y 1.99, Z 29.52 mm. Smallest tipping angle **36.3 deg** (towards 0 deg; PLAN asks > 30).

## Key numbers

- Mic holes: D2.0 teardrop straight through the USB recess wall, 6.1 / 6.3 mm from the mics (no collar, no duct: Paul).
- PWR and BOOT buttons: no access (Paul, v5: not used by the firmware). Walls are plain.
- Speaker: none (Paul, v9). The battery stands on its edge along the right wall.
- Switch: SS-12D00 pressed into the left wall at (-25.43, 0.0, 4.19), knob out 1.85 mm through a 1.0 mm skin.
- Shell: alignment rim (1.2 mm, 2.5 deep, 0.15 clear) inside the base wall, 4 corner magnet pairs (4 x 2), goes on last along the board normal.
- Board: carrier plate (2.4 mm PETG) on the 3 standoffs with M2 x 3 flat heads; 3 tabs on base posts, M2.5 x 6 pan head + M2.5 nut each (nut slides into a slot in the post).

