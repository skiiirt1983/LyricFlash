"""Regenerate every file in out/ from the current params, without the slow checks. Run after check_all.py has passed.

  xvfb-run -a python -u regen_outputs.py

Builds the parts, writes out/phase2/ (STLs in print orientation, STEPs, the Bambu project 3MF, renders, sections), the five other
PETG base variants, the six PLA bases, carrier_pla.stl, out/lyricflash_print_pack.zip (with the multi-plate 3MFs) and the C12
carrier coupon. Takes about 15 min. Does not commit anything."""
import os
import pickle
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
os.chdir(HERE)
sys.path.insert(0, str(HERE))

import params as P  # noqa: E402
import phase2 as PH  # noqa: E402
from lfcase import enclosure as E  # noqa: E402

parts, info = E.build_all(log=lambda m: None)
for k, v in parts.items():
    v.exportBrep(str(PH.CACHE / f"{k}.brep"))
(PH.CACHE / "info.pkl").write_bytes(pickle.dumps(info))
PH.printable(parts, info)                     # writes out/phase2/stl/base.stl, carrier.stl, top_shell.stl (and clears the rest)
PH.export(parts, info)
try:
    PH.render(parts, info)
    PH.sections(parts, info)
except Exception as e:                        # renders need VTK + a display (xvfb-run)
    print("render failed:", e)
keep = P.TEXTURE
PH.variant("panels", "base_panels")
PH.variant(None, "base_smooth")
for tex, nm in (("ribs", "base_ribbed_nocleat"), ("panels", "base_panels_nocleat"), (None, "base_smooth_nocleat")):
    PH.variant(tex, nm, cleat=False)
P.TEXTURE = keep
for script in ("make_pla_bases.py",):
    subprocess.run([sys.executable, "-u", str(HERE / script)], check=True)
(HERE / "out" / "phase2" / "stl" / "carrier_pla.stl").write_bytes((HERE / "out" / "phase2" / "stl" / "carrier.stl").read_bytes())
for script in ("make_print_pack.py", "coupon_carrier.py"):
    subprocess.run([sys.executable, "-u", str(HERE / script)], check=True)
print("DONE", flush=True)
