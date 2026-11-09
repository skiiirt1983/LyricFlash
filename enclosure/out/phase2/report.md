# Phase 2 report: full case v1

Tilt 12 deg, board centre 23.9 mm above the desk. Parts: top_shell, base, pwr_pin, cable_clip, clip_front, clip_back.

## Interference against the STEP board (boolean volume)

| Part | Solids | Valid | Overlaps |
|---|---|---|---|
| top_shell | 1 | True | none |
| base | 1 | True | speaker_envelope 4.656 mm3 (the speaker grip ribs, 0.15 mm by design, as C8) |
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
| base | floor down | 49 flagged layers (see below) |
| top_shell | face-down (top face on the plate) | 4 flagged layers (see below) |
| pwr_pin | on its flat side | clean |
| clip_front | flat (profile on the plate) | clean |
| clip_back | flat (profile on the plate) | clean |
| cable_clip | as installed | clean |

Flagged in the base, all inside except the USB-C mouth:

- Over the battery: the USB-C port block and both mic bars (one region, 103 mm2 at Z 13.7) and the inner end of the BOOT tube. **Tree supports, inside only** (Paul's choice); they snap off before assembly.
- The USB-C block's underside and the PWR tube's roof follow the 12 deg board tilt, so each layer steps ~0.7 mm: bridged between side walls, prints, may need the supports above.
- **USB-C opening, top edge** (the only visible one): with the 12 deg tilt it's no longer a flat 9.5 mm bridge. Paint a support enforcer inside the opening in Bambu Studio if the first print shows a droop.

Top shell: the latch catch ledges (0.9 deep) are anchored on the outer wall and bridged 6.6 mm between the pocket's side walls.

## Centre of gravity and tipping

Total 86.9 g (board 20 g and battery 38 g are estimates). CG at X 2.87, Y 0.44, Z 14.88 mm. Smallest tipping angle **59.6 deg** (towards 90 deg; PLAN asks > 30).

## Key numbers

- Mic ducts: D1.2 teardrop, 7.7 / 7.9 mm (PLAN <= 8), open (no collar, Paul).
- PWR pin: exits back-right at (34.04, 26.56, 28.11), rest gap 0.35 to the plunger.
- BOOT pinhole: D1.6, 23.5 mm guide, exits front-right at (34.78, -26.26, 17.4).
- Speaker: 53 x D1.5 grille holes (teardrop) in the right wall.
- Switch: C4 K3 pocket in the floor at (-16.0, 20.4).
- Latches: 2 PETG clips (C7 v5 notch 1: hook 1.0, 70 deg) at X 12.5, front and back.

