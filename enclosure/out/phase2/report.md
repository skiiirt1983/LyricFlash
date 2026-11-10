# Phase 2 report: full case v5 (20 deg, layout B, no button access)

Tilt 20 deg, board centre 32.9 mm above the desk. Parts: top_shell, base, clip_front, clip_back.

## Interference against the STEP board (boolean volume)

| Part | Solids | Valid | Overlaps |
|---|---|---|---|
| top_shell | 1 | True | none |
| base | 1 | True | battery_envelope 9.996 mm3, speaker_envelope 4.656 mm3 (speaker: the grip ribs, 0.15 mm by design, as C8) (battery: the case's rounded inner corners trim the pocket's 1 mm clearance at its four extreme corners by <= 0.5 mm; the 56 mm cell keeps >= 0.5 mm) |
| clip_front | 1 | True | none |
| clip_back | 1 | True | none |

Part against part: none overlap.

## Assembly (tilt-in swing)

**PASS**: nothing comes within 0.2 mm on the way in except designed contacts. Details in `assembly_sim.md`.

## Printability (layer check, print orientation)

| Part | Orientation | Result |
|---|---|---|
| base | floor down | 20 flagged layers (see below) |
| top_shell | face-down (top face on the plate) | 2 flagged layers (see below) |
| clip_front | flat (profile on the plate) | clean |
| clip_back | flat (profile on the plate) | clean |

Base: **no supports needed**. What the check still flags, all small:

- **USB-C opening, top edge** (visible): with the 12 deg tilt it's a sloped ceiling, not a flat 9.5 mm bridge. If the first print shows a droop, paint a support enforcer inside the opening only.
- Small specks under 2 mm2 (a lead-gap edge): print as short overhangs.

The USB-C insert's underside is filled down to the wall at 45 deg, and the mic bars are gone (Paul), so nothing inside hangs over the battery any more.

Top shell: the latch catch ledges (0.9 deep) are anchored on the outer wall and bridged 6.6 mm between the pocket's side walls.

## Centre of gravity and tipping

Total 93.6 g (board 20 g and battery 38 g are estimates). CG at X -0.49, Y 1.59, Z 18.28 mm. Smallest tipping angle **53.8 deg** (towards 180 deg; PLAN asks > 30).

## Key numbers

- Mic holes: D2.0 teardrop straight through the USB recess wall, 6.1 / 6.3 mm from the mics (no collar, no duct: Paul).
- PWR and BOOT buttons: no access (Paul, v5: not used by the firmware). Walls are plain.
- Speaker: 53 x D1.5 grille holes (teardrop) in the right wall.
- Switch: C4 K3 pocket in the floor at (16.58, 22.7).
- Latches: 2 PETG clips (C7 v5 notch 1: hook 1.0, 70 deg) at X 12.5, front and back.

