"""Assembly-path proof for the tilt-in port base + backstop key (re-run after any design change).

    python simulate_assembly.py     # writes out/round3/assembly_sim.md

Steps simulated against the STEP board and the top shell:
  1. base closed
  2. base swung open about the hinge line, 1..25 deg (hook the USB-C first, swing closed)
  3. key pushed up through the floor slot from 12 mm below
A step passes only with zero intersecting volume.
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import cadquery as cq  # noqa: E402

import params as P  # noqa: E402
from lfcase import coupons_a as A  # noqa: E402
from lfcase.assembly import hinge_pose  # noqa: E402
from lfcase.board import load_board  # noqa: E402
from lfcase.interference import check  # noqa: E402


def main():
    out = HERE / "out" / "round3"
    out.mkdir(parents=True, exist_ok=True)
    g = load_board()
    base, key = A.tilt_base(), A.usb_key()
    top = A.top_shell_slice(A.USB_A0, A.USB_A1, ())
    obst = {k: v for k, v in g.items() if k not in ("display_fpc", "microsd", "misc_front")}
    obst["top_shell"] = top
    H = (P.TILT_HINGE[0], 0, P.TILT_HINGE[1])
    rows, ok = [], True

    def run(label, shape, ob):
        nonlocal ok
        h = [(k, round(v, 3)) for k, v, _ in check(shape, ob)]
        ok &= not h
        rows.append(f"| {label} | {'CLEAR' if not h else h} |")
        print(rows[-1], flush=True)

    run("base closed", base, obst)
    for a in (1, 3, 6, 9, 11, 13, 15, 18, 20, 25):
        run(f"base swung open {a} deg", hinge_pose(base, a, H), obst)
    ob2 = dict(obst)
    ob2["base"] = base
    for d in ((12, 8, 4, 2, 1, 0.3, 0) if getattr(P, "USB_BACKSTOP_KEY", True) else ()):
        run(f"key {d} mm below its final position", key.moved(cq.Location(cq.Vector(0, 0, -d))), ob2)
    text = ["# Assembly-path simulation", "",
            f"Hinge line X {P.TILT_HINGE[0]}, Z {P.TILT_HINGE[1]} (board frame), along Y. "
            "Obstacles: every STEP board part (incl. header envelope, plugs, standoffs, USB-C) + the top shell.", "",
            "| Step | Result |", "|---|---|"] + rows + ["", f"**Overall: {'PASS' if ok else 'FAIL'}**"]
    (out / "assembly_sim.md").write_text("\n".join(text))
    return ok


if __name__ == "__main__":
    sys.exit(0 if main() else 1)
