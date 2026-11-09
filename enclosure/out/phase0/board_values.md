# Board values extracted from the STEP

Board frame: origin = PCB centre on the PCB back face, +Z out of the glass, USB-C points to -X. Units mm / deg.

| Name | Value | Source |
|---|---|---|
| PCB_D | `46.028` mm | STEP 'BOARD' solid: max vertex radius x2 (outline is a 64-gon) |
| PCB_THK | `1.2` mm | STEP 'BOARD' solid: Z extent |
| PCB_BACK_Z | `0.0` mm | STEP 'BOARD' solid: back face (defines board-frame Z=0) |
| PCB_FRONT_Z | `1.2` mm | STEP 'BOARD' solid: front face |
| GLASS_D | `48.96` mm | STEP 'H0175Y003AMT003': largest cylindrical face (cover glass edge) |
| GLASS_TOP_Z | `6.9` mm | STEP 'H0175Y003AMT003': top face |
| GLASS_BOTTOM_Z | `5.8` mm | STEP 'H0175Y003AMT003': bottom of the glass edge cylinder |
| GLASS_THK | `1.1` mm | GLASS_TOP_Z - GLASS_BOTTOM_Z |
| GLASS_EDGE_R | `0.5` mm | STEP 'H0175Y003AMT003': torus face on the glass top edge (edge round) |
| GLASS_FLAT_D | `47.96` mm | GLASS_D - 2*GLASS_EDGE_R (flat part of the glass face) |
| PANEL_D | `46.0` mm | STEP 'H0175Y003AMT003': AMOLED panel cylinder under the glass |
| PANEL_BOTTOM_Z | `4.85` mm | STEP 'H0175Y003AMT003': bottom of the panel cylinder |
| DISPLAY_FPC_FOLD_YMAX | `24.07` mm | STEP 'H0175Y003AMT003' below the glass: +Y extent (display flex fold) |
| DISPLAY_FPC_FOLD_YMIN | `-23.88` mm | STEP 'H0175Y003AMT003' below the glass: -Y extent (display flex fold) |
| DISPLAY_UNDER_GLASS_RMAX | `24.515` mm | STEP 'H0175Y003AMT003' below the glass: max vertex radius (flex fold corners) |
| ACTIVE_AREA_D | **TODO** | NOT IN STEP: the display is one solid, no active-area face. Ask Paul. |
| PMMA_D | `46.0` mm | STEP 'ESP32-S3-TOUCH-AMOLED-1_75-PMMA': X extent (frame between PCB and panel) |
| PMMA_Z | `(1.2, 4.85)` mm | STEP 'ESP32-S3-TOUCH-AMOLED-1_75-PMMA': Z range |
| PMMA_FLAT_Y | `-17.8` mm | STEP 'ESP32-S3-TOUCH-AMOLED-1_75-PMMA': flat cut on the header side |
| BOARD_MIN_Z | `-5.8` mm | STEP: lowest point of any part ('header') |
| BOARD_TOTAL_THK | `12.7` mm | GLASS_TOP_Z - BOARD_MIN_Z (glass face to tallest back part) |
| BOARD_MIN_Z_NO_HEADER | `-4.8` mm | STEP: lowest point excluding the 8-pin header |
| MOUNT_HOLES_XY | `[(-13.75, -14.698), (-0.0, 20.501), (13.75, -14.701)]` mm | STEP 'BOARD': three R1.1 cylindrical holes |
| MOUNT_HOLE_D | `2.2` mm | STEP 'BOARD': hole cylinder radius x2 |
| STANDOFF_OD | `3.5` mm | STEP 'SMTSO-M2-3_5ET': X extent (M2 SMT standoff) |
| STANDOFF_Z | `(-3.5, 1.0)` mm | STEP 'SMTSO-M2-3_5ET': Z range (bottom = screw face) |
| EDGE_NOTCHES | `[(12.768, -18.376), (-15.057, 16.569)]` mm | STEP 'BOARD': outline arcs fitted to circles (half-round locating notches) |
| EDGE_NOTCH_D | `1.944` mm | STEP 'BOARD': fitted notch circle diameter (mean) |
| BUTTON_FLAT1_ANGLE | `-34.004` deg | STEP 'BOARD': outward normal of the straight outline cut, deg from +X |
| BUTTON_FLAT1_DIST | `21.801` mm | STEP 'BOARD': distance of that cut from board centre |
| BUTTON_FLAT2_ANGLE | `33.996` deg | STEP 'BOARD': outward normal of the straight outline cut, deg from +X |
| BUTTON_FLAT2_DIST | `21.801` mm | STEP 'BOARD': distance of that cut from board centre |
| USB_DIR | `-X` mm | STEP 'H1 / 698B116-264-211': receptacle mouth faces -X |
| USB_MOUTH_X | `-23.794` mm | STEP 'H1': front face of the shell (min X of the receptacle) |
| USB_MOUTH_PROUD_OF_PCB | `0.78` mm | -(USB_MOUTH_X) - PCB_D/2 |
| USB_SHELL_W | `8.94` mm | STEP 'H1': shell section 0.3 mm behind the mouth, Y extent |
| USB_SHELL_H | `3.16` mm | STEP 'H1': same section, Z extent |
| USB_CENTER_Y | `-0.02` mm | STEP 'H1': mouth section centre Y |
| USB_CENTER_Z | `-1.67` mm | STEP 'H1': mouth section centre Z |
| USB_SHELL_REAR_X | `-16.086` mm | STEP 'H1': rear end of the receptacle (backstop goes here) |
| USB_ENVELOPE | `((-23.794, -4.81, -3.25), (-16.086, 4.77, 0.91))` mm | STEP 'H1': bbox of all receptacle solids incl. legs/tabs |
| USB_GLASS_OVERHANG | `0.686` mm | GLASS_D/2 + USB_MOUTH_X: glass edge sticks out past the mouth by this |
| KEY1_BBOX | `((14.735, -13.796, -1.8), (19.441, -8.829, 0.4))` mm | STEP 'KEY1_ASM / SWITCH-TS24CA': bbox |
| KEY1_PRESS_DIR | `(-0.819, 0.574)` | STEP 'KEY1': inward normal of the plunger tip face (side-push, in board plane) |
| KEY1_PRESS_ANGLE | `-35.005` deg | STEP 'KEY1': outward plunger normal, deg from +X |
| KEY1_TIP_CENTER | `(18.907, -12.7, -0.9)` mm | STEP 'KEY1': centre of the plunger tip face |
| KEY1_TIP_DIST | `22.772` mm | STEP 'KEY1': tip face distance from board centre along the press axis |
| KEY1_PLUNGER_PROUD | `1.2` mm | STEP 'KEY1': tip face to switch body face |
| KEY1_TIP_Z | `(-1.3, -0.5)` mm | STEP 'KEY1': plunger tip Z range |
| KEY1_TIP_W | `1.8` mm | STEP 'KEY1': plunger tip width in the board plane |
| KEY2_BBOX | `((14.845, 8.82, -1.8), (19.539, 13.801, 0.4))` mm | STEP 'KEY2_ASM / SWITCH-TS24CA': bbox |
| KEY2_PRESS_DIR | `(-0.829, -0.559)` | STEP 'KEY2': inward normal of the plunger tip face (side-push, in board plane) |
| KEY2_PRESS_ANGLE | `33.993` deg | STEP 'KEY2': outward plunger normal, deg from +X |
| KEY2_TIP_CENTER | `(19.019, 12.664, -0.9)` mm | STEP 'KEY2': centre of the plunger tip face |
| KEY2_TIP_DIST | `22.849` mm | STEP 'KEY2': tip face distance from board centre along the press axis |
| KEY2_PLUNGER_PROUD | `1.2` mm | STEP 'KEY2': tip face to switch body face |
| KEY2_TIP_Z | `(-1.3, -0.5)` mm | STEP 'KEY2': plunger tip Z range |
| KEY2_TIP_W | `1.8` mm | STEP 'KEY2': plunger tip width in the board plane |
| KEY_TRAVEL | `0.25` mm | TODO verify by test print: not in the STEP (static model); 0.25 mm default for a side tact switch |
| PWR_IS | `KEY2` | ANSWERED: Paul's photo 2026-11-09: silkscreen 'BOOT' is next to KEY1, so PWR = KEY2 |
| MIC1_CENTER | `(-17.54, 10.959)` mm | STEP 'MIC1_ASM': centre of the package footprint |
| MIC1_SIZE | `(2.599, 3.4, 0.998)` mm | STEP 'MIC1_ASM': package edge lengths and height |
| MIC1_Z | `(-1.1, -0.102)` mm | STEP 'MIC1_ASM': Z range (on the PCB back) |
| MIC1_ANGLE | `22.592` deg | STEP 'MIC1_ASM': footprint rotation (diag. corner to corner) |
| MIC2_CENTER | `(-17.35, -11.738)` mm | STEP 'MIC2_ASM': centre of the package footprint |
| MIC2_SIZE | `(2.599, 3.399, 0.998)` mm | STEP 'MIC2_ASM': package edge lengths and height |
| MIC2_Z | `(-1.1, -0.102)` mm | STEP 'MIC2_ASM': Z range (on the PCB back) |
| MIC2_ANGLE | `-17.601` deg | STEP 'MIC2_ASM': footprint rotation (diag. corner to corner) |
| MIC_PORT_SIDE | `top` | ANSWERED: Paul 2026-11-09: both mic lids have a 0.3-0.5 mm hole (top-port, faces away from the PCB) |
| J1_BBOX | `((-10.631, 15.919, -4.8), (-2.981, 20.119, 0.0))` mm | STEP: J1 MX1.25 2P vertical bbox |
| H3_BBOX | `((2.67, 15.918, -4.8), (10.32, 20.118, 0.0))` mm | STEP: H3 MX1.25 2P vertical bbox |
| MICROSD_BBOX | `((7.007, -7.8, 0.6), (22.01, 8.3, 3.05))` mm | STEP: microSD socket (front side, under display) bbox |
| HEADER_BBOX | `((-10.428, -19.88, -5.8), (10.422, -17.479, 3.0))` mm | STEP: 8-pin 2.54 female header bbox |
| DISPLAY_FPC_BBOX | `((-18.9, -0.423, 1.6), (-1.699, 9.177, 3.7))` mm | STEP: display FPC bbox |
| BATTERY_CONNECTOR | `J1` | ANSWERED: Paul's photo 2026-11-09: silkscreen 'BAT' at J1, 'SPK' at H3 |
| HEADER_PITCH | `2.54` mm | STEP 'BOARD': spacing of the 8 header holes |
| BOOT_IS | `KEY1` | ANSWERED: Paul's photo 2026-11-09: silkscreen 'BOOT' next to KEY1 |
| SPEAKER_CONNECTOR | `H3` | ANSWERED: Paul's photo 2026-11-09: silkscreen 'SPK' at H3 (nothing plugged in) |
| BEZEL_STYLE | `A` | ANSWERED: Paul 2026-11-09: option A, PLAN 3.1 lip, board loads from behind, base clamps it forward |
