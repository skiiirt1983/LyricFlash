# USB-C port: tolerance stack-up (option A)

The whole USB-C opening is in the **base tray**. The base doesn't take its position from the top
shell: it locates on the board itself, right at the port. Every link is a feature on the base
touching the receptacle or the PCB edge a few mm away. All numbers come from `params.py`, with
board values from the Waveshare STEP.

```
        top shell (glass in the bore, +/-0.15)        seam Z 3.0 (2.8 above the opening)
  ----------------------------------------------------------------------------
  base:  web (0.63 at centre) touches PCB/PMMA edge  -> sets X (flush)
         guide land, 0.10/side around the shell      -> sets Y
         support pad, 0.10 under the shell           -> sets Z (lower limit)
         backstop, 0.05 behind the receptacle        -> takes the plug force
         cups on the 3 M2 standoffs (0.2 radial)     -> clamps Z, coarse XY only
         spigot into top-shell bore (0.3 radial)     -> carries plug shear into the top shell
```

## Assumptions

| Item | Value | Source |
|---|---|---|
| Printed feature-to-feature error on one part, within ~5 mm | +/-0.05 | P2S, same part, same layers (assumption; coupon confirms) |
| Receptacle position on the PCB (vs PCB edge / standoffs) | +/-0.10 | **not in the STEP**; typical SMT placement. Coupon confirms |
| Opening clearance | 0.30/side | PLAN 3.7 |

## Y (across the opening)

| Link | Worst case |
|---|---|
| Guide land clearance around the shell | +/-0.10 |
| Land to opening (same part, same profile centre) | +/-0.05 |
| **Opening centre vs receptacle** | **+/-0.15** (requirement +/-0.20) |
| Smallest side gap left (0.30 - 0.15) | 0.15 |

The cups don't take part: their 0.2 radial clearance is larger than the land's, so the land wins.

## Z (up/down)

| Link | Worst case |
|---|---|
| Pad gap under the receptacle (nominal 0.10) | 0 to 0.20 |
| Pad to opening (same part) | +/-0.05 |
| Cup seat to pad (same part) | +/-0.05 |
| Standoff face to receptacle (board) | +/-0.10 |
| **Opening vs receptacle** | **+/-0.20** (requirement +/-0.20) |

If the real receptacle sits lower than the STEP, it rests on the pad. That's harmless: the pad
then supports the plug, which is what it's there for.

## X (flush: mouth vs outer face)

The web's inner face touches the PCB/PMMA edge with zero gap, and the backstop sits 0.05 behind
the receptacle. Between them, the board can only move 0.05 in X relative to the base.

| State | Nominal proud | Worst case |
|---|---|---|
| No plug (board against the web) | +0.15 | +0.00 .. +0.30 |
| Plug pushed in (board on the backstop) | +0.10 | -0.05 .. +0.25 |

Worst case = nominal +/- (0.10 board + 0.05 web print). The board term isn't in the STEP, so
**coupon C2 measures it**. If the mouth reads recessed, raise `USB_PROUD_TARGET` (up to 0.4 per
the fallback); if it reads too proud, lower it. Each 0.05 change moves the web the same 0.05.

## Forces

- **Plug push (+X):** receptacle -> backstop (0.05) -> base -> spigot -> top-shell bore. The
  solder joints see only the force needed to move the board 0.05 mm.
- **Overmould on the face:** web -> PCB/PMMA edge (zero gap), so the web is in compression
  instead of bending. The web spans the opening horizontally, along the layer lines.
- **Latches near the port (Paul's condition 4):** a plug push is in the seam plane, so the spigot
  carries it, not the latches. The only peel load comes from the push sitting 4.7 mm below the
  spigot. At a 20 N insertion force that's ~0.1 N·m, or ~2.3 N on the latch pair. Each latch's
  45° hook holds ~4–5 N in PETG (beam stiffness ~4.3 N/mm x 0.5 mm deflection), so the pair has
  about 4x margin. Coupon C2 checks this with real plug-ins.
