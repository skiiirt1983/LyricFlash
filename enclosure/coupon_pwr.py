"""C14: PWR button coupon. A slice of the real base round the button (right wall, blade, nub, bar, face, scoop), printed
in the base's orientation and filament, in two blade thicknesses. Paul's first finger-bar print (v11g) did not move: the
0.5 mm tunnel bonded and the roof bridge sagged onto the bar. This coupon has the v11h tunnel (PWR_TUNNEL_*).
Writes out/coupons/C14_pwr_button_<t>.stl and C14_pwr_button.3mf (one plate, PETG)."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import cadquery as cq  # noqa: E402
import trimesh  # noqa: E402

import params as P  # noqa: E402
from lfcase import enclosure as E  # noqa: E402
from lfcase import pwr_button as B  # noqa: E402
from lfcase.export import write_3mf_plates, write_stl  # noqa: E402

OUT = HERE / "out" / "coupons"
THICKNESSES = (None, 0.75)         # None = solved for PWR_TARGET_FORCE (~0.65, ~2.3 N); 0.75 is the stiffer fallback (~3.7 N)


def build(zb, thk):
    P.PWR_BLADE_THK = thk
    B._G.clear()
    b = E.base(zb)
    b, info = B.pwr_button(b, zb)
    g = B.geometry(zb)
    d = B.dims()
    keep = E._box((d["x1"] - 13.0, g["yc"] - 15.0, g["z0"] - 14.0), (d["x1"] + 2.0, g["yc"] + 15.0, g["z1"] + 6.0))
    return b.intersect(keep), info, d


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    zb = E.board_z()
    plates = []
    for thk in THICKNESSES:
        sh, info, d = build(zb, thk)
        tag = f"{d['t']:.2f}"
        f = OUT / f"C14_pwr_button_{tag}.stl"
        write_stl(sh, f)
        m = trimesh.load(f)
        m.apply_translation([0, 0, -m.bounds[0][2]])
        m.export(f)
        from lfcase import layers
        bad = [i for i in layers.check(f) if i[1] in ("FLOATING", "LONG BRIDGE")]
        print("layer issues floating/long:", bad[:6])
        print("C14", tag, "watertight", m.is_watertight, "size", [round(float(v), 1) for v in m.extents],
              "force est", round(info["stiffness"]["force"], 2), "N blade h", info["blade"]["h_at_load"])
        plates.append(dict(name=f"C14_pwr_button_{tag}", mesh=m, material="black", plate_name=f"C14 PWR button {tag}"))
    write_3mf_plates([plates], OUT / "C14_pwr_button.3mf", bambu_app=False)
    print("wrote", OUT / "C14_pwr_button.3mf")


if __name__ == "__main__":
    main()
