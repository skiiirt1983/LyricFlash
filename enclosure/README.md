# LyricFlash enclosure

A parametric, 3D-printed case for LyricFlash on the Waveshare ESP32-S3-Touch-AMOLED-1.75.
The design brief is PLAN.md (kept with Paul). All dimensions live in `params.py`.

Status: **Phase 0 done** (board imported, values extracted, labelled renders).
Phase 1 (fit-test coupons) starts once the open questions below are answered.

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
python phase0.py
# Headless Linux (no display): xvfb-run -a python phase0.py
```

This rewrites the board block in `params.py` and writes `out/phase0/`:

| File | What |
|---|---|
| `board_values.md` | every extracted value with the STEP feature it came from |
| `board_back.png` | component side, oriented like Waveshare's drawing, key parts labelled |
| `board_front.png`, `board_side_usb.png`, `board_side_buttons.png`, `board_iso_back.png` | other views |

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
out/               generated renders/exports (committed so they can be viewed on GitHub)
```
