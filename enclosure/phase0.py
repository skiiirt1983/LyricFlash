"""Phase 0: import the board STEP, extract PLAN.md section 4 values, render it.

    python phase0.py            # (Linux headless: xvfb-run -a python phase0.py)

Writes:
  params.py                      BOARD-FROM-STEP block (regenerated)
  out/phase0/board_values.md     table of every extracted value + source
  out/phase0/board_*.png         labelled renders
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from lfcase.board import DEFAULT_STEP, _step_hash, load_board  # noqa: E402
from lfcase.extract import extract  # noqa: E402
from lfcase.paramgen import write_board_block  # noqa: E402
from lfcase.render import Scene, save_labelled  # noqa: E402

OUT = HERE / "out" / "phase0"

COLORS = {
    "pcb": (0.10, 0.42, 0.22),
    "display": (0.12, 0.12, 0.14),
    "pmma_frame": (0.85, 0.85, 0.80),
    "display_fpc": (0.85, 0.55, 0.15),
    "usb_c": (0.80, 0.80, 0.84),
    "key1": (0.95, 0.55, 0.10),
    "key2": (0.95, 0.55, 0.10),
    "mic1": (0.85, 0.15, 0.15),
    "mic2": (0.85, 0.15, 0.15),
    "j1": (0.95, 0.95, 0.88),
    "h3": (0.95, 0.95, 0.88),
    "microsd": (0.55, 0.60, 0.70),
    "header": (0.15, 0.15, 0.15),
    "standoffs": (0.85, 0.70, 0.30),
    "misc_back": (0.35, 0.35, 0.38),
    "misc_front": (0.35, 0.35, 0.38),
}


def table(values) -> str:
    rows = ["| Name | Value | Source |", "|---|---|---|"]
    for v in values:
        val = "**TODO**" if v.value is None or v.value == "TODO" else f"`{v.value}`" + (f" {v.unit}" if v.unit else "")
        rows.append(f"| {v.name} | {val} | {v.source} |")
    return "\n".join(rows)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    groups = load_board()
    values = extract(groups)
    from lfcase.answers import ANSWERS
    from lfcase.extract import Value
    seen = set()
    for v in values:
        if v.name in ANSWERS:
            v.value, v.source = ANSWERS[v.name][0], "ANSWERED: " + ANSWERS[v.name][1]
            v.unit = ""
            seen.add(v.name)
    for k, (val, src) in ANSWERS.items():
        if k not in seen:
            values.append(Value(k, val, "ANSWERED: " + src, ""))
    V = {v.name: v.value for v in values}
    write_board_block(values, HERE / "params.py", DEFAULT_STEP.name, _step_hash(DEFAULT_STEP))
    (OUT / "board_values.md").write_text(
        "# Board values extracted from the STEP\n\n"
        "Board frame: origin = PCB centre on the PCB back face, +Z out of the glass, "
        "USB-C points to -X. Units mm / deg.\n\n" + table(values) + "\n"
    )

    def scene(skip=()):
        s = Scene()
        for g, shp in groups.items():
            if g in skip:
                continue
            s.add(shp, COLORS.get(g, (0.6, 0.6, 0.6)))
        return s

    k1, k2 = V["KEY1_TIP_CENTER"], V["KEY2_TIP_CENTER"]
    m1, m2 = V["MIC1_CENTER"], V["MIC2_CENTER"]
    usb = (V["USB_MOUTH_X"], V["USB_CENTER_Y"], V["USB_CENTER_Z"])
    j1 = V["J1_BBOX"]
    h3 = V["H3_BBOX"]
    cen = lambda b: ((b[0][0] + b[1][0]) / 2, (b[0][1] + b[1][1]) / 2, b[0][2])
    notch = V["EDGE_NOTCHES"]
    sd = V["MICROSD_BBOX"]

    # ---- back view (component side), oriented like Waveshare's drawing
    s = scene()
    s.camera("back_drawing", zoom=0.72)
    save_labelled(
        s, OUT / "board_back.png",
        title="Back (component side), oriented like Waveshare's drawing",
        footer="KEY/J1/H3 roles from Paul's photo (silkscreen). Mic sound-port side: pending.",
        labels=[
            (usb, "USB-C (H1)", complex(150, 12), "tab:blue"),
            (k1, "KEY1\nPWR or BOOT?", 230, "tab:orange"),
            (k2, "KEY2\nPWR or BOOT?", 230, "tab:orange"),
            ((m1[0], m1[1], -1.1), "MIC1", complex(200, 25), "tab:red"),
            ((m2[0], m2[1], -1.1), "MIC2", complex(200, -25), "tab:red"),
            (cen(j1), "J1 MX1.25\nBAT or SPK?", complex(330, -12), "tab:purple"),
            (cen(h3), "H3 MX1.25\nBAT or SPK?", complex(330, 12), "tab:purple"),
            ((0, -18.7, -5.8), "8-pin header", 330, "black"),
            ((notch[0][0], notch[0][1], 0), "edge notch", complex(170, -20), "tab:green"),
            ((notch[1][0], notch[1][1], 0), "edge notch", complex(170, 20), "tab:green"),
            ((V["MOUNT_HOLES_XY"][1][0], V["MOUNT_HOLES_XY"][1][1], -3.5), "M2 standoff", 120, "goldenrod"),
        ],
    )

    # ---- front view (glass side)
    s = scene()
    s.camera("front_drawing", zoom=0.72)
    save_labelled(
        s, OUT / "board_front.png", title="Front (glass side)",
        footer=f"Glass D{V['GLASS_D']}  panel D{V['PANEL_D']}  PCB D{V['PCB_D']}  total thickness {V['BOARD_TOTAL_THK']}",
        labels=[
            (usb, "USB-C", 230, "tab:blue"),
            (k1, "KEY1", 200, "tab:orange"),
            (k2, "KEY2", 200, "tab:orange"),
            ((sd[1][0], 0, 1.8), "microSD slot\n(under glass)", 230, "slategray"),
        ],
    )

    # ---- side views
    s = scene()
    s.camera("left", zoom=1.5)
    save_labelled(
        s, OUT / "board_side_usb.png", title="Side view, looking into USB-C (-X side)",
        footer=f"USB mouth {V['USB_SHELL_W']} x {V['USB_SHELL_H']}, centre Z {V['USB_CENTER_Z']}; glass overhangs the mouth by {V['USB_GLASS_OVERHANG']}",
        labels=[
            (usb, "USB-C", (0, 170), "tab:blue"),
            ((-24.4, 0, 6.3), "cover glass", (-250, -110), "black"),
            ((-22.0, 0, 0.6), "PCB", (250, 110), "green"),
            ((-17, 11, -0.6), "MIC2", (-200, 140), "tab:red"),
            ((-17, -11, -0.6), "MIC1", (200, 140), "tab:red"),
        ],
    )
    s = scene()
    s.camera("right", zoom=1.5)
    save_labelled(
        s, OUT / "board_side_buttons.png", title="Side view, buttons (+X side)",
        footer=f"Side-push buttons; plunger tips at Z {V['KEY1_TIP_Z']}, pressing toward the centre at about +/-34 deg",
        labels=[
            (k1, "KEY1", (-180, 150), "tab:orange"),
            (k2, "KEY2", (180, 150), "tab:orange"),
            ((22.0, 0, 1.8), "microSD slot", (0, -160), "slategray"),
        ],
    )

    # ---- iso
    s = scene()
    s.camera("iso_back", zoom=0.85)
    save_labelled(
        s, OUT / "board_iso_back.png", title="Iso, back side",
        labels=[
            (usb, "USB-C", 200, "tab:blue"),
            (k1, "KEY1", 200, "tab:orange"),
            (k2, "KEY2", 200, "tab:orange"),
            ((m1[0], m1[1], -1.1), "MIC1", 200, "tab:red"),
            ((m2[0], m2[1], -1.1), "MIC2", 200, "tab:red"),
        ],
    )
    print(table(values))
    print(f"\nWrote {OUT}")


if __name__ == "__main__":
    main()
