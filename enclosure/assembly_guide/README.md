# Assembly guide

- `LyricFlash_Assembly_Guide.pdf`: illustrated step-by-step assembly (design v9).
- `LyricFlash_Checklist_for_Install.pdf`: one-page check-and-report sheet for each build.

## Rebuild after a design change
Needs Python 3 with numpy, matplotlib, Pillow and reportlab (no CAD libraries).

```
python3 build_images.py      # renders img/*.png from ../out/phase2/lyricflash_case_project.3mf
python3 build_pdf.py         # assembly guide
python3 build_checklist.py   # checklist
```

The printed parts come from the project 3MF in their assembled positions. The board, battery and switch
are simple shapes placed from the numbers in `lfr.py` (`ZB`, `BAT_X0/X1`, `switch_wf`): update those if the
layout moves. `img/board_back.png` is a crop of `../out/phase0/board_back.png`. Step text lives in
`build_pdf.py`; checklist rows in `build_checklist.py`.
