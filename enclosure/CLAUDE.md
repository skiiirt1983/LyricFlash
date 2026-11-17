# LyricFlash case (enclosure): handoff and working notes

For any Claude model starting fresh. Read this top to bottom, then `README.md`, `DESIGN_RULES.md` (Paul's rules, they win over older
notes) and the top of `CHANGELOG.md` (every decision, newest first). The owner is Paul (concise, direct updates, wants honest
status: say what was checked on the model and what has not been printed).

## What this is
A parametric, 3D-printed case for **LyricFlash**, a desk device on the **Waveshare ESP32-S3-Touch-AMOLED-1.75** (round 1.75" AMOLED).
Python/CadQuery builds it; the numbers live in `params.py` (later assignments override earlier ones: **the last block wins**).
The repo is `skiiirt1983/LyricFlash` (firmware at the root, case in `enclosure/`). **The case work is on the branch
`claude/loving-heisenberg-n1judx`, not on `main`.** An older line of the same work is on `origin/ccr-6cc70ba5-80g88r` (v10.x).
Paul has a separate private repo `skiiirt1983/project-summit` (different board and case); it has no button test data for this case.

Board: the original and **Rev 1.1** measure the same (standoffs, USB-C to standoff, diameter). Rev 1.1's PWR button position was not
measured separately: assumed the same as the STEP model.

## The design as of v11g (2026-11-17)
Two printed shells and a plate: gold **PLA Metal top shell** (filament slot 1), black **PETG base** (slot 2; a PLA-fit version of every
base exists), PETG **carrier plate**. Case 52.9 x 62.8 mm, display tilted 20 deg. Board frame: origin PCB-back centre, +Z out of the
glass, USB-C toward -X, buttons +X, header -Y.
- **Board screws to the carrier plate** (3 x M2x3 flat heads through its standoffs). The plate screws to two posts in the base (tab A
  front-left, tab B back-centre) with **M2.5 x 6 self-tapping screws, no nuts**. Tab A's hole is a **3.6 mm round hole** (tab B 2.9).
- **Base pilot holes: 2.5 mm** (`POST_PILOT_D`) with a 45 deg 0.8 funnel at the mouth (`POST_PILOT_LEAD`). Paul's C13 gauge: 2.5 best.
- **Shell to base:** four 4x2 mm magnet pairs in the corners (PETG base pockets 4.10, PLA 4.20) and a 1.2 mm alignment rim hanging
  under the shell inside the base walls, **only on the left and back** (`RIM_DROP_SIDES = ("x+","y-")`: the right and front rim was
  in the way of the SD card tab and the display flex folds). The shell goes on last, straight down the board's normal.
- **No slide switch** (deleted in v11). The battery stays plugged in; the board's own PWR key (KEY2) turns it on and off.
- **PWR button** (`lfcase/pwr_button.py`, params `PWR_*`): a printed flexure in the right wall built from the round-2 **C6 coupon**
  (Paul: clear click, responsive; beam 0.9, nub 0.2 from the plunger). A thin vertical **blade** tangent to the board at the plunger,
  anchored in the wall, bends sideways along the layers; a **nub** on its inside touches the plunger (rest gap `PWR_REST_GAP` 0.28,
  nub only 0.5 tall in the lower half of the plunger face, because the board swings in/out straight past it); a **bar** with a
  45 deg underside (so it prints without support) runs out through a tunnel in the wall to a **face** (`PWR_FACE_W` 7 wide, bottom
  BF z -6.0, head top `PWR_FACE_TOP_GAP` 2.0 under the seam, 45 deg top corners, **0.8 proud** of the wall) in a **finger scoop** (~16 mm,
  1.0 deep). The blade thickness is solved for the target beam force (`PWR_TARGET_FORCE` 2.4 N, `PWR_BLADE_THK = None` -> ~0.65 mm);
  total press force estimate ~4.9 N including the switch's own ~2.5 N (the coupon was ~4.0). **These forces are estimates (Euler-
  Bernoulli, PETG E 1500 MPa), not measured.** The face is a skewed shape (axis is 34 deg to the wall) and each mm of face below the
  plunger makes the blade a mm taller and stiffer. The opening cannot just move: the plunger axis fixes where it leaves the wall.
- **Shell bore reliefs** (`FOLD_RELIEFS`, angle/width/depth): back 90 deg 16 x 1.2 (unchanged), front 270 deg 16 x 1.8, right 0 deg
  17.5 x 0.6 for the SD card's tab (the shell wall is only 1.6 thick there; 0.6 keeps 1.0 of skin).
- Cable clip on the left wall, a cable cleat on the back (variants without it), ribbed / panels / smooth outside textures: six PETG
  bases and six PLA bases in all. Floor logo mark. USB-C bay with mic holes in the left wall. Battery stands on its edge on the right.

## Status: what Paul has confirmed on printed parts, and what has not been printed
- **Confirmed:** magnet pocket fits (C11), C6 flexure feel (round 2), **C12 plate + posts: "perfect"** (3.6 mm plate hole, pilot 2.4),
  **C13 pilot gauge: 2.5 mm best**, the v10.x shell/base fit generally.
- **Not printed yet (all v11d-g changes):** the finger-bar button (v11c-e), the 2.5 mm pilot in the real base, the shell with no right/
  front rim and the SD-tab relief, the front relief. Paul's old print: shell sat with a small gap and was not flush at the front;
  the changes target the SD tab / flex folds pushing the shell up. **Whether the gap is gone is unknown until he prints.**
- **Not opened in Bambu Studio by Claude (no access):** the new multi-plate 3MFs. Paul was asked to open one and report.

## Open items
1. Ask Paul how the printed button feels (PHASE2_CHECKLIST.md item 8): finger reach, click, force, self-pressing, cracking at the blade
   root. Knobs: `PWR_TARGET_FORCE` (down = thinner blade, floor ~0.62 mm), `PWR_FACE_W`, `PWR_FACE_BOT`, `PWR_REST_GAP` (down if mushy).
2. Verify the shell seats flush (front, right); the SD tab relief is small by necessity.
3. **The assembly guide (`assembly_guide/`, PDF + images) still shows the slide switch and the old plate** and has not been regenerated.
4. Paul may want the back (90 deg) relief removed (he said the cutouts belong at the bottom and right); left as is.
5. The Bambu project text (`assets/bambu_template/project_info.json`) is Paul's MakerWorld text with the switch line removed; it still
   says nothing about the new button and its "Supports: Yes - top USB port" line is from older versions.
6. No access to BOOT/RESET (not used). The firmware does not use the PWR key itself; it only needs to be pressable.

## How Paul works (rules that matter)
- **Ask Paul before any design decision DESIGN_RULES.md and the plan don't cover.** Fit and feel questions go to him as a printed
  **coupon** with a check list, not a guess. Never force fits on the board, glass or display; simulate the assembly path with >= 0.2 mm air.
- Cheap handled parts get the best (press) fit; expensive parts are never forced. No internal supports (supports on the outside are
  acceptable). Prefer printed solutions to bought hardware. Round-number text on parts is unreadable below ~10 mm; use notches.
- He reads results as short summaries: what changed, what was checked, what is untested, what he should print first.
- Branch/commit rules from the session setup: work on the named branch, commit and push when asked (the stop hook insists). **No pull
  request unless he asks.** Commit message footer lines come from the session's reminder. Don't commit `out/phase2/` while a build runs.
- Files for Paul go to him as attachments (print pack zip, the multi-plate 3MFs, coupons), not as paths.

## Multi-plate .3mf files (Bambu Studio): how to write them
A plain 3MF with `<plate>` entries in `model_settings.config` opens in Bambu Studio as ONE plate with the objects scattered round it
at their plate positions (Paul's screenshot, 2026-11-17). Never write a multi-plate file with hand-rolled XML.
- Use `lfcase/bambu3mf.py: write_plates(plates, path)` (`lfcase/export.py: write_3mf_plates` hands any file with more than one plate to it).
  `plates` = one list per plate of `dict(name, mesh (trimesh, print orientation), material 'gold'|'black', plate_name)`. Then run
  `bambu3mf.check_plates(path)` and fix anything it returns (`make_print_pack.py` does this).
- It copies the structure of the file Paul fixed in Bambu Studio 02.09.01.70 (`assets/bambu_template/` holds his project settings,
  filament profile, slice info and project info text):
  - production-extension layout: `3D/3dmodel.model` objects hold `<component p:path=...>` pointing at `3D/Objects/object_N.model`; every
    object/component/item has a `p:UUID`; `requiredextensions="p"`;
  - `Metadata/model_settings.config`: per object an `<object>` + `<part>` (uuid, matrix, source_*, mesh_stat); per plate a `<plate>` with
    `plater_id`, `plater_name`, `locked`, `filament_map_*` and a `<model_instance>` with `object_id`, `instance_id`, `identify_id`;
    and an `<assemble>` list;
  - `project_settings.config`, `filament_settings_1.config`, `slice_info.config`, `cut_information.xml`, `filament_sequence.json`;
  - Application tag `BambuStudio-02.09.01.70`, `BambuStudio:3mfVersion` 1. No thumbnails are written.
- Layout is Bambu's grid: columns = ceil(sqrt(n)); pitch = bed x 1.2 (P2S bed 256 -> 307.2); plate (row, col) is centred at
  (128 + col x 307.2, 128 - row x 307.2). Each mesh is stored centred on its bounding box and the item's z is half its height (lowest
  point at z = 0), otherwise Bambu merges objects into one.
- Verified only by rebuilding Paul's 8-plate file from its own meshes (same structure, same plate positions) and by `check_plates`.
  Single-plate files (`write_3mf_plates` with one plate, `bambu_app=False`) and the assembled project (`write_3mf_project`: parts flat
  on the plate and assembled in the Assembly view) are proven by Paul; leave them alone. If plates break again, ask for a Bambu-saved
  copy and diff its structure against the output.

## Running things (Linux, headless)
Setup: `README.md`. In short: `python3 -m venv .venv && .venv/bin/pip install -r requirements.txt vtk`; download Waveshare's **standard**
board STEP (`https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75/ESP32-S3-Touch-AMOLED-1.75-3D.zip`, the file
`ESP32-S3-Touch-AMOLED-1_75.stp`) to `board/` (git-ignored); prefix every script with `xvfb-run -a`. `phase0.py` re-extracts board
values and rewrites a generated block in `params.py` (harmless).
- `check_all.py`: build + board interference + layer check + swing + shell drop (30-40 min; run in the background and wait for it;
  do not edit source files you want checked while it runs, and kill it first if you change geometry mid-run).
  Pass = interference `[]` for every part, SWING True, DROP True, layer check flags only the known USB-C opening (15 flags at X -23.6).
- `regen_outputs.py`: after it passes, rewrites all of `out/` (STLs, STEPs, Bambu project, renders, PLA bases, `carrier_pla.stl`, the print
  pack zip with the multi-plate 3MFs, C12 coupon). `phase2.py` deletes `out/phase2/stl/`, which is why this script exists.
- Coupons: `coupon_carrier.py` (C12 plate + posts), `coupon_pilot.py` (C13 pilot gauge), `coupon_magnets.py` (C11).
- The swing check (`simulate_v10.py`) moves board + carrier into the base about the USB-C and drops the shell; 0.2 mm air to the board,
  screen and carrier everywhere except designed contacts. It is what caught: the PWR nub (0.2 gap came within 0.11), and a carrier tab
  extended toward the USB-C (0.04).

## Lessons from this work (so they are not repeated)
- The board swings in vertically past the PWR key (0.75 mm per degree of swing, drifting 0.09 mm/deg toward the nub): any feature beside
  the plunger must be checked in the swing, not just at rest.
- A feature printed in a tilted case (display 20 deg) has overhangs that differ from the board frame: horizontal surfaces for bridges must
  be cut level in the world frame (`pwr_button._roof`, the tunnel ceiling); the layer check (`lfcase/layers.py`) is the judge. Bridges up to
  10 mm are accepted; the head's top corners are cut at 45 deg to stay under that.
- A ruled loft of two `slot2D` wires gave an inside-out solid ("could not build a valid solid"); the scoop is built from shapely polygons.
- Printed holes come out ~0.2 mm small (pilot 2.2 was too tight); Paul's printer/filament are Bambu P2S, PLA Metal gold + PETG Matte black.
- Don't assume a fix from one photo: the plate slot (v11b) was based on a mis-read of where the problem was; the real cause was the base
  pilot size. Ask for the coupon result before changing parts that fitted.
- Waveshare's STEP has no switch travel; `KEY_TRAVEL` (0.25) is an assumption.

## Where things are
- `out/lyricflash_print_pack.zip` (committed): `stl/`, `stl_pla/`, `3mf_per_plate/`, `3mf_per_plate_pla/`, the two multi-plate projects
  (`lyricflash_print_plates.3mf`, `lyricflash_print_plates_PLA.3mf`), `PHASE2_CHECKLIST.md`, `README.txt`. `out/print_pack/` is the unzipped
  copy and is git-ignored.
- `out/phase2/`: renders, sections (`section_pwr.png` shows the button), `assembly_sim.md`, `report.md`, `lyricflash_case_project.3mf`.
- `out/coupons/`: C12 and C13. `PHASE2_CHECKLIST.md`: assembly steps and the print checks (the PWR button is item 8).
