# Enclosure changelog

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
