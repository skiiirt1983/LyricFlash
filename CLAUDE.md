# LyricFlash

Firmware for a desk device (Waveshare ESP32-S3-Touch-AMOLED-1.75): song identification, synced lyrics, a clock. ESP-IDF; see
`README.md`, `BUILD.md` and `VALIDATION.md` for the firmware, its host tests (`tests/run_tests.sh`) and the release/OTA process.

## The printed case is in `enclosure/`
**Read `enclosure/CLAUDE.md` first for anything about the case** (3D-printed shell, base, carrier plate, the PWR button, print packs,
the multi-plate 3MF rules, how to run the checks). The case work is on the branch `claude/loving-heisenberg-n1judx`; `main` does not
have the `enclosure/` folder.

## Working rules
- Work on the branch the session names. Commit and push when asked; no pull request unless asked.
- Say plainly what was checked on hardware or in a model and what was not.
- Ask Paul before a design decision the written rules (`enclosure/DESIGN_RULES.md` for the case) don't cover.
