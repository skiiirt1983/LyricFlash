"""Round 2 coupons after Paul's round-1 results. One Bambu project 3MF:
flat on the plate, assembled in the Assembly view, slot 1 = gold PLA Metal, slot 2 = black PETG.

    python round2.py      # (headless Linux: xvfb-run -a python round2.py)
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import cadquery as cq  # noqa: E402

from lfcase import coupons as C  # noqa: E402
from lfcase import coupons_a as A  # noqa: E402
from lfcase.board import load_board  # noqa: E402
from lfcase.export import write_3mf_project, write_stl  # noqa: E402
from lfcase.interference import check as interference  # noqa: E402
from lfcase.layers import check as layer_check  # noqa: E402

OUT = HERE / "out" / "round2"
INTENDED = {("mic_base", "pcb")}          # crush ribs


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    g = load_board()
    report = ["# Round 2 coupons", ""]
    project = []
    x_off = 0.0
    for build in (A.c1_display_seat, A.c5_mic, A.c6_flexure, A.c9_usb_gauge, C.snap_latches, C.speaker_pocket):
        cp = build()
        board = cp.board_check() if cp.board_check else {}
        report += [f"## {cp.name}: {cp.title}"] + [f"- {n}" for n in cp.notes]
        for pn, sh in cp.parts.items():
            sh_print = A.print_orientation(pn, sh) if pn in board else sh
            f = OUT / f"{cp.name}__{pn}.stl"
            write_stl(sh_print, f)
            lay = [x for x in layer_check(f) if x[1] not in ("bridge", "short overhang")]
            line = f"- `{pn}`: layers {'OK' if not lay else 'FLOATING ' + str(lay)}"
            if pn in board:
                hits = [(k, round(v, 3)) for k, v, _ in interference(sh, g) if (pn, k) not in INTENDED]
                line += f", interference {'0 mm3' if not hits else hits}"
            report.append(line)
            gold = pn == "seat_ring"
            # assembled pose: board-frame parts keep their pose (shifted per coupon), others sit beside
            asm = sh if pn in board else sh_print
            project.append(dict(name=f"{cp.name} {pn} ({'gold' if gold else 'black'})",
                                shape_assembled=asm.moved(cq.Location(cq.Vector(x_off, 0, 0))),
                                flip_x_deg=180 if pn in ("seat_ring",) else 0,
                                material="gold" if gold else "black"))
        x_off += 70.0
        report.append("")
    write_3mf_project(project, OUT / "round2_project.3mf")
    (OUT / "report.md").write_text("\n".join(report))
    print("\n".join(report))


if __name__ == "__main__":
    main()
