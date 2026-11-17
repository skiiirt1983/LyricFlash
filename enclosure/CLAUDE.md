# Enclosure (LyricFlash case): notes for Claude

Read `README.md` and `DESIGN_RULES.md` first (Paul's rules). Every change goes in `CHANGELOG.md`; fit/feel questions go to Paul as a
coupon, not a guess. All dimensions live in `params.py` (later assignments override earlier ones; the last block wins).

## Multi-plate .3mf files (Bambu Studio): how to write them
A plain 3MF with `<plate>` entries in `model_settings.config` opens in Bambu Studio as ONE plate with the objects scattered
round it at their plate positions (Paul's screenshot, 2026-11-17). Never write a multi-plate file with hand-rolled XML.
- Use `lfcase/bambu3mf.py: write_plates(plates, path)` (`lfcase/export.py: write_3mf_plates` now hands any file with more than
  one plate to it). `plates` = one list per plate of `dict(name, mesh (trimesh, print orientation), material 'gold'|'black',
  plate_name)`. Then run `bambu3mf.check_plates(path)` and fix anything it returns (`make_print_pack.py` does this).
- What it copies from the file Paul fixed in Bambu Studio 02.09.01.70 (`assets/bambu_template/` holds his project settings,
  filament profile and project info text):
  - parts in the production-extension layout: `3D/3dmodel.model` objects hold `<component p:path=...>` pointing at
    `3D/Objects/object_N.model`; every object/component/item has a `p:UUID`; `requiredextensions="p"`;
  - `Metadata/model_settings.config`: per object an `<object>` + `<part>` (uuid, matrix, source_*, mesh_stat); per plate a `<plate>`
    with `plater_id`, `plater_name`, `locked`, `filament_map_*` and a `<model_instance>` with `object_id`, `instance_id`,
    `identify_id`; and an `<assemble>` list;
  - `project_settings.config`, `filament_settings_1.config`, `slice_info.config`, `cut_information.xml`, `filament_sequence.json`;
  - Application tag `BambuStudio-02.09.01.70`, `BambuStudio:3mfVersion` 1.
- Plate layout is Bambu's grid: columns = ceil(sqrt(n)); pitch = bed x 1.2 (P2S bed 256 -> 307.2); plate (row, col) is centred at
  (128 + col x 307.2, 128 - row x 307.2). Each mesh is stored centred on its bounding box and the item's z is half its height
  (lowest point on z = 0), otherwise Bambu merges objects into one.
- Single-plate files (`write_3mf_plates` with one plate, `bambu_app=False`) and the assembled project
  (`write_3mf_project`, flat on the plate and assembled in the Assembly view) are proven; leave them alone.
- No thumbnails are written (Bambu makes its own). If Paul reports the plates broken again, ask for a Bambu-saved copy and diff
  its structure against the output (see how `check_plates` and the writer's docstring were made).

## Regenerating everything (Linux, headless)
Setup in `README.md` (venv, board STEP in `board/`, `xvfb-run -a` in front of every script).
- `phase2.py` builds the case, runs the checks and writes `out/phase2/`; **it deletes `out/phase2/stl/`**, so afterwards run
  `make_pla_bases.py`, copy `carrier.stl` to `carrier_pla.stl`, then `make_print_pack.py` (PETG + PLA bases, plate files, zip).
- Checks to pass before handing over: no board interference, swing PASS, shell drop PASS (`simulate_v10.py`), layer check on the
  base only flags the known USB-C opening (`lfcase/layers.py`).
- Coupons: `coupon_carrier.py` (C12 plate + posts), `coupon_pilot.py` (C13 pilot gauge), `coupon_magnets.py` (C11).
- Work on the branch the session names; commit the source first, then `out/` once a run has finished (a running build rewrites
  `out/phase2/`).
