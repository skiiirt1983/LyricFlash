"""One pass over the final case geometry: build every part, then run the four checks Paul's rules need, and save the result.

  xvfb-run -a python -u check_all.py        (about 30-40 min: the board interference check and the swing simulation are slow)

Prints BLADE / INFO (PWR button numbers), INTERFERENCE (must list [] for every part), LAYER (base layers the print check flags;
the 15 at X -23.6 are the known USB-C opening, anything else is new), SWING and DROP (must be True), then SAVED (the parts are
cached in cache/ for regen_outputs.py). Does not write the print packs: run regen_outputs.py after it passes."""
import os
import pickle
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
os.chdir(HERE)
sys.path.insert(0, str(HERE))

import params as P  # noqa: E402,F401
import phase2 as PH  # noqa: E402
import simulate_v10 as SV  # noqa: E402
from lfcase import enclosure as E  # noqa: E402

if getattr(P, "PWR_BUTTON", False):
    from lfcase import pwr_button as B
    d = B.dims()
    print("BLADE t", d["t"], {k: round(float(v), 2) for k, v in B.stiffness(d).items()}, flush=True)
parts, info = E.build_all(log=lambda m: None)
print("INFO", info.get("pwr"), flush=True)
chk = PH.check(parts, info)
print("INTERFERENCE", [(r[0], r[1], r[2]) for r in chk], flush=True)
lay = PH.printable(parts, dict(info))
for k, iss in lay.items():
    for i in iss:
        if i[1] in ("FLOATING", "LONG BRIDGE") and i[4][0] > 0:
            print("LAYER", k, i, flush=True)            # the USB-C opening's flags are at X < 0 and are known
ok_s, ok_d, sr, dr = SV.run(parts, info["zb"], out=PH.OUT / "assembly_sim.md")
print("SWING", ok_s, "DROP", ok_d, flush=True)
for k, v in parts.items():
    v.exportBrep(str(PH.CACHE / f"{k}.brep"))
(PH.CACHE / "info.pkl").write_bytes(pickle.dumps(info))
print("SAVED", flush=True)
