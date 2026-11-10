# Phase 2: full case v3 (layout B, no mic boxes), print and check

One file: `out/phase2/lyricflash_case_project.3mf`. Everything is flat on the plate, fitted together in
the **Assembly view**, and pre-assigned to a filament: **slot 1 = Iridium Gold PLA Metal**, **slot 2 =
black PETG**. If the parts don't all fit on one plate, Bambu Studio puts the extras on a second plate.

| Part | Filament | Orientation on the plate |
|---|---|---|
| `top_shell` | gold (1) | face-down: the top face (with the screen ring) on the plate |
| `base` | black PETG (2) | floor down |
| `clip_front`, `clip_back` | black PETG (2), springy | flat |
| `pwr_pin` | gold (1) | on its flat side |
| `cable_clip` | black PETG (2), springy | as installed (U-channel facing up) |

**Settings:** 0.2 mm layers, 4 walls, 15 % gyroid, 0.4 mm nozzle.
- **Base: supports OFF.** Nothing inside hangs over the battery any more (v3).
- **Top shell, pin, clips, cable clip: supports OFF.**
- **USB-C opening:** with the 12° tilt, its top edge prints as a sloped ceiling, not a flat bridge. Print once
  without support. If the top edge of the opening droops, paint a support enforcer inside the opening only.

`out/phase2/report.md` has every check: board interference (none), the tilt-in swing (PASS), the layer check,
the centre of gravity (tips at 58.1°, PLAN asks for > 30°).

## Before assembly
- Check the USB-C opening, both mic holes (Ø2.0, inside the USB-C recess, either side of the port) and the
  BOOT pinhole are clear: a 1 mm paperclip should slide into the BOOT hole and click the button.
- **Switch (optional):** solder the SS-12D00 into the battery's red lead first. Slide it into the floor
  pocket at the back-right (behind the speaker) from the battery side, knob down through the slot. Nothing is pressed in: it slides
  sideways and the end lips hold it, as on C4.

## Assembly (no pressing on the screen, ever)
1. **Battery:** lay it flat in its pocket, long side front-to-back, leads at the back-left corner, out
   through the gap in the low wall on its left. Its right side closes the switch pocket.
2. **Speaker:** slide it down into the pocket against the right wall, cone toward the grille, leads at the
   back end and up through the slot. The side ribs hold it, as on C8.
3. **Latch clips:** slide each clip sideways into its channel on the front and back wall (it goes in from
   the open end, hook facing the outer wall, beam pointing up). No force: 0.25 mm per side.
4. **PWR pin:** from inside, push it into the short tube on the right wall (towards the back), collar
   inside, until the collar stops at the tube. It's loose on purpose.
5. **Board:** put a soft cloth on the table, put the gold top shell face-down on it, and drop the board in
   glass-first. The glass rests on the lip. Plug in the battery (J1) and the speaker (H3).
6. **Base:** hold it upside down over the board. Hook the USB-C opening over the board's USB-C receptacle
   first (left side), then swing the right side down. It should come down on its own weight until the
   latches touch, with nothing rubbing on the way (the simulation keeps 0.2 mm of air everywhere else).
   **Snap the latches by squeezing the base and the gold shell together at the two latch spots (front and
   back, about 12 mm right of the middle).** Don't push on the middle of the base: the board is already clamped and
   the snap force should go clip-to-shell, not through the screen.
7. **Cable clip:** drop it onto the rail on the left face, below the USB-C recess, until it sits on the
   stop. Press the cable into the U from above; it lifts out the same way.

**Opening:** case face-down on the cloth, pull the base's right end up to unsnap both latches, then swing it
off about the USB-C.

## Check and report
| | Check | Report |
|---|---|---|
| 1 | Base swings on with no rubbing or forcing at any point | smooth / rubs where |
| 2 | Both latches click, case doesn't rattle, opens by hand | click (both / one / none), rattle (yes / no), opening (easy / firm / too hard) |
| 3 | USB-C mouth flush with the face, gap even all round | sunk / level / slightly proud / clearly proud; gap even (yes / touches where) |
| 4 | USB-C opening top edge printed clean | clean / droops |
| 5 | Plug a cable in 20 times while holding the case | board moves (no / yes), latches hold |
| 6 | PWR: a clear click, the device turns on/off, no self-pressing | click (clear / mushy / none), self-press (yes / no) |
| 7 | BOOT: a paperclip reaches the button and clicks it | yes / no |
| 8 | Mics: play a song from ~1 m, LyricFlash identifies it as reliably as with the bare board | same / worse / much worse |
| 9 | Speaker at higher volume | buzz or rattle (yes / no) |
| 10 | Switch: slides end to end, knob below the bottom face, cuts power | OK / rubs / sticks out |
| 11 | Cable clip: cable presses in and lifts out by hand; a light tug doesn't slide or tip the case | OK / too tight / too loose |
| 12 | Face-down on the table: the glass doesn't touch | OK / touches |
| 13 | Touch works right to the edge, no lit pixels hidden by the lip | OK / hidden where |
