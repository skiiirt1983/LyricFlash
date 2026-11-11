# Phase 2: full case v10 (carrier plate + screws, magnets, switch in the wall), print and check

Print pack: `out/lyricflash_print_pack.zip` (one plate per print). Filament **slot 1 = Iridium Gold PLA Metal**,
**slot 2 = black PETG**.

| Part | Filament | Orientation on the plate |
|---|---|---|
| `top_shell` | gold (1) | face-down: the top face (with the screen ring) on the plate; the rim points up |
| `base` (one of six styles) | black PETG (2) | floor down |
| `carrier_plate` | black PETG (2) | flat, board side down; the countersinks face up |

**Settings:** 0.2 mm layers, 4 walls, 15 % gyroid, 0.4 mm nozzle. Supports OFF everywhere, except:
- **USB-C opening:** paint a support enforcer on the opening's top edge, as before. Clean it out fully and check
  the bare board's USB-C socket slides into the opening freely before assembly.

**Hardware:** 3x M2 x 3 flat head (board to plate), 3x M2.5 x 6 pan head (plate to base; they self-tap into the
posts, no nuts), 8x 4 x 2 mm disc magnets.

`out/phase2/report.md` has every check: interference (none), both assembly paths (board + plate into the base,
shell onto the base: PASS), the battery drop path (PASS), the layer check, and the centre of gravity.

## Before assembly
- **Magnets:** press one into each corner pocket of the base and one into each corner pocket of the shell
  (snug fit; a drop of CA glue if one is loose). **Check polarity first:** each shell magnet must attract
  the base magnet under it. Stack the pairs before pressing them in and mark the faces that meet.
- **Switch:** solder the SS-12D00 into the battery's red lead. From inside the case, push the body into its
  pocket in the left wall under the USB-C, knob first through the slot. It's a firm press-fit (Paul's rule:
  cheap parts that get handled get the best fit). The knob should stand about 1.85 mm out of the wall and slide
  end to end.
- There is no access to the PWR or BOOT buttons (not used). The switch is the off switch.

## Assembly (no pressing on the screen, ever)
1. **Board onto the carrier plate (on the bench):** board face-down on a soft cloth. Lay the plate on its back,
   countersinks up, holes over the three standoffs. Fit the three M2 x 3 flat heads. Snug, don't crank: they
   thread into the board's own standoffs.
2. **Battery:** stand it on its thin edge and lower it straight into its slot (right of centre), leads at the back
   and at the top. Nothing is over the slot. Plug the battery into J1 on the board.
3. **Board + plate into the base:** hold the board by the plate, tilt it, and hook the USB-C into its opening in the
   left wall first. Then lower the right side down. The three tabs land on their posts.
4. **Screw the plate down:** three M2.5 x 6 pan heads through the tabs, straight down, from above. They cut their
   own thread in the posts' pilot holes: the first time is a little stiff; stop as soon as the head seats.
   Every screw is outside the board's edge.
5. **Shell:** put it straight on from above, square to the screen, until the magnets pull it down. The rim inside
   the base walls lines it up. It closes with no force on the screen: the bezel lip stands 0.25 mm clear of the glass, and the magnets pull
   through the seam.
6. **Cable:** one turn round the cleat on the back (bases with the cleat).

**Opening:** pull the shell straight up off the magnets. Then undo the three M2.5 screws and lift the board and plate
out, right side first, unhooking the USB-C last.

## Check and report
| | Check | Report |
|---|---|---|
| 1 | Board + plate go in USB-C first and settle on the posts without forcing; screws pull it down flat | OK / rubs where |
| 2 | Shell: seats flush all round, magnets hold it firmly, comes off by hand, no rattle | flush (yes / gap where), hold (firm / weak / too strong), rattle (yes / no) |
| 3 | USB-C mouth flush with the face, gap even all round | sunk / level / slightly proud / clearly proud; gap even (yes / touches where) |
| 4 | USB-C opening top edge printed clean | clean / droops |
| 5 | Plug a cable in 20 times while holding the case | board moves (no / yes), latches hold |
| 6 | Mics: play a song from ~1 m, LyricFlash identifies it as reliably as with the bare board | same / worse / much worse |
| 7 | Battery goes in and comes out by hand, no forcing; doesn't rattle when closed | OK / tight where / rattles |
| 8 | Switch: slides end to end, knob below the bottom face, cuts power | OK / rubs / sticks out |
| 9 | Cable cleat: one turn holds; a light tug from any side doesn't pull on the plug, slide or tip the case | OK / slips / too fiddly |
| 10 | Face-down on the table: the glass doesn't touch | OK / touches |
| 11 | Touch works right to the edge, no lit pixels hidden by the lip | OK / hidden where |
