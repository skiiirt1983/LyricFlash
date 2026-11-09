"""Trial: C2 halves + C7 0.25 pair laid flat on the plate, with an assembled pose for
Bambu Studio's Assembly view. Gold = top-shell material, black = base material."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE))
import cadquery as cq  # noqa: E402

from lfcase import coupons as C  # noqa: E402
from lfcase import coupons_a as A  # noqa: E402
from lfcase.export import write_3mf_project  # noqa: E402

c2 = A.c2_usb().board_check()
c7 = C.snap_latches().parts
# C7 parts are drawn flat in print orientation; assembled = male pushed into female
male, fem = c7["snap_male_0.25"], c7["snap_female_0.25"]
mb, fb = male.BoundingBox(), fem.BoundingBox()
male_asm = male.moved(cq.Location(cq.Vector(fb.xmin - mb.xmin + (fb.xlen - mb.xlen) / 2, fb.ymax - (mb.ymin + 5.5), 0)))
off = cq.Location(cq.Vector(45, 0, 0))   # put the latch pair beside the C2 assembly
parts = [
    dict(name="C2 top slice (gold)", shape_assembled=c2["usb_top_slice"], flip_x_deg=180, material="gold"),
    dict(name="C2 base (black)", shape_assembled=c2["usb_base"], material="black"),
    dict(name="C7 hook 0.25 (gold)", shape_assembled=male_asm.moved(off), material="gold"),
    dict(name="C7 socket 0.25 (black)", shape_assembled=fem.moved(off), material="black"),
]
write_3mf_project(parts, HERE / "out" / "concepts" / "assembled_trial_v2.3mf")
print("ok")
