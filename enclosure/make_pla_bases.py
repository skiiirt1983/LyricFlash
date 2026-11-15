"""PLA versions of the six bases (Paul, 2026-11-11), for people who don't want to print PETG.

Only change: the base's magnet pockets use the PLA fit from Paul's C11 coupon (dia 4.20, coupon #3, the fit the
PLA shell uses) instead of the PETG fit (dia 4.10). Everything else is the same as the PETG bases. Print with
X-Y hole compensation 0 (the larger pockets are already in the model).
Writes out/phase2/stl/base_*_pla.stl.
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import params as P  # noqa: E402
import phase2 as F  # noqa: E402

VARIANTS = [("ribs", "base_ribbed_pla", True), ("panels", "base_panels_pla", True), (None, "base_smooth_pla", True),
            ("ribs", "base_ribbed_nocleat_pla", False), ("panels", "base_panels_nocleat_pla", False),
            (None, "base_smooth_nocleat_pla", False)]


def main():
    keep_fit, keep_tex = P.MAGNET_FIT, P.TEXTURE
    P.MAGNET_FIT = P.MAGNET_FIT_SHELL                    # PLA pocket fit (coupon #3)
    try:
        for tex, name, cleat in VARIANTS:
            F.variant(tex, name, cleat=cleat)
            print("wrote", name, flush=True)
    finally:
        P.MAGNET_FIT, P.TEXTURE = keep_fit, keep_tex


if __name__ == "__main__":
    main()
