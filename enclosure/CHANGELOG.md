# Enclosure changelog

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
