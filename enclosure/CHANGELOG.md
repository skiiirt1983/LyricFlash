# Enclosure changelog

## Phase 2 v10.5 (2026-11-11)
- Paul: the cleat horns need to be bigger, with more to hook the cable onto. Horns 13 mm each side of the stem
  (was 9), so 32 mm tip to tip (was 24). The turned-up ears are 3.5 mm tall x 2 mm (were 2 x 1.6). The gap behind
  the horns for the cable is 5.5 mm (was 5), and the horn tips are 2.5 mm thick (were 2). The horn tops are at
  16 mm (were 14) so the longer 45 deg undersides stay 0.5 mm off the desk. Prints with no support (checked on
  the ribbed, panel and smooth bases). The no-cleat bases are unchanged.
- PLA bases (Paul: an extra print profile for people who don't want PETG). make_pla_bases.py writes
  base_*_pla.stl for all six styles. The only change is the base magnet pockets at dia 4.20, the PLA fit from Paul's
  C11 coupon (#3, as in the PLA shell), instead of the PETG dia 4.10. Measured in the STLs: 4.20 in all four
  corners. Watertight, one body, layer check as the PETG bases. Print with X-Y hole compensation 0 (the larger
  pockets are already in the model).


## Phase 2 v10.4 (2026-11-11)
- Paul: the three plate screws were very hard to line up (two would, the third wouldn't). Measured in the
  assembled model first: every tab hole is exactly on its post's pilot axis (offset 0.000 mm, angle 0.000 deg, both
  20 deg from vertical along the board normal), spacings match (A-B 45.962, A-C 36.800, B-C 18.405 mm) and every
  tab sits on its post with 0.000 mm gap. So the cause was the tolerance stack plus over-constraint. Board standoffs
  are +/-0.1. The M2 screws have +/-0.15 in their 2.3 mm plate holes. PETG shrink over 46 mm is 0.15-0.25 on the
  plate and again on the base posts. The 40-55 mm tall posts drift 0.1-0.2 at the top. The 20 deg post tops print
  as 0.2 stair-steps. And the USB-C web (zero gap) and guide (+/-0.10) already fix the board. That's about +/-0.4
  typical (up to ~0.9) against +/-0.2-0.3 of capture per screw, with the USB-C and three screws all fixing the board.
- **Two screws** (Paul installed it with two and it's solid): tab C, BF (-17.5, 18.3) on the back USB-C side, is
  gone with its post and pilot hole. Kept: A front-left BF (-17.5, -18.5) and B back-centre BF (0, 24).
- Tab B: round 2.9 mm hole (sets the plate's position). Tab A: **2.9 mm slot, +/-0.6 mm along the line to B**,
  which soaks up the shrink between the plate and the posts. Holes and posts stay coaxial along the board normal.
- **Marking**: raised (0.4 mm) arrow + FRONT and arrow + USB-C on the countersink face, the face you look at on the
  bench. It reads correctly when the plate is the right way up. A physical key isn't possible on the board side:
  where the plate touches the board, the board's back is mirror-symmetric (standoffs, header, J1/H3), so a flipped
  plate still seats. In the base it only goes one way (flipped, tab A lands over the battery with no post).
- **Switch**: moved 4 mm back (Y 0 -> 4, still under the USB-C bay) into the room post C left. The gap to the
  front-left post grows from 5.3 to 9.3 mm, and the back side is open to the corner. 0.6 mm x 45 deg lead-in
  round the pocket mouth.
- **C12 carrier coupon** (coupon_carrier.py): the plate plus the two real posts on a bar, to try the screws before
  a full base.
- Checks: no interference; swing PASS; shell drop PASS; battery drop PASS; plate and coupon clean in the layer check.

## Phase 2 v10.3 (2026-11-11)
- Paul: the floor logo wasn't legible printed. The "LYRIC FLASH" text is gone: the floor now carries only the mark
  (rounded square, lightning bolt, music notes). Logo-only source kept as assets/logo_floor_mark.stl (the 3 big
  pieces of Paul's cut, original size) so the text can't come back by accident.
- Refit with concepts/fit_logo.py (now tries 0 and 90 deg, keeps MARGIN 1.5, thickens thin strokes, sets the
  depth): **21.7 x 21.7 mm** (scale 1.126 of Paul's mark; the square shape gives the same size at either
  rotation), centred in the open floor at (-9.5, 1.5), 1.52 mm from the nearest wall, post, block or the switch.
- **1.0 mm deep** (5 layers at 0.2; was 0.5). The floor is 1.6 mm there, so 0.6 mm stays under it.
- Thinnest stroke, the square's outline: **0.92 mm** after scaling, so no thickening was needed (>= 0.8). The
  bolt's tip and one note corner come to a point, as drawn.
- Asset: assets/logo_floor_mark_v10_3.stl (cut from z 0.6 to 1.65).

## Phase 2 v10.2 (2026-11-11)
- Magnet pocket fits from Paul's C11 coupon (coupon_magnets.py): base (PETG, pockets leaning 20 deg) stays at
  dia 4.10 (#2); shell (PLA, pockets straight up) goes to dia 4.20 (#3). At 4.10 the PLA pocket wouldn't let the
  magnet sit flush, and at 4.00 it wouldn't go in. Separate MAGNET_FIT / MAGNET_FIT_SHELL.

## Phase 2 v10.1 (2026-11-11)
- Paul: no nuts. The M2.5 x 6 screws self-tap into a 2.2 mm pilot hole in each post (PETG); the nut slots and
  their bridged ceilings are gone. The base's only flagged layers are the known USB-C mouth spots again.
- Paul: the base magnet pockets didn't work (photo, all four corners). v10's corner bosses were a pure 45 deg cone
  that narrowed so fast the pocket broke out through its side. Now each corner is a solid block: straight sides
  with 1.2 mm round the magnet and below it (deeper toward the middle, since the pocket leans with the 20 deg
  tilt), then a 45 deg underside into the walls. The block's inner corner is trimmed on the diagonal so the
  board's plate tabs keep >= 0.2 mm on the way in. Checked: a full 1 mm shell of plastic round every pocket.
- Cell 0.5 mm further left so the right corner blocks stay clear of its way in. Logo refit (14.7 x 23.4 mm).

## Phase 2 v10 (2026-11-11)
Paul's new rules (DESIGN_RULES.md): never force the expensive parts, but give cheap handled parts the best fit;
screw the board down; make the shell-to-base joint clean and solid.
- **Board screwed down via a printed carrier plate.** The plate takes the board's three M2 standoffs (M2 x 3 flat
  heads from below, countersunk). Three tabs past the board's edge screw onto square posts in the base (M2.5 x 6 pan
  head into an M2.5 nut in a slot in each post), so every screw is reached from straight above, never through the
  screen. The posts are tied into the nearest wall; the nut slots open to the front so their ceilings print as
  5 mm bridges. The board ledges are gone. The board is 3.3 mm higher so the plate clears the battery: case 47.8 mm
  at the front, 70.7 at the back (footprint unchanged, 52.9 x 62.8).
- **Shell: latch clips replaced by magnets and an alignment rim.** Four 4 x 2 magnet pairs sit in the corners
  (bosses with 45 deg undersides in the base, pockets in the shell). A 1.2 mm rim hangs 2.5 mm below the shell
  inside the base walls, 0.15 mm clear, so the seam closes square and can't shift. The shell goes on last, straight
  down along the screen's normal. The hinge relief in the shell is gone (the base no longer swings past it).
- **Battery moved 5.6 mm left** (the ledges no longer constrain it) so both right corners take magnets.
- **Switch moved to the left wall, under the USB-C.** Pressed into a pocket sized to Paul's measured body
  (8.75 x 3.97 x 3.94, +0.05 a side), knob out through a 1 mm skin (sticks out ~1.85 mm), flat patch in the
  texture round it. The floor pocket is gone. The old floor pocket bound because it left 2 mm for the pins; the
  wired pins need 8 mm.
- Logo refit into the floor that's left (concepts/fit_logo.py): 14.7 x 23.3 mm, centred.
- New checks (simulate_v10.py): board + plate swung into the base about the USB-C, and the shell dropped on along
  the normal. Both keep >= 0.2 mm to the board, screen and plate except designed contacts (USB-C web; plate tabs
  settling onto the posts beside the hinge; seam faces). The shell stays >= 0.25 mm from the screen all the way
  down; the rim guides at 0.15 mm on the base wall.
- **Bezel lip 0.25 mm clear of the glass** (LIP_GAP). The board is screwed down now, so the shell no longer
  clamps it; the magnets' pull goes through the seam, never onto the screen. Shell 0.25 mm taller to keep the
  lip 1.0 mm thick.

## Phase 2 v9 (2026-11-10)
- Paul: the flat battery couldn't get past the board ledges. The two front ledges and the back one hung over
  its ends, and the old checks never tested the battery's way in (my miss). The board's underside leaves no
  other place to support its front, so the ledges stay where they are and the battery moves:
  - **The cell stands on its thin edge along the right wall** (pocket 11.3 x 58 x 36.5, side wall 16 mm). That
    column is clear of every ledge, the latch bosses and the USB-C insert, so it drops straight in.
  - **Speaker removed** (Paul): no pocket, divider, ribs or grille; the right wall is plain.
  - **Base taller**: the board sits above the standing cell. The case is 44.5 mm at the front and 67.4 at the
    back (was 29.9 / 52.7), so the base front is about 42 mm, as Paul asked. Footprint unchanged, 52.9 x 62.8.
    BOARD_Z_EXTRA is now 0, because the ledges no longer sit over the battery.
- Both latch clips are the same 6 mm part (the front one was 4 mm). The front clip moved to the front wall at X -15
  and the back clip to X -4. Both are off the cell's column and both are well away from the USB-C hinge line, so
  the swing brings them straight down onto their catches.
- Switch pocket +0.2 mm all round (9.7 x 4.7) and +0.2 mm height clearance: the body wouldn't slide in (Paul).
  It now sits left of the cell at the back, still open toward the battery, which keeps it in.
- New check, every build: the battery drop path. The real 10.3 x 56 x 34.5 cell is lowered straight into its
  pocket: centred with 0.2 mm air, and against either pocket side. PASS.
- Checks: interference none (board, parts); swing PASS; drop path PASS; base layer check shows only the USB-C
  mouth's top edge (painted support, as before); top shell 3 short latch-pocket roof spots, the same kind as
  before; tips at 33.0 deg (PLAN > 30; was 53.8, because the case is taller).

## Phase 2 v8 (2026-11-10)
- Paul: the side cable catch didn't really work (a pull along the cable just slides through). Replaced by a
  cable cleat on the back, low near the desk: a 6 mm stem standing on the plate, horns 24 mm across with
  45 deg undersides and turned-up tips, 5 mm gap behind them for the cable. One turn round it takes the pull.
  Prints with no support. The side catch is off (CABLE_CLIP = False).
- Paul: every style also without the cleat: base_{ribbed,panels,smooth}_nocleat.stl. Print pack now has
  eight plates: top shell, three bases with the cleat, three without, latch clips.
- Checks: swing PASS; all six bases watertight, no floating spots (USB-C mouth painted), logo intact.

## Phase 2 v7 (2026-11-10)
- Paul printed the base: the internals were very flimsy (pieces round the speaker broke taking it off the
  plate). Thicker everywhere: outer wall 2.0 -> 2.4 (6 perimeters), speaker divider 1.2 -> 2.0, speaker end
  walls 1.2 -> 2.4, lead slot 4.0 -> 3.0 (wider posts either side), grip rib 0.8 -> 1.6 wide, battery low
  walls 1.2 -> 2.0, board ledges 5 -> 6 wide, USB-C guide walls 1.6 -> 2.0. Floor stays 1.6 (logo unchanged).
- Corner radius 8 -> 6.5 so the 56 x 34.5 cell keeps 0.8 mm air at the corners with the thicker wall.
- Case 52.9 x 62.8 (was 62.0), front 29.9 / back 52.7. Checks: board none, swing PASS, all three bases
  watertight, no floating spots (USB-C mouth painted), logo intact, tips at 53.6 deg. Print pack rebuilt.

## Phase 2 v6.5 (2026-11-10)
- Paul's logo + "LYRIC FLASH" (his Bambu edit of base_panels.stl) applied to all three bases. Taken as the
  difference between his file and mine (84.6 mm3, 0.5 mm deep in the top of the floor, inside under the
  battery), stored in assets/logo_floor_cut.stl, and cut with an exact mesh boolean into every exported base
  (STLs and the project 3MF; lfcase/logo.py). FLOOR_LOGO = "" turns it off. Panel base matches his file to
  0.1 mm3. Base STLs now sit exactly on z = 0.

## Phase 2 v6.4 (2026-11-10)
- Paul: a latch pocket broke through the top shell's first layer (the front pocket, leaning into the top-edge
  chamfer). Pockets now stay >= 0.6 mm (3 layers) inside every outside face of the top shell, and their roof
  slopes with the hook's 30 deg lead-in toward the wall. First three print layers: screen opening only.
- Paul: plain (untextured) base added: `out/phase2/stl/base_smooth.stl`, with the built-in cable clip.
- Checks: no part overlaps; swing PASS; all three bases watertight, no floating spots (USB-C mouth painted).

## Phase 2 v6.3 (2026-11-10)
- Paul: cable clip built into the base (matte PETG sliding joints bind and break; separate parts were never
  wanted for colour). U-channel along Y on the left wall at desk height, opening up; the wall is its inner
  side, only the 1.2 mm outer jaw flexes (mouth 0.92 x OD = 3.5 for a 3.8 cable, ~2 % strain). Sticks out
  4.95 mm (was ~11 with rail + clip). 1.5 mm plain border round it, texture resumes above on a 45 deg slope.
  Rail, stop and the separate clip part removed. Wall behind the channel 1.65 mm. Checks: board none, swing
  PASS, no new layer flags on either base.

## Phase 2 v6.2 (2026-11-10)
- Paul: less blank round the USB-C side. Ribs (and panels) now run right up to the USB-C recess and the cable
  rail. The rail runs from the stop up to the recess floor; it stands 2.5 off the wall so the clip sits 1.3
  off it and slides down from above (through the recess) with 0.3 mm over the texture. Clip reprint needed;
  one clip fits both bases. (Texturing the clip itself was dropped, Paul.)
- Panel base: panels wrap the rounded corners too (one curved panel per corner, same 45 deg edges); one
  groove line all round. Checks (both bases): board none, swing PASS; panel base no floating spots.

## Phase 2 v6.1 (2026-11-10)
- Paul: an alternative base with raised panels (his second picture): `out/phase2/stl/base_panels.stl`.
  Panels ~13 mm wide in two rows on the four flat sides, 0.8 proud, 45 deg edges, 1.2 mm grooves; rounded
  corners plain; skips the USB-C recess and the cable rail (45 deg cut above it); grille drilled through.
  Same top shell and small parts. Checks: board none, swing PASS, only the USB-C mouth flagged (painted).
  TEXTURE = "ribs" | "panels" | None in params.py picks the base in the project 3MF (ribs by default).

## Phase 2 v6 (2026-11-10)
- Paul: ribbed / japandi outside on the base (BumpMesh gave poor results). Modelled, not a texture: vertical
  half-round flutes R1.5, 1.0 proud, 3.0 pitch, following the rounded corners, from the desk to a 1.5 mm
  plain band under the gold top. Skipped over the USB-C recess and round the cable rail (the ribs above the
  clip start with a 45 deg underside); the speaker grille is drilled through them. RIBS = False turns it off.
- Base now 56.1 x 64.0 at the floor (+2 mm each way). Other parts unchanged. Checks: board none, swing PASS,
  no floating spots except the USB-C mouth (painted).

## Phase 2 v5.4 (2026-11-10)
- Paul: a leaning spike in the slicer. The latch boss reached past the battery's edge and ran to the floor
  as a thin leaning column; bosses now get the 45 deg underside across their full width.
- Front latch moved from the front wall to the right wall's front corner: at the low front wall the battery
  clearance left the clip channel's inner wall floating. There it stands on the floor; clip 4 mm wide (the
  straight wall between speaker and corner is 6.4 mm), about 2/3 of the C7 snap force. Back latch unchanged.
  The clip goes in before the speaker (it slides in from that side).
- Ledges get only the front-to-back 45 deg trim (the sideways one left a strip).
- Checks: board none; swing PASS; base: no floating spots except the USB-C mouth (painted support); gold
  shell skin intact round the new pocket; tips at 54.3 deg.

## Phase 2 v5.3 (2026-11-10)
- Paul: battery lead gap closed; the low wall runs the full length (leads leave the cell ~6.75 up, over the
  5.6 wall top).
- Paul: speaker grip rib on the lead end removed (the lead slot left its top as a free-standing fin). The
  -Y rib still pushes the speaker onto the lead-end wall with the C8 0.15 grip.
- USB-C mouth: Paul painted support on its top edge in Bambu; that cleared the floating-region warning.

## Phase 2 v5.2 (2026-11-10)
- Paul: Bambu reported floating regions on the base. Cause: the two front board ledges. With the battery
  running to the front wall, the battery's 45 deg clearance cut ran parallel to the ledges' own underside and
  left floating slivers that couldn't carry the board. Ledges over the battery are now solid wedges whose
  underside is that 45 deg cut, ended square where thinner than 1 mm; the latch bosses likewise.
- Board raised 2.5 mm (Paul) so those wedges reach 1.3 mm under the PCB: front 30.0 / back 52.6 mm,
  tips at 54.3 deg. The USB-C insert needs no support (45 deg underside since v3).

## Phase 2 v5 (2026-11-10)
- Paul: no button access. PWR and BOOT aren't used by his firmware, so the PWR pin, its guide tube and
  finger dimple, and the BOOT pinhole and guide are gone (BUTTON_ACCESS = False; the code stays for later).
  The right wall and its corners are plain.
- Checks: board none; swing PASS; base needs no supports; tips at 55.9 deg.

## Phase 2 v4 (2026-11-10)
- Tilt 12 -> 20 deg (Paul: 12 wasn't enough). Same 52.9 x 62 footprint; front 27.5 / back 50.1 mm; board
  centre 30.4 mm up; tips at 55.9 deg. Ledges re-picked (same angles).
- Paul: "the other mic box" was the C10 USB-C insert, which spanned the whole +/-14 mm recess and reached
  6.5 mm into the case behind both mics. Behind the port wall it now keeps only the receptacle guide
  (+/-6.5 mm).
- Paul: odd edge in the slicer = the 45 deg fill under that insert, built from 0.4 mm-stepped copies
  (sawtooth edge, creases). Now an exact sweep: flat 45 deg faces.
- Latch clips: anchor bar 4.0 -> 2.6 and inner shoulder 1.6 -> 1.0 (the lower front seam at 20 deg put the
  front clip into the battery space). Beam and hook unchanged (C7 v5).
- Checks: board none; swing PASS; base still needs no supports; mic paths clear.

## Phase 2 v3 (2026-11-10)
- Paul: a hole through the base wall beside the PWR tube. The finger dimple's cone ran along the pin axis,
  34 deg to the wall, so one side cut through the 2 mm wall. Now on the wall normal, 1.0 deep, >= 1.0 left.
- Paul: mic boxes removed (their supports turned the whole left wall into a support). Plain D2.0 teardrop
  holes (was D1.2) straight through the USB recess wall, 6.1 / 6.3 mm from the mics, path checked clear.
- No supports in the base: the USB-C insert's underside is filled down to the wall at 45 deg (only from the
  material under the receptacle, so the opening keeps its 0.55 clearance), the recess floor/cheeks reach
  under the port wall, the BOOT guide is 3 mm (it sat 0.1 above the speaker pocket).
- Checks: board none, swing PASS, tips at 58.1 deg.

## Phase 2 v2: layout B (2026-11-09)
- Paul: v1 looked off-centre and bigger than pictured. v1 was 70.8 x 53.7 with the board 9 mm left of centre
  (flush USB-C on the left, 58 mm battery longer than the board, speaker at the battery's right end). The
  65 x 57 quoted at the tilt question came from a layout study that put the speaker above the battery, where
  it would have hit the top shell; v1 moved it beside the battery without saying so (my miss).
- Layout B (Paul): battery front-to-back, speaker standing against the right wall under the board's right
  side, board centred left-right (BOARD_X 0). Case 52.9 x 62.0 (PLAN ~62 x 62), front 28.8 / back 42.0 mm
  (board 3.4 mm higher so the speaker fits under it). PWR and BOOT guides now 7.7 mm (were ~24).
- Board height from a new solver (lowest position with 0.5 mm air to battery, speaker and floor).
- Speaker divider lowered to 16 mm (full height came 0.01 from a standoff in the swing); battery held on its
  left by a low wall with a 9 mm lead gap at the back corner; switch pocket moved behind the speaker.
- Ledges and the USB recess floor/cheeks get 45 deg undersides over the battery / cavity.
- Checks: board interference none; tilt-in swing PASS; tips at 58.1 deg; layer check as before.

## Phase 2 v1 (2026-11-09)
- 12 deg tilt, two main parts, internal supports allowed for hidden features (Paul). Speaker beside the battery.
- USB-C: C10 port geometry set into a recess in the left wall (bay +/-14 mm), tied in with a floor and cheeks.
- Mics: open ducts, no collar or crush rib (Paul). The tilt-in swing moves each collar ~1.4 mm sideways, so a
  C5-style sealed collar would sit on a standoff / small parts. A bar under each mic (top 0.4 below the lid),
  a vertical hole under the port, and a D1.2 teardrop duct that exits inside the USB bay. 7.7 / 7.9 mm long.
- PWR: KEY2 is ~14 mm inside the case wall, so a separate gold push pin in a guide tube (rest gap 0.35, loose,
  no preload), exits the back-right corner in a finger dimple.
- BOOT: D1.6 paperclip guide on KEY1's axis, exits the front-right corner (PLAN 3.5).
- Speaker: C8 numbers, standing, fires out the right wall (53 teardrop holes), seal rib, grip ribs on the
  end walls, lead slot toward H3.
- Switch: underside pocket (Paul), the C4 K3 cell in the floor at the back-left, open toward the battery.
- Latches: two separate PETG clips with the C7 v5 notch-1 geometry (hook 1.0, 70 deg, guides 0.3, play
  0.1), printed flat (layers along the beam, as promised after C2's latch broke). Each slides sideways into
  a channel on the front / back wall; the gold top has a bridged catch. At X 12.5, clear of the PWR/BOOT
  paths and the plug zones.
- Cable: separate clip on a vertical dovetail on the left face at desk height (PLAN 3.2 clip; no underside
  channel: the battery fills the floor). U-channel opening up, mouth 0.85 x OD.
- Board ledges at -70 / -110 / 135 deg, picked so they keep 0.25 mm off every part along the whole swing
  (-50 came 0.06 from a standoff).
- Hinge relief: the base wall outboard of the hinge line rises as the base swings, so the top shell is cut
  back along that path with 0.2 mm air (a small shadow line along the left seam).
- Edges: 45 deg chamfers on the top edge (1.5) and bottom edge (1.0): both are on the print bed, where a
  3 mm fillet (PLAN 3.1) would overhang. Vertical corners R8.
- Checks: 0 board interference (except the C8 grip ribs on the speaker, by design); tilt-in swing PASS
  (mesh-sample check, 0.2 mm air rule, designed contacts labelled); layer check; CG/tip 59.6 deg.

## Phase 1 complete (2026-11-09)
- C7 v5: 1 notch (hook 1.0, 70 deg catch, guides 0.3, play 0.1) = best snap -> LATCH_* params.
- All coupon questions answered; see params.py for the confirmed values. Next: Phase 2 (full case).

## Latches (2026-11-09)
- C7 v3 (hook 1.0, 80 deg, guides 0.15): needed pliers / wouldn't assemble.
- C7 v4 (hook 0.8, 45/55/65 deg, guides 0.3): 65 deg tightest but very little resistance; same
  after 50 pulls (durable). -> C7 v5: hook 1.0, 70/74/78 deg, guides 0.3, play 0.1.

## Labels (2026-11-09)
- Paul: 3.5 mm text unreadable; ~10 mm is legible with a 0.4 mm nozzle (depth 0.4). Small parts get countable edge
  notches instead; matching halves placed next to each other on the plate.

## C1 decision (2026-11-09)
- Paul, looking at C1 v2 from the front: "this look is good" -> lip design final (1.0 over the
  glass, 1.0 proud, flex-fold notches). Bore clearance 0.3 -> 0.7 on diameter (no force).

## Round 2 results (2026-11-09, printed after round 3)
- Paul broke the display during the C9 test by pushing a gauge on: my C9 instructions asked for a
  press-on that the geometry could not allow. New rule: NO press/force fits anywhere; assembly
  simulation must also show >= 0.2 mm air along every path (except designed contacts).
- C1 ring: glass "just slides through" the bore -> open the bore (pending Paul's clarification of
  the 0.1-0.2 mm gap). C1 locator perfect. C5 good. C6 clicky/responsive. C8 tight but very good.
  C9: wide enough, not long enough (resolved: no backstop, pocket open at the back).
- C7 v2: "incredibly loose"; photo shows the T rocking in its shallow 1.5 mm pocket so the hook
  tips out instead of the beam bending. -> C7 v3: 6 mm guide posts, beam room only on the catch
  side, hook 1.0, catch 80 deg; labels >= 3.5 mm (10 pt) bold (Paul: v2 text unreadable).

## Round 3 results (2026-11-09)
- C10: tilt-in assembly fits on the real board. Key too hard to remove and loose -> Paul: no
  backstop (USB_BACKSTOP_KEY = False); board held firmly instead (port-side web contact, far-side
  PCB-edge stop 0.10, Z clamp, latches).
- Plugs + bent wires need 7 mm below the PCB back (Paul) -> MIN_DEPTH_BELOW_PCB 7.5 for Phase 2.

## Tilt-in assembly (2026-11-09)
- Assembly-path simulation (lfcase/assembly.py, simulate_assembly.py): straight-up base collides
  with the USB-C after 0.5 mm (= the C2/C9 misfit). A swinging base can't carry a rigid backstop
  (it would pass through the port). Solution: base hooks onto the port and swings closed about a
  hinge line just outside it (X -24.2, Z 5.5); flat seats on the standoffs; no spigot within
  +/-45 deg of the port; backstop = separate key pushed up through a floor slot, bearing on a
  buttress wall. Every step 0..25 deg + key insertion: zero collision. Coupon C10.
- Paul: C9 box pinched at the back end -> key ribs reach 0.1 behind the STEP rear, key face 0.5.

## C9 result (2026-11-09)
- Gauge C: the opening (0.55/side) is perfectly sized; the pocket behind it is still too tight.
  A and B way too tight. It only goes on angled: with the web above the opening, a base moving straight up
  must pass the web over the port (root cause of the C2 misfit too). -> tilt-in (hinge) assembly,
  proven with an assembly-path simulation before printing.

## Round 2 (after Paul's round-1 table, 2026-11-09)
- Results: C1 glass didn't reach the lip (flex folds stick out past the STEP: bore had 0.13 on
  paper), lip too much to swipe, locator rocks (top cup 0.08 mm from the SPK socket; plugs in);
  C2 doesn't fit; C3 good; C4 K3 pocket good -> knob 3 mm; C5 collar gap (song ID OK); C6 mushy +
  too hard; C7 no tension + rattle (v1 had 0.15-0.35 axial play by design); C8 too tight.
- C2 gold top + C5 base (loose USB relief) "fits nice": lip OK when seated -> keep lip, add
  flex-fold reliefs. Fallback if C1 v2 still bad: PrintSphere-style flush glass (Paul).
- microSD stays inside (no slot).
- Changes: fold reliefs (FOLD_RELIEF_*), plug keep-out (PLUG_KEEPOUT_GROW) on all base parts,
  MIC_CRUSH 0.35, FLEXURE_THK 0.9 + FLEX_REST_GAP 0.2, C7 axial play P1/P0/L1, SPK_CLEAR 0.4.

## Phase 1 print results, round 1 (Paul 2026-11-09, gold PLA Metal)
- C2: USB-C receptacle does not fit in its base pocket; the support pad under it is not deep
  enough (C2 clearances 0.10 side / 0.10 under / 0.05 behind were too tight as printed).
  -> C9 USB fit gauge, three variants A/B/C (see params USB_GAUGE_VARIANTS).
- C2: one latch beam broke ("pretty flimsy"): upright PLA beam, bending across the layers.
  -> Phase 2: latches become separate clips printed flat in PETG (layers along the beam).
- C2: rest of the test can't be done (receptacle doesn't fit). C9 gauge replaces it; plug-in and
  flush checks move to the Phase 2 print.
- C3 battery tray: cell fits flat with room to spare; finger notch works. Leads leave the cell at
  one corner of the 34.5 mm end (that orientation lets the cell lie flattest) and went over the
  wall there, not through the centred slot -> Phase 2: wire exit at that corner. Thickness gauge
  (12.0 gap): cell fits perfectly -> BATTERY_POCKET thickness 12.0 confirmed. C3 done.

## Acceptance criterion (Paul 2026-11-09)
- A ~100 g device can't take a 5-20 N USB-C insertion one-handed on a desk. C2 test now plugs in
  while holding the coupon. Proposed PLAN 7 wording: "USB-C plugs in fully while the case is held;
  once plugged in, a gentle tug on the cable from any direction doesn't slide or tip the case on
  the desk" (the tug is what the cable clip + low exit handle). Pending Paul's OK.

## Materials, 2026-11-09
- Top shell: Iridium Gold PLA Metal. Base tray: black PETG. Coupons printed per plate in those.
- Phase 2: latch beams move to the PETG base (no flexing features in PLA).

## Phase 1 v2, 2026-11-09 (Paul: non-manifold 3MF, C6 floating regions)
- 3MF writer welds each part into one connected mesh; every object checked: 0 non-manifold edges.
- C6 flexure redesigned: beam stands on the plate (full-height inner wall layer, gap behind,
  nothing above, slot through the floor); pad and nub grow out with 45 deg undersides.
- New layer-by-layer check in report.md (floating islands, cantilevered overhangs, bridges);
  it flags the old C6 and passes all 15 v2 parts.
- USB opening top corners R0.5 (Paul's web-corner fillet); the top edge is one 8.7 mm bridge.

## Phase 1 (coupons), 2026-11-09
- Bezel option A chosen (lip, board loads from behind, base clamps it on the M2 standoffs).
- Board facts from Paul: PWR = KEY2, BOOT = KEY1, BAT = J1, SPK = H3, both mics top-port.
- Coupons C1-C8 built from params.py, checked against the STEP board, exported (STL/STEP/3MF).
- USB-C: port wall, web, guide land, pad and backstop all on the base tray (Paul's conditions 1-6).
  Seam at Z 3.0 (2.8 mm above the opening); spigot top 0.2 below the display flex folds.
  Mouth target +0.15 (see USB_STACKUP.md).
- Speaker added (30 x 20 x 6.8, Paul's listing image): pocket + grille coupon C8.
- Speaker: leads exit dead centre of a 20 mm end, mid-thickness (C8 slot spans it); placement free, no mic keep-out (plays only when mics idle).

## Phase 0, 2026-11-09
- Board STEP imported; section 4 values extracted into params.py with sources; labelled renders.
