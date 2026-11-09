"""Trial of the 'assembled preview' 3MF using coupon C2 (top slice + base), as it fits together."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))
from lfcase import coupons_a as A  # noqa: E402
from lfcase.export import write_3mf_assembled  # noqa: E402

cp = A.c2_usb()
parts = cp.board_check()            # board frame = assembled positions
write_3mf_assembled({"C2 top slice (gold PLA)": parts["usb_top_slice"], "C2 base (black PETG)": parts["usb_base"]},
                    HERE / "out" / "concepts" / "assembled_trial_C2.3mf",
                    colors={"C2 top slice (gold PLA)": "#C99E4C", "C2 base (black PETG)": "#151517"})
print("ok")
