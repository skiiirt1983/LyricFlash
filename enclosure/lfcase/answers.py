"""Facts the STEP cannot provide, answered by Paul. phase0.py merges these into
params.py in place of the matching TODO values. Keep the source of each one."""

ANSWERS = {
    "PWR_IS": ("KEY2", "Paul's photo 2026-11-09: silkscreen 'BOOT' is next to KEY1, so PWR = KEY2"),
    "BATTERY_CONNECTOR": ("J1", "Paul's photo 2026-11-09: silkscreen 'BAT' at J1, 'SPK' at H3"),
    "BOOT_IS": ("KEY1", "Paul's photo 2026-11-09: silkscreen 'BOOT' next to KEY1"),
    "SPEAKER_CONNECTOR": ("H3", "Paul's photo 2026-11-09: silkscreen 'SPK' at H3 (nothing plugged in)"),
    # MIC positions confirmed by Paul 2026-11-09: MIC1 bottom-right, MIC2 bottom-left of USB-C
    # in the drawing orientation (matches the STEP ref-des).
    "MIC_PORT_SIDE": ("top", "Paul 2026-11-09: both mic lids have a 0.3-0.5 mm hole (top-port, faces away from the PCB)"),
    "HEADER_IN_USE": (False, "Paul 2026-11-09: 8-pin header was broken off for a magnetic charger that was abandoned; "
                             "not used. Leftover bent pins/solder ~4-5 mm remain, so the header keep-out (Z -5.8) stays "
                             "unless Paul clips them flush."),
    "BEZEL_STYLE": ("A", "Paul 2026-11-09: option A, PLAN 3.1 lip, board loads from behind, base clamps it forward"),
}
