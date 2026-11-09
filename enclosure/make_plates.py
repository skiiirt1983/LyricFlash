"""Split the Phase 1 coupons into two plates by final colour:
top-shell colour (seat ring, C2 top slice, C7 hooks) and base-tray colour (everything else).

    python make_plates.py     # writes out/phase1/plate_TOP_colour.3mf and plate_BASE_colour.3mf
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from lfcase import coupons as C  # noqa: E402
from lfcase import coupons_a as A  # noqa: E402
from lfcase.export import pack, write_3mf  # noqa: E402

TOP_PARTS = {"seat_ring", "usb_top_slice"}            # + every snap_male_*
BUILDERS = [A.c1_display_seat, A.c2_usb, C.battery_tray, C.switch_pocket, A.c5_mic, A.c6_flexure,
            C.snap_latches, C.speaker_pocket]


def main():
    top, base = {}, {}
    for b in BUILDERS:
        cp = b()
        for pn, sh in cp.parts.items():
            if cp.board_check:
                sh = A.print_orientation(pn, sh)
            (top if pn in TOP_PARTS or pn.startswith("snap_male") else base)[f"{cp.name}__{pn}"] = sh
    out = HERE / "out" / "phase1"
    write_3mf(pack(top), out / "plate_TOP_colour.3mf")
    write_3mf(pack(base), out / "plate_BASE_colour.3mf")
    print("TOP colour:", sorted(top))
    print("BASE colour:", sorted(base))


if __name__ == "__main__":
    main()
