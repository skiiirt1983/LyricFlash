# Phase 1: print and check

**Plate:** `out/phase1/lyricflash_coupons.3mf` (everything already laid flat in print
orientation). v2: every object is one welded, watertight mesh (0 non-manifold edges), so no repair
should be needed. Single STLs/STEPs are in `out/phase1/<coupon>/`.

`out/phase1/report.md` lists two small overhangs on `usb_base`, both expected: the 9.5 mm bridge that forms the top edge of the opening, and the 0.4 mm ledge where the side lead-in chamfers stop. Both print without support.

**Bambu Studio, P2S, 0.4 mm nozzle:** PETG (or PLA for a quick look), 0.2 mm layers, 4 walls,
15 % gyroid, **supports off**. Leave the wall generator on **Arachne** (default). Before
printing, open **Preview** on `usb_base` and step through layers 7.9–10.7 mm: the strip above
the USB opening should show two wall lines with no gap. (I sliced it with PrusaSlicer 2.7
Arachne at the same settings: solid on every layer, and the first layer over the opening is a
9.5 mm bridge.)

Print the latches (C7) and both C2 parts in **PETG** if you can: the beams sit at 0.9–1.4 %
strain, which is close to PLA's repeated-use limit.

For each coupon, report the **Report** line. Words are fine ("tight", "loose", "touches");
no measuring needed.

---

## C1: display seat ring + base locator

Seat ring (printed lip-down) and a floor plate with three standoff cups.

1. Drop the board glass-first into the ring from the back. **Check:** it goes in by hand with no
   force, the glass sits flat on the lip all the way round, and it can't rattle much when you
   shake it.
2. Look from the front. **Check:** the lip covers only the black border, with no lit pixels
   hidden at the edge (turn the display on, white screen if possible), and touch works right to
   the edge.
3. Put the ring face-down on the table. **Check:** the glass doesn't touch the table.
4. Board glass-down on the table, press the locator onto its back. **Check:** all three cups
   drop over the standoffs at once, it only fits one way round, and nothing else touches (it
   shouldn't rock).
- **Report:** ring fit (loose / good / tight), any pixels hidden, locator seats (yes / rocks /
  won't go on).

## C2: USB-C wall (base) + glass-seat slice (top)

The real two-part arrangement: top-shell slice with the lip and two latch beams, and base sector
with the flush port wall, thin web, guide land, support pad, backstop, a standoff cup and the
spigot.

1. Board glass-first into the top slice, then push the base on from behind until both latches
   click. The receptacle has to enter the opening and the cup has to drop over the standoff.
2. **Flush:** run a fingernail across the port face. The metal mouth should be level with the
   face or *just* proud (you feel a tiny edge), **never sunk in**.
3. **Centring:** look into the opening. The gap around the metal shell should look even; it
   must not touch on any side.
4. **Web:** the strip above the opening is 0.63 mm thick at the centre. Check it printed
   complete (no holes, not drooping onto the shell).
5. Plug your cable in and out **50 times**, one-handed, with the coupon on the desk. **Check:**
   the board doesn't shift, the web doesn't crack (especially at its corners), the latches stay
   closed, and the mouth still reads flush with the plug in.
6. Squeeze the plug's overmould hard against the face. **Check:** the web doesn't flex.
7. Open the latches (pull the beam tips outward) and close them 10 times. **Check:** no
   whitening at the beam roots.
- **Report:** flush (sunk / level / slightly proud / clearly proud), gap even (yes / touches at
  top / bottom / left / right), web (OK / cracked / drooped), board movement on plug-in (none /
  some), latches (hold / pop open), and your cable's overmould height if it hits the top shell.

## C3: battery pocket tray

1. **Check:** the cell drops in flat by hand, with a visible gap all round.
2. Push the cell to the far end. **Check:** the plug and leads fit in the 10 mm lead space
   without a sharp bend, and the leads leave through the slot.
3. Lift the cell out with one finger in the notch.
4. Stand the cell on its long edge and slide it into the C-gauge on the end (12.0 gap).
   **Check:** it slides freely.
- **Report:** side gap (none / ~1 mm / more), lead space OK?, gauge free / tight.

## C4: switch pocket + knob slot

Three pockets for knob length 3 / 4 / 5 mm (K3 / K4 / K5).

1. Slide the switch sideways into each pocket from the open side, knob through the channel.
   **Check:** it goes in by hand, and the end lips hold it.
2. **Check:** in the right pocket the knob tip sits just *below* the outer face (0.3–0.5 mm).
   Rub a thumb over it: it shouldn't catch.
3. Move the knob end to end with a fingernail. **Check:** the full 2 mm travel, no rubbing.
4. **Check:** solder blobs on the pins clear the bottom.
- **Report:** which K fits, tight / loose, any rubbing.

## C5: mic ducts (base)

Base sector like C2, with a collar around each mic (crush rib onto the PCB) and a Ø1.2 duct out
through the wall. Use the **C2 top slice** to clamp the board.

1. Assemble with the C2 top slice. **Check:** the latches still close (the crush ribs add a little
   force), and the board isn't bowed.
2. Open it again and look at the PCB around each mic. **Check:** there's a continuous ring mark
   from the crush rib all the way round.
3. Hold a light behind the board side. **Check:** you can see light through each duct exit hole
   (the duct is open).
4. Run LyricFlash with the coupon fitted and play a song from a phone ~1 m away. **Check:** it
   identifies as reliably as the bare board. Then tape over one hole, then the other: detection
   should get worse (that proves the ducts carry the sound).
- **Report:** ring mark complete (yes / gaps where), latches close (yes / hard), recognition (same
  as bare / worse / much worse).

## C6: PWR flexure (base)

Base sector around KEY2 (PWR). The beam is the inner 1.2 mm of the wall, 10 mm long, standing on
the build plate, with a 0.5 mm gap behind it and nothing above it, so no part of it prints in
mid-air (v2: the first version had floating strips). You press it through a window in the outer
skin on a pad sunk 1.5 mm, and a nub on its inside pushes the button.

The full-height beam is stiffer than a short one: estimated **~8 N** to click (beam ~6 N + switch).
If that's too hard, the fix is one number: `FLEXURE_THK` 1.2 -> 1.0 gives ~5 N.

1. Board glass-down on the table, press the coupon onto its back (two cups on the standoffs).
2. **Check:** at rest the nub doesn't press the button (the board doesn't power on or off by
   itself).
3. Press the pad. **Check:** a clear click, and the device turns on/off. It should need a
   deliberate press, not a brush.
4. Press 50 times. **Check:** the beam springs back, with no whitening at its root.
- **Report:** click (clear / mushy / none), force (light / good / too hard), self-pressing (yes /
  no), any cracking.

## C7: snap latch pairs (0.15 / 0.25 / 0.35)

1. Snap each pair together, then pull it apart 10 times.
- **Report**, for each clearance: snap force (easy / firm / very hard), rattle when closed (yes /
  no), any whitening.

## C8: speaker pocket + grille

1. Press the speaker in, cone down onto the grille. **Check:** it goes in by hand and the side
   ribs hold it (turn it over: it stays).
2. **Check:** the front frame sits on the seal rib all the way round (no rocking).
3. **Check:** the leads come out of the middle of the 20 mm end and through the end slot without being pinched.
4. Plug the speaker into **H3 (SPK)** and play something. **Check:** no buzz or rattle at higher
   volume.
- **Report:** fit (loose / good / tight), corners (OK / the speaker corners are rounder or
  sharper than the pocket), lead exit side, any buzz.
