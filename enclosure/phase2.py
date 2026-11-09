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


def check(parts, info):
    """Interference: every part vs the STEP board + battery/speaker envelopes, and the parts vs each other."""
    from lfcase.board import load_board
    from lfcase.interference import check as chk
    zb = info["zb"]
    g = load_board(verbose=False)
    gw = {k: E.to_wf(v, zb) for k, v in g.items()}
    gw["battery_envelope"] = E._box(*E.battery_box())
    gw["speaker_envelope"] = E.speaker_dummy()
    rows = []
    for k, v in parts.items():
        hits = [(n, round(x, 3)) for n, x, _ in chk(v, gw)]
        rows.append((k, "board/battery/speaker", hits, len(v.Solids()), v.isValid()))
        print(rows[-1], flush=True)
    names = list(parts)
    for i, a in enumerate(names):
        for b_ in names[i + 1:]:
            hits = [(n, round(x, 3)) for n, x, _ in chk(parts[a], {b_: parts[b_]})]
            if hits:
                rows.append((a, b_, hits, None, None))
                print(rows[-1], flush=True)
    return rows


GOLD, BLACK, ACCENT = (0.83, 0.66, 0.30), (0.13, 0.13, 0.15), (0.85, 0.68, 0.30)


def render(parts, info, prefix="case"):
    from lfcase.board import load_board
    from lfcase.render import Scene, save_labelled
    zb = info["zb"]
    disp = E.to_wf(load_board(verbose=False)["display"], zb)
    col = {"top_shell": GOLD, "base": BLACK}
    views = [
        ("front_left", ((-0.8, -1.0, 0.55), (0, 0, 1)), "Front-left: USB-C bay, mic holes, cable clip"),
        ("back_right", ((0.9, 1.0, 0.5), (0, 0, 1)), "Back-right: PWR button, speaker grille"),
        ("front_right", ((0.9, -1.0, 0.45), (0, 0, 1)), "Front-right: BOOT pinhole, latch line"),
        ("underside", ((0.25, -0.35, -1.0), (0, 1, 0)), "Underside: switch pocket"),
    ]
    for name, cam, title in views:
        s = Scene(size=(1600, 1200))
        s.add(disp, (0.03, 0.03, 0.04))
        for k, v in parts.items():
            s.add(v, col.get(k, ACCENT if k != "cable_clip" else GOLD))
        s.camera(cam, target=(4, 0, 16), zoom=1.15)
        save_labelled(s, str(OUT / f"{prefix}_{name}.png"), title=title)
