# Round 2: print and check

**File:** `out/round2/round2_project.3mf`. Every part is laid flat on the plate and pre-assigned:
**filament 1 = gold** (C1 seat ring only), **filament 2 = black PETG** (everything else). The
Assembly view shows how each set fits.

You don't need to reprint C2, C3 or C4: they're done or replaced. For C5, use your **C2 gold top
slice** to clamp the board, the same way you did for your "fits nice" photo.

| Coupon | What changed (from your round-1 results) | Check | Report |
|---|---|---|---|
| **C1 v2** seat ring | Two notches inside the ring for the display's flex folds (the green flex and its twin on the other side). The lip is unchanged; it fitted well on your C2+C5. | Board goes in glass-first and the glass sits **flat against the lip all round**. Swipe the screen edges. | Glass on the lip (yes/no), swiping OK (yes/no) |
| **C1 v2** locator | Opening around the battery and speaker plugs; the top cup no longer touches them. | Press onto the back of the board with the plugs **plugged in**. | Rocks (yes/no) |
| **C5 v2** mics | Crush rib 0.35 tall (was 0.12) and thinner, so it squashes. | Assemble with the C2 top slice, open again, and look for the ring mark on the PCB. | Ring mark complete / gaps |
| **C6 v2** PWR | Beam 0.9 thick (was 1.2), nub 0.2 from the button (was 0.3). | Press the pad. | Click (clear/mushy), force (light/good/hard) |
| **C7 v2** latches | Play at the hook is now **P1** (0.1 preload, pulls tight), **P0** (zero play) or **L1** (0.1 play). v1 had 0.15–0.35 play, which was the rattle. | Snap each pair, wiggle it, pull it apart. | Which one is tight without being too hard |
| **C8 v2** speaker | Pocket +0.4 per side (was 0.3), grip ribs 0.15 (were 0.25). | Push the speaker in. | Fit (loose/good/tight) |
| **C9** USB gauge | New (same as before). | Press over the USB-C on the bare board. | Tightest letter that goes on by hand; centred? flush? |

## C10: tilt-in USB-C port + backstop key (`out/round3/C10_tilt_port.3mf`)

Replaces C2 and C9. Port sizes = your C9-C opening. The pocket is longer at the back, and the
backstop is now a separate **key** (blue in the pictures) because a rigid backstop can't swing in
past the port. The whole sequence is proven collision-free against the STEP board by
`simulate_assembly.py` (report: `out/round3/assembly_sim.md`).

1. Board glass-first into the gold top slice.
2. **Hook the black base onto the USB-C at an angle** (port into the opening), then **swing it
   closed**. It should close without forcing (picture `C10_1_hook_on.png` / `C10_2_swing_closed.png`).
3. **Push the key up** through the slot in the base floor until its bottom is flush. Its three thin
   ribs touch the back of the port and may squash a little the first time; that's intended.
4. Check: port centred in the opening, mouth flush with the face, a plug goes in cleanly, and the
   board doesn't move when you push a plug in.
5. To take it apart: pull the key out with a fingernail (groove on its bottom), swing the base open.
- **Report:** closes without force (yes/no), key goes in (easy/tight/won't), port centred and flush,
  board moves on plug-in (yes/no).
