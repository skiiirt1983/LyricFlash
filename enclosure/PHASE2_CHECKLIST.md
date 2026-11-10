# Phase 2: full case v5 (20° tilt, no button access), print and check

One file: `out/phase2/lyricflash_case_project.3mf`. Everything is flat on the plate, fitted together in
the **Assembly view**, and pre-assigned to a filament: **slot 1 = Iridium Gold PLA Metal**, **slot 2 =
black PETG**. If the parts don't all fit on one plate, Bambu Studio puts the extras on a second plate.

| Part | Filament | Orientation on the plate |
|---|---|---|
| `top_shell` | gold (1) | face-down: the top face (with the screen ring) on the plate |
| `base` | black PETG (2) | floor down |
| `clip_front`, `clip_back` | black PETG (2), springy | flat |

**Settings:** 0.2 mm layers, 4 walls, 15 % gyroid, 0.4 mm nozzle.
- **Base: supports OFF.** Nothing inside hangs over the battery any more (v3).
- **Top shell, pin, clips, cable clip: supports OFF.**
- **USB-C opening:** with the tilt, its top edge prints as a sloped ceiling. **Paint a support enforcer on
  the opening's top edge** (that clears Bambu's floating-region warning). Clean the support out fully after
  printing and check the bare board's USB-C socket slides into the opening freely before assembly.

`out/phase2/report.md` has every check: board interference (none), the tilt-in swing (PASS), the layer check,
the centre of gravity (tips at 54.3°, PLAN asks for > 30°).

## Before assembly
- Check the USB-C opening and both mic holes (Ø2.0, inside the USB-C recess, either side of the port) are clear.
- There is no access to the PWR or BOOT buttons (v5, Paul: not used). The board powers up from USB or the
  battery; the underside switch is the off switch. To flash in recovery mode, open the case.
- **Switch (optional):** solder the SS-12D00 into the battery's red lead first. Slide it into the floor
  pocket at the back-right (behind the speaker) from the battery side, knob down through the slot. Nothing is pressed in: it slides
  sideways and the end lips hold it, as on C4.

## Assembly (no pressing on the screen, ever)
1. **Battery:** lay it flat in its pocket, long side front-to-back, leads at the back-left corner, over
   the low wall on its left. Its right side closes the switch pocket.
2. **Latch clips (before the speaker):** slide each clip sideways into its channel, hook facing the outer wall, beam pointing up. The 6 mm clip
   goes on the back wall; the 4 mm clip on the right wall's front corner, sliding in from the speaker side
   (that's why it goes in first). No force: 0.25 mm per side.
3. **Speaker:** slide it down into the pocket against the right wall, cone toward the grille, leads at the
   back end and up through the slot. The side ribs hold it, as on C8.
4. **Board:** put a soft cloth on the table, put the gold top shell face-down on it, and drop the board in
   glass-first. The glass rests on the lip. Plug in the battery (J1) and the speaker (H3).
5. **Base:** hold it upside down over the board. Hook the USB-C opening over the board's USB-C receptacle
   first (left side), then swing the right side down. It should come down on its own weight until the
   latches touch, with nothing rubbing on the way (the simulation keeps 0.2 mm of air everywhere else).
   **Snap the latches by squeezing the base and the gold shell together at the two latch spots (back wall,
   right of the middle, and the right wall's front corner).** Don't push on the middle of the base: the board is already clamped and
   the snap force should go clip-to-shell, not through the screen.
6. **Cable:** take the cable round to the back and give it one turn round the cleat at the bottom of the back
   wall (under one horn, over the other). A tug on the cable is then taken by the cleat, not the USB-C plug.
   Bases without the cleat: nothing to do.

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
| 6 | Mics: play a song from ~1 m, LyricFlash identifies it as reliably as with the bare board | same / worse / much worse |
| 7 | Speaker at higher volume | buzz or rattle (yes / no) |
| 8 | Switch: slides end to end, knob below the bottom face, cuts power | OK / rubs / sticks out |
| 9 | Cable cleat: one turn holds; a light tug from any side doesn't pull on the plug, slide or tip the case | OK / slips / too fiddly |
| 10 | Face-down on the table: the glass doesn't touch | OK / touches |
| 11 | Touch works right to the edge, no lit pixels hidden by the lip | OK / hidden where |
