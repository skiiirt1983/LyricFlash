"""Phase 2: build the full case, check it against the STEP board, simulate the tilt-in, export."""
import pickle
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from lfcase import enclosure as E  # noqa: E402

OUT = HERE / "out" / "phase2"
CACHE = OUT / "brep"


def build():
    CACHE.mkdir(parents=True, exist_ok=True)
    t = time.time()
    parts, info = E.build_all(log=lambda m: print(f"[{time.time() - t:6.0f}s] {m}", flush=True))
    for k, v in parts.items():
        v.exportBrep(str(CACHE / f"{k}.brep"))
    (CACHE / "info.pkl").write_bytes(pickle.dumps(info))
    return parts, info


def load():
    import cadquery as cq
    parts = {p.stem: cq.Shape.importBrep(str(p)) for p in sorted(CACHE.glob("*.brep"))}
    return parts, pickle.loads((CACHE / "info.pkl").read_bytes())


if __name__ == "__main__":
    build()
