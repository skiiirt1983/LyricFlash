# Phase 2 v10 assembly simulation

Surface-sample check, spacing 0.12 mm (good to about +/-0.06 mm). Rule: >= 0.2 mm of air to the board, screen and carrier on every path; printed-to-printed guides >= 0.1 mm.

## 1. Board + carrier into the base (USB-C first, swung down about the USB-C hinge)

| Opened by | Closest (mm, < 1.0) | Designed | FAIL |
|---|---|---|---|
| 0 deg | carrier 0.0, pcb 0.01, pmma_frame 0.01, misc_back 0.27, usb_c 0.4 | USB-C web on the PCB edge (zero gap by design, separating); plate tabs settling onto their posts beside the hinge (printed on printed, nothing on the board) | none |
| 0.25 deg | pcb 0.01, carrier 0.01, pmma_frame 0.02, misc_back 0.28, usb_c 0.4 | USB-C web on the PCB edge (zero gap by design, separating); plate tabs settling onto their posts beside the hinge (printed on printed, nothing on the board) | none |
| 0.5 deg | pcb 0.03, carrier 0.03, pmma_frame 0.04, misc_back 0.3, usb_c 0.4 | USB-C web on the PCB edge (zero gap by design, separating); plate tabs settling onto their posts beside the hinge (printed on printed, nothing on the board) | none |
| 1 deg | carrier 0.05, pmma_frame 0.06, pcb 0.07, misc_back 0.34, usb_c 0.4 | USB-C web on the PCB edge (zero gap by design, separating); plate tabs settling onto their posts beside the hinge (printed on printed, nothing on the board) | none |
| 1.5 deg | pmma_frame 0.08, carrier 0.08, pcb 0.1, misc_back 0.37, usb_c 0.4 | USB-C web on the PCB edge (zero gap by design, separating); plate tabs settling onto their posts beside the hinge (printed on printed, nothing on the board) | none |
| 2 deg | pmma_frame 0.1, carrier 0.1, pcb 0.12, misc_back 0.4, usb_c 0.4 | USB-C web on the PCB edge (zero gap by design, separating); plate tabs settling onto their posts beside the hinge (printed on printed, nothing on the board) | none |
| 3 deg | carrier 0.11, pmma_frame 0.14, pcb 0.15, usb_c 0.4, misc_back 0.46 | USB-C web on the PCB edge (zero gap by design, separating); plate tabs settling onto their posts beside the hinge (printed on printed, nothing on the board) | none |
| 4 deg | carrier 0.1, pmma_frame 0.19, pcb 0.2, usb_c 0.4, misc_back 0.52 | USB-C web on the PCB edge (zero gap by design, separating); plate tabs settling onto their posts beside the hinge (printed on printed, nothing on the board) | none |
| 5 deg | carrier 0.04, pmma_frame 0.23, pcb 0.24, usb_c 0.4, misc_back 0.58 | - | carrier 0.04 at (-17.5, -20.0, 39.8) |
| 6 deg | carrier 0.04, pmma_frame 0.28, pcb 0.31, usb_c 0.4, misc_back 0.64 | - | carrier 0.04 at (-17.8, -19.9, 39.9) |
| 8 deg | carrier 0.03, pmma_frame 0.37, usb_c 0.4, pcb 0.47, misc_back 0.77 | - | carrier 0.03 at (-17.9, -19.9, 39.7) |
| 10 deg | carrier 0.02, usb_c 0.4, pmma_frame 0.46, pcb 0.65, misc_back 0.91 | - | carrier 0.02 at (-18.7, -19.8, 39.9) |
| 12 deg | carrier 0.03, usb_c 0.39, pmma_frame 0.56, pcb 0.8 | - | carrier 0.03 at (-18.6, -19.7, 39.7) |
| 15 deg | carrier 0.04, usb_c 0.41, pmma_frame 0.7, pcb 0.98 | - | carrier 0.04 at (-19.0, -19.6, 39.7) |
| 18 deg | carrier 0.03, usb_c 0.52, pmma_frame 0.84 | - | carrier 0.03 at (-19.3, -19.5, 40.1) |
| 22 deg | carrier 0.15, usb_c 0.8 | - | carrier 0.15 at (-19.8, -19.4, 40.1) |
| 26 deg | carrier 0.2, usb_c 0.89 | - | none |
| 30 deg | carrier 0.42, usb_c 0.89 | - | none |

**Swing: FAIL**

## 2. Top shell onto the base (straight down along the board normal)

| Above closed by | Closest (mm, < 1.0) | Designed | FAIL |
|---|---|---|---|
| 0 mm | base 0.0, display 0.25 | seam faces closing | none |
| 0.05 mm | base 0.05, display 0.29 | seam faces closing | none |
| 0.1 mm | base 0.1, display 0.29 | seam faces closing | none |
| 0.2 mm | base 0.15, display 0.29 | - | none |
| 0.3 mm | base 0.15, display 0.29 | - | none |
| 0.5 mm | base 0.15, display 0.29 | - | none |
| 0.75 mm | base 0.15, display 0.3 | - | none |
| 1 mm | base 0.15, display 0.3 | - | none |
| 1.5 mm | base 0.15, display 0.31 | - | none |
| 2 mm | base 0.15, display 0.32 | - | none |
| 2.5 mm | base 0.15, display 0.32 | - | none |
| 3 mm | display 0.32, base 0.52 | - | none |
| 3.5 mm | display 0.33 | - | none |
| 4 mm | display 0.47 | - | none |
| 5 mm | display 0.47 | - | none |
| 6 mm | display 0.48 | - | none |
| 8 mm | all > 1.0 | - | none |
| 10 mm | all > 1.0 | - | none |
| 12 mm | all > 1.0 | - | none |

**Shell drop: PASS**
