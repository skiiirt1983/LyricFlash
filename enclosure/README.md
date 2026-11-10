# LyricFlash enclosure

A parametric, 3D-printed case for LyricFlash on the Waveshare ESP32-S3-Touch-AMOLED-1.75.
The design brief is PLAN.md (kept with Paul). All dimensions live in `params.py`.

Status: **Phase 2 v3 built** (layout B: board centred, 52.9 x 62 mm) (full case, all checks in `out/phase2/report.md`). Print and assembly steps:
`PHASE2_CHECKLIST.md`. Phase 1 (fit tests) is in `PHASE1_CHECKLIST.md`, the USB-C tolerance stack-up in
`USB_STACKUP.md`, and every decision in `CHANGELOG.md`.

## Setup

Python 3.10 or newer.

```bash
cd enclosure
python -m venv .venv
# Windows: .venv\Scripts\activate     macOS/Linux: source .venv/bin/activate
pip install -r requirements.txt
```

### Board model (not committed)

Download Waveshare's official 3D model, unzip it, and copy the **standard** board STEP
(not the `-B` cased one) to `enclosure/board/ESP32-S3-Touch-AMOLED-1_75.stp`:

https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75/ESP32-S3-Touch-AMOLED-1.75-3D.zip

Or point `LF_BOARD_STEP` at the file. The first run reads the STEP (about 15 s) and caches each
component under `enclosure/cache/`.

## Run

```bash
python phase0.py      # board import + params.py + labelled renders
python phase1.py      # all coupons: checks, STL/STEP, 3MF plate, renders, report
python phase1.py C2_usb_wall C6_pwr_flexure   # just some coupons
python phase2.py      # full case: build, board check, tilt-in swing, layer check, CG, renders, 3MF
# Headless Linux (no display): prefix with  xvfb-run -a
```

`phase1.py` writes `out/phase1/lyricflash_coupons.3mf` (one P2S plate), per-coupon STL/STEP and
PNGs, and `out/phase1/report.md`: sizes, watertight check, overhangs, beam strains, and a boolean
interference check of every board-related coupon against the STEP board.

This rewrites the board block in `params.py` and writes `out/phase0/`:

| File | What |
|---|---|
| `board_values.md` | every extracted value with the STEP feature it came from |
| `board_back.png` | component side, oriented like Waveshare's drawing, key parts labelled |
| `board_front.png`, `board_side_usb.png`, `board_side_buttons.png`, `board_iso_back.png` | other views |

`phase2.py` writes `out/phase2/lyricflash_case_project.3mf` (Bambu project: flat on the plate and
assembled in the Assembly view, gold = slot 1, black PETG = slot 2), STLs in print orientation
(`out/phase2/stl/`), assembled STEPs (`out/phase2/step/`), renders and sections, `assembly_sim.md` and
`report.md`. The swing check (`simulate_case.py`, `lfcase/meshsim.py`) measures distances on dense
surface samples (k-d trees) instead of BRep booleans, so a full run takes a few minutes.

## Board frame

Origin = PCB centre on the PCB **back** face. +Z points out of the glass. USB-C points to -X,
the two side buttons are on +X, and the 8-pin header is on -Y. Every part in this project uses this frame.

## Layout

```
params.py          all dimensions (board block generated from the STEP + design values from PLAN.md)
phase0.py          Phase 0 entry point
lfcase/board.py    STEP loader (keeps component names), board frame, cache
lfcase/extract.py  measures the PLAN section 4 values from the STEP
lfcase/paramgen.py writes the generated block into params.py
lfcase/render.py   VTK off-screen renders with labels
lfcase/coupons.py  board-independent coupons (battery, switch, latches, speaker)
lfcase/coupons_a.py option-A coupons in the board frame (seat, USB-C, mics, PWR flexure)
lfcase/interference.py  boolean check against the STEP board
concepts/          decision aids (bezel option renders)
out/               generated renders/exports (committed so they can be viewed on GitHub)
```

## Bambu Studio project files

`lfcase/export.py: write_3mf_project()` writes one 3MF where every part is **separate and flat
on the plate** (print orientation) and **assembled in Bambu Studio's Assembly view**. Each part is
assigned to a filament slot (1 = gold PLA Metal, 2 = black PETG) in `model_settings.config`;
no 3MF colour tags, because with a settings file present Bambu's colour-preview path would erase
the slot assignment.
Rules it follows (from Bambu Studio's loader, confirmed on Paul's machine 2026-11-09):
- assembled poses go in `Metadata/model_settings.config` as `<assemble_item>`;
- each mesh is stored centred on its bounding box (Bambu re-centres objects on import);
- every object's lowest point is exactly z = 0, otherwise Bambu merges the objects into one
  multi-part object and drops the assembled poses.
Trial: `concepts/assembled_trial.py` -> `out/concepts/assembled_trial_v2.3mf`.
Confirmed by Paul 2026-11-09: parts flat on the plate, assembled in Assembly view, parts land on
filament slots 1/2. Bambu shows a harmless "not from Bambu Lab" notice (geometry + slots load fine).
