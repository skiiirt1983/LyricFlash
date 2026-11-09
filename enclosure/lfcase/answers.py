"""Facts the STEP cannot provide, answered by Paul. phase0.py merges these into
params.py in place of the matching TODO values. Keep the source of each one."""

ANSWERS = {
    "PWR_IS": ("KEY2", "Paul's photo 2026-11-09: silkscreen 'BOOT' is next to KEY1, so PWR = KEY2"),
    "BATTERY_CONNECTOR": ("J1", "Paul's photo 2026-11-09: silkscreen 'BAT' at J1, 'SPK' at H3"),
    "BOOT_IS": ("KEY1", "Paul's photo 2026-11-09: silkscreen 'BOOT' next to KEY1"),
    "SPEAKER_CONNECTOR": ("H3", "Paul's photo 2026-11-09: silkscreen 'SPK' at H3 (nothing plugged in)"),
    # MIC positions confirmed by Paul 2026-11-09: MIC1 bottom-right, MIC2 bottom-left of USB-C
    # in the drawing orientation (matches the STEP ref-des). Port side: still pending.
}
