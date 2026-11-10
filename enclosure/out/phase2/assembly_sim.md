# Phase 2 assembly simulation (tilt-in)

Base swings about the USB-C hinge line (board frame X -24.2, Z 5.5, along Y). Surface-sample check, spacing 0.12 mm (distances good to about +/-0.06 mm). Rule: >= 0.2 mm of air on the way in.

Designed contacts (allowed): the USB-C web on the PCB edge, the seam faces at the hinge line, the latch hooks riding their catches. Anything else under 0.2 mm is a FAIL.

| Opened by | Closest pairs (mm, < 1.0 shown) | Designed contacts | Below 0.2, not designed (FAIL) |
|---|---|---|---|
| 0 deg | base/pcb 0.0, base/top_shell 0.0, base/pmma_frame 0.01, clip_front/top_shell 0.09, clip_back/top_shell 0.09, base/misc_back 0.27 | USB-C web on the PCB edge (zero gap by design, separating); latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 0.25 deg | base/top_shell 0.0, base/pcb 0.01, clip_front/top_shell 0.01, clip_back/top_shell 0.01, base/pmma_frame 0.02, base/misc_back 0.28 | USB-C web on the PCB edge (zero gap by design, separating); latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 0.5 deg | base/top_shell 0.01, clip_back/top_shell 0.01, clip_front/top_shell 0.02, base/pcb 0.03, base/pmma_frame 0.03, base/misc_back 0.3 | USB-C web on the PCB edge (zero gap by design, separating); latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 1 deg | base/top_shell 0.01, clip_front/top_shell 0.01, clip_back/top_shell 0.02, base/pcb 0.06, base/pmma_frame 0.06, base/misc_back 0.33 | USB-C web on the PCB edge (zero gap by design, separating); latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 1.5 deg | clip_front/top_shell 0.01, clip_back/top_shell 0.01, base/top_shell 0.02, base/pmma_frame 0.08, base/pcb 0.1, base/header 0.34 | USB-C web on the PCB edge (zero gap by design, separating); latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 2 deg | clip_back/top_shell 0.01, base/top_shell 0.02, clip_front/top_shell 0.02, base/pmma_frame 0.1, base/pcb 0.12, base/header 0.34 | USB-C web on the PCB edge (zero gap by design, separating); latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 3 deg | clip_front/top_shell 0.01, clip_back/top_shell 0.01, base/top_shell 0.03, base/pmma_frame 0.14, base/pcb 0.15, base/header 0.34 | USB-C web on the PCB edge (zero gap by design, separating); latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 4 deg | clip_back/top_shell 0.01, base/top_shell 0.04, clip_front/top_shell 0.11, base/pcb 0.19, base/pmma_frame 0.19, base/header 0.35 | USB-C web on the PCB edge (zero gap by design, separating); latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 5 deg | clip_back/top_shell 0.01, base/top_shell 0.06, base/pmma_frame 0.23, base/pcb 0.24, base/header 0.35, base/usb_c 0.4 | latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 6 deg | clip_back/top_shell 0.02, base/top_shell 0.07, base/pmma_frame 0.28, base/pcb 0.31, base/header 0.34, base/usb_c 0.4 | latch hook riding over its catch (the snap); seam faces opening up about the hinge (separating) | none |
| 8 deg | base/top_shell 0.09, base/header 0.34, base/pmma_frame 0.37, base/usb_c 0.4, base/pcb 0.47, clip_back/top_shell 0.63 | seam faces opening up about the hinge (separating) | none |
| 10 deg | base/top_shell 0.13, base/header 0.35, base/usb_c 0.4, base/pmma_frame 0.46, base/pcb 0.65, base/misc_back 0.91 | seam faces opening up about the hinge (separating) | none |
| 12 deg | base/top_shell 0.16, base/header 0.35, base/usb_c 0.4, base/pmma_frame 0.56, base/pcb 0.8 | seam faces opening up about the hinge (separating) | none |
| 15 deg | base/top_shell 0.2, base/header 0.35, base/usb_c 0.41, base/pmma_frame 0.7, base/pcb 0.98 | - | none |
| 18 deg | base/top_shell 0.2, base/header 0.35, base/usb_c 0.51, base/pmma_frame 0.84 | - | none |
| 22 deg | base/top_shell 0.2, base/header 0.44, base/usb_c 0.79 | - | none |
| 26 deg | base/top_shell 0.2, base/usb_c 0.89 | - | none |
| 30 deg | base/top_shell 0.16, base/usb_c 0.89 | seam faces opening up about the hinge (separating) | none |

**Overall: PASS**
