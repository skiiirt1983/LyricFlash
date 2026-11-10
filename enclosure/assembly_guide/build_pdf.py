import os
HERE = os.path.dirname(os.path.abspath(__file__))
from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import HexColor, white
from reportlab.lib.utils import ImageReader

pdfmetrics.registerFont(TTFont("Sans", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
pdfmetrics.registerFont(TTFont("SansB", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"))

IMG = os.path.join(HERE, "img") + "/"
OUT = os.path.join(HERE, "LyricFlash_Assembly_Guide.pdf")
W, H = letter
M = 40
INK = HexColor("#1d2330")
GREY = HexColor("#6b7280")
LIGHT = HexColor("#f3f4f6")
GOLD = HexColor("#c9962e")
ORANGE = HexColor("#f08a24")
BLUE = HexColor("#1f6fd1")
RED = HexColor("#c0392b")

c = canvas.Canvas(OUT, pagesize=letter)
c.setTitle("LyricFlash Case: Assembly Guide")
c.setAuthor("Paul / Peakform")
page = [0]


def img(name, x, y, w=None, h=None, anchor="sw"):
    ir = ImageReader(IMG + name)
    iw, ih = ir.getSize()
    if w and h:
        s = min(w / iw, h / ih)
    elif w:
        s = w / iw
    else:
        s = h / ih
    dw, dh = iw * s, ih * s
    if anchor == "c":           # centre inside the (w, h) box whose lower-left is (x, y)
        x = x + ((w or dw) - dw) / 2
        y = y + ((h or dh) - dh) / 2
    c.drawImage(ir, x, y, dw, dh, mask="auto")
    return dw, dh


def wrap(text, font, size, width):
    lines = []
    for para in text.split("\n"):
        words = para.split(" ")
        cur = ""
        for w_ in words:
            t = (cur + " " + w_).strip()
            if pdfmetrics.stringWidth(t, font, size) <= width:
                cur = t
            else:
                lines.append(cur)
                cur = w_
        lines.append(cur)
    return lines


def para(text, x, y, width, size=10.5, font="Sans", color=INK, lead=1.38):
    c.setFont(font, size)
    c.setFillColor(color)
    for ln in wrap(text, font, size, width):
        c.drawString(x, y, ln)
        y -= size * lead
    return y


def bullets(items, x, y, width, size=10.5, color=INK, gap=4, dot=ORANGE):
    for it in items:
        bold = None
        if it.startswith("!"):
            it = it[1:]
            bold = RED
        c.setFillColor(bold or dot)
        c.circle(x + 3, y + size * 0.33, 2.6, stroke=0, fill=1)
        y = para(it, x + 13, y, width - 13, size, "SansB" if bold else "Sans", bold or color)
        y -= gap
    return y


def footer():
    page[0] += 1
    c.setFont("Sans", 8)
    c.setFillColor(GREY)
    c.drawString(M, 22, "LyricFlash case: assembly guide · design v9 (2026-11-10)")
    c.drawRightString(W - M, 22, str(page[0]))


def header(title, sub=None):
    c.setFillColor(INK)
    c.rect(0, H - 62, W, 62, stroke=0, fill=1)
    c.setFillColor(GOLD)
    c.rect(0, H - 66, W, 4, stroke=0, fill=1)
    c.setFillColor(white)
    c.setFont("SansB", 20)
    c.drawString(M, H - 40, title)
    if sub:
        c.setFont("Sans", 10)
        c.setFillColor(HexColor("#cbd5e1"))
        c.drawRightString(W - M, H - 38, sub)


def step_head(n, title, y, optional=False):
    c.setFillColor(ORANGE)
    c.circle(M + 16, y + 6, 16, stroke=0, fill=1)
    c.setFillColor(white)
    c.setFont("SansB", 16)
    c.drawCentredString(M + 16, y, str(n))
    c.setFillColor(INK)
    c.setFont("SansB", 16)
    c.drawString(M + 42, y, title)
    if optional:
        tw = pdfmetrics.stringWidth(title, "SansB", 16)
        c.setFillColor(GREY)
        c.setFont("Sans", 10)
        c.drawString(M + 50 + tw, y + 1, "(optional)")
    return y - 26


def note(text, x, y, w, color=BLUE, fill=HexColor("#eef4fd"), size=9.5):
    lines = wrap(text, "Sans", size, w - 22)
    h = len(lines) * size * 1.38 + 14
    c.setFillColor(fill)
    c.setStrokeColor(color)
    c.setLineWidth(1)
    c.roundRect(x, y - h, w, h, 6, stroke=1, fill=1)
    c.setFillColor(color)
    c.rect(x, y - h, 4, h, stroke=0, fill=1)
    yy = y - 9 - size * 0.8
    c.setFont("Sans", size)
    c.setFillColor(INK)
    for ln in lines:
        c.drawString(x + 14, yy, ln)
        yy -= size * 1.38
    return y - h - 8


def box(x, y, w, h, fill=LIGHT):
    c.setFillColor(fill)
    c.roundRect(x, y, w, h, 8, stroke=0, fill=1)


# =============================================================== page 1: cover
c.setFillColor(INK)
c.rect(0, H - 210, W, 210, stroke=0, fill=1)
c.setFillColor(GOLD)
c.rect(0, H - 214, W, 4, stroke=0, fill=1)
c.setFillColor(white)
c.setFont("SansB", 34)
c.drawString(M, H - 92, "LyricFlash Case")
c.setFont("Sans", 18)
c.setFillColor(HexColor("#e5e7eb"))
c.drawString(M, H - 122, "Assembly guide")
c.setFont("Sans", 10.5)
c.setFillColor(HexColor("#cbd5e1"))
c.drawString(M, H - 160, "For the Waveshare ESP32-S3-Touch-AMOLED-1.75 · 20° tilted desk case · design v9")
c.drawString(M, H - 176, "7 steps · no screws, no glue")
img("hero.png", M, 250, W - 2 * M, H - 214 - 270, anchor="c")
# bottom strip
box(M, 60, W - 2 * M, 170)
img("hero_back.png", M + 10, 70, 230, 150, anchor="c")
y = para("How it goes together", M + 260, 205, 260, 13, "SansB")
y = bullets(["Small parts go into the black base: switch, two latch clips, battery.",
             "The display board drops glass-first into the gold top shell.",
             "The base hooks onto the USB-C port and swings shut. Two clips snap.",
             "The cable takes one turn round the cleat on the back."], M + 262, y - 4, 268, 9.8, gap=2)
footer()
c.showPage()

# =============================================================== page 2: parts
header("What you need", "Parts, tools and print settings")
y0 = H - 92
c.setFont("SansB", 13)
c.setFillColor(INK)
c.drawString(M, y0, "Printed parts")
cards = [("part_top.png", "Top shell", "Gold PLA Metal · ×1", "Holds the screen"),
         ("part_base.png", "Base", "Black PETG · ×1", "Ribbed, panels or smooth; with or without the cleat"),
         ("part_clip_back.png", "Latch clip", "PETG · ×2 · 6 mm wide", "Both clips are the same part")]
cw = (W - 2 * M - 2 * 10) / 3
for i, (f, t, s1, s2) in enumerate(cards):
    x = M + i * (cw + 10)
    box(x, y0 - 205, cw, 190)
    img(f, x + 8, y0 - 120, cw - 16, 100, anchor="c")
    c.setFillColor(INK)
    c.setFont("SansB", 10.5)
    c.drawString(x + 10, y0 - 138, t)
    c.setFont("Sans", 8.5)
    c.setFillColor(GREY)
    para(s1, x + 10, y0 - 153, cw - 20, 8.5, "Sans", GREY)
    para(s2, x + 10, y0 - 168, cw - 20, 8.5, "Sans", GREY, 1.3)

y1 = y0 - 232
c.setFont("SansB", 13)
c.setFillColor(INK)
c.drawString(M, y1, "Electronics")
cards = [("board_back.png", "Display board", "Waveshare ESP32-S3-Touch-AMOLED-1.75", "Battery plug J1"),
         ("part_battery.png", "LiPo battery", "3.7 V 2000 mAh · 56 × 34.5 × 10.3 mm", "With the plug that fits J1"),
         ("part_switch.png", "Slide switch", "SS-12D00 (optional)", "Hard power-off for storage")]
for i, (f, t, s1, s2) in enumerate(cards):
    x = M + i * (cw + 10)
    box(x, y1 - 195, cw, 180)
    img(f, x + 8, y1 - 110, cw - 16, 90, anchor="c")
    c.setFillColor(INK)
    c.setFont("SansB", 10.5)
    c.drawString(x + 10, y1 - 128, t)
    para(s1, x + 10, y1 - 143, cw - 20, 8.5, "Sans", GREY, 1.3)
    para(s2, x + 10, y1 - 167, cw - 20, 8.5, "Sans", GREY, 1.3)

y2 = y1 - 222
colw = (W - 2 * M - 20) / 2
c.setFont("SansB", 13)
c.setFillColor(INK)
c.drawString(M, y2, "Tools")
bullets(["Soft cloth (the screen goes face-down on it)",
         "Multimeter (battery polarity check)",
         "Flush cutters or a deburring tool (support clean-up)",
         "Only for the switch: soldering iron and heat-shrink"], M, y2 - 22, colw, 9.8, gap=2)
x2 = M + colw + 20
c.setFont("SansB", 13)
c.drawString(x2, y2, "Print settings")
bullets(["0.2 mm layers, 4 walls, 15 % gyroid, 0.4 mm nozzle",
         "Top shell: face-down (screen ring on the plate), no supports",
         "Base: floor down, supports off. Paint a support enforcer on the top edge of the USB-C opening only.",
         "Clips: two, flat on the plate, PETG (they need to flex)"], x2, y2 - 22, colw, 9.8, gap=2)
footer()
c.showPage()

# =============================================================== page 3: map
header("Where everything goes", "Base seen from above, and the three layers")
img("map.png", M - 10, 150, 320, 540, anchor="c")
img("exploded.png", M + 300, 150, W - 2 * M - 300, 540, anchor="c")
y = note("The case has three layers: the gold top shell holds the screen, the board sits under it, and the black "
         "base holds everything else. The battery stands on its edge along the right wall. The USB-C port and both "
         "microphone holes are on the left side, and the cable cleat is on the back.", M, 140, W - 2 * M, BLUE)
footer()
c.showPage()

# =============================================================== page 4: before you start
header("Before you start", "Five quick checks")
y = H - 100
checks = [
    ("Clean the USB-C opening", "Remove all the painted support from inside the opening. Hold the bare board's USB-C "
     "socket up to it: it should slide in freely."),
    ("Check the two mic holes", "Two Ø2 mm holes, one either side of the USB-C opening. They must be clear all the way "
     "through. LyricFlash listens through them."),
    ("Check the battery polarity", "Use a meter before you plug the battery in. Red/black order isn't the same from "
     "every seller, and a reversed LiPo can kill the board's power chip."),
    ("Know what isn't reachable", "There's no access to the PWR or BOOT buttons. The board powers up from USB or the "
     "battery, and the switch turns it off. To flash in recovery mode, open the case."),
    ("Never press on the screen", "Every push in this guide goes plastic-to-plastic. The board is clamped, so no "
     "force should go through the glass."),
]
for i, (t, d) in enumerate(checks):
    h = 78
    warn = i == 2
    box(M, y - h, W - 2 * M, h, HexColor("#fdecea") if warn else LIGHT)
    c.setFillColor(RED if warn else INK)
    c.roundRect(M + 14, y - 44, 22, 22, 4, stroke=0, fill=1)
    c.setFillColor(white)
    c.setFont("SansB", 12)
    c.drawCentredString(M + 25, y - 38, "!" if warn else "✓")
    c.setFillColor(RED if warn else INK)
    c.setFont("SansB", 12.5)
    c.drawString(M + 50, y - 26, t)
    para(d, M + 50, y - 44, W - 2 * M - 70, 10, "Sans", INK)
    y -= h + 12
y = note("Which base? All six bases (ribbed, panels or smooth, each with or without the cleat) assemble the same way. "
         "Bases without the cleat skip step 7.", M, y - 4, W - 2 * M, GOLD, HexColor("#fbf5e6"))
footer()
c.showPage()

# =============================================================== page 5: steps 1-2
header("Into the base", "Steps 1–2")
y = H - 100
y = step_head(1, "Slide switch", y, optional=True)
colw = 230
bullets(["Solder the switch into the battery's red (+) lead. Insulate both joints.",
         "Slide it sideways into the floor pocket at the back, just left of the battery's slot, coming from the battery side.",
         "The knob points down through the slot in the floor. Nothing is pressed in: the end lips hold it.",
         "Not fitting a switch? Skip to step 2."], M, y - 4, colw, 10, gap=3)
img("step_switch.png", M + colw + 15, y - 205, 160, 215, anchor="c")
img("step_switch_under.png", M + colw + 185, y - 205, W - 2 * M - colw - 185, 215, anchor="c")
y -= 240
c.setStrokeColor(HexColor("#e5e7eb"))
c.line(M, y, W - M, y)
y -= 30
y = step_head(2, "Latch clips", y)
bullets(["Both clips are the same 6 mm part.",
         "One goes into the channel on the back wall, left of the middle. The other goes into the channel on the left part of the front wall.",
         "Slide each clip in sideways, with the hook facing the outer wall and the beam pointing up. No force needed."],
        M, y - 4, colw, 10, gap=3)
img("step_clips.png", M + colw + 15, y - 215, W - 2 * M - colw - 15, 225, anchor="c")
img("step_clip_back_zoom.png", M, y - 268, 110, 95, anchor="c")
img("step_clip_front_zoom.png", M + 120, y - 268, 110, 95, anchor="c")
c.setFont("Sans", 8.5)
c.setFillColor(GREY)
c.drawCentredString(M + 55, y - 279, "Back clip, seated")
c.drawCentredString(M + 175, y - 279, "Front clip, seated")
footer()
c.showPage()

# =============================================================== page 6: step 3
header("Into the base", "Step 3")
y = H - 100
y = step_head(3, "Battery", y)
bullets(["Stand the battery on its thin edge and lower it straight down into the slot along the right wall.",
         "The leads go at the back end, at the top.",
         "Nothing hangs over the slot, so it goes in without tilting. It should drop in by hand, with no forcing.",
         "The battery's left side closes off the switch pocket, which keeps the switch in place."],
        M, y - 4, W - 2 * M, 10, gap=3)
img("step_battery.png", M, 70, W - 2 * M, y - 100 - 70, anchor="c")
footer()
c.showPage()

# =============================================================== page 7: step 4
header("Board into the top shell", "Step 4")
y = H - 100
y = step_head(4, "Board into the top shell", y)
bullets(["Put a soft cloth on the table and lay the gold top shell face-down on it.",
         "Drop the board in glass-first. The glass rests on the lip. Turn it so the USB-C port points to the case's left side, where the base has its USB-C opening."],
        M, y - 4, W - 2 * M, 10, gap=3)
img("step_board.png", M, y - 286, W - 2 * M, 230, anchor="c")
y -= 300
bullets(["Plug the battery into J1. There is no speaker in this version.",
         "!Check the battery polarity with a meter first (see Before you start)."], M, y, W - 2 * M, 10, gap=3)
img("step_plugs.png", M, y - 296, (W - 2 * M) / 2 - 8, 230, anchor="c")
img("board_back.png", M + (W - 2 * M) / 2 + 8, y - 296, (W - 2 * M) / 2 - 8, 230, anchor="c")
c.setFont("Sans", 8.5)
c.setFillColor(GREY)
c.drawCentredString(M + (W - 2 * M) * 0.25, y - 308, "Seated, glass down: J1 highlighted")
c.drawCentredString(M + (W - 2 * M) * 0.75, y - 308, "The real board, back side")
footer()
c.showPage()

# =============================================================== page 8: step 5 + opening
header("Close the case", "Step 5")
y = H - 100
y = step_head(5, "Hook on at the USB-C, then swing shut", y)
bullets(["Hold the base upside down over the board.",
         "Hook the base's USB-C opening over the board's USB-C socket first.",
         "Then swing the other end down. It should come down under its own weight until the latches touch, without "
         "rubbing anywhere on the way.",
         "Tuck the battery wires in as it closes so they don't get pinched at the seam."],
        M, y - 4, W - 2 * M, 10, gap=3)
y -= 120
img("step_close_a.png", M, y - 280, (W - 2 * M) / 2 - 8, 280, anchor="c")
img("step_close_b.png", M + (W - 2 * M) / 2 + 8, y - 280, (W - 2 * M) / 2 - 8, 280, anchor="c")
y -= 300
y = note("If it won't close by itself, don't force it. Lift it off and look for a pinched wire or a clip that's "
         "sticking up out of its channel.", M, y, W - 2 * M, ORANGE, HexColor("#fef3e8"))
y -= 6
c.setFont("SansB", 12)
c.setFillColor(INK)
c.drawString(M, y, "Opening the case later")
bullets(["Lay the case face-down on the cloth and pull the base's right end up to unsnap both latches.",
         "Swing it off, pivoting at the USB-C port. Lift the battery straight out."], M, y - 18, W - 2 * M, 10, gap=2)
footer()
c.showPage()

# =============================================================== page 9: steps 6-7
header("Lock it and route the cable", "Steps 6–7")
y = H - 100
y = step_head(6, "Snap the two latches", y)
bullets(["Squeeze the base and the gold shell together at the two latch spots: the back wall, left of the middle, "
         "and the left part of the front wall.",
         "Each latch clicks. Don't push on the middle of the base.",
         "Check: no rattle, and the seam is even all round."], M, y - 4, colw, 10, gap=3)
img("step_latches.png", M + colw + 15, y - 235, W - 2 * M - colw - 15, 250, anchor="c")
y -= 255
c.setStrokeColor(HexColor("#e5e7eb"))
c.line(M, y, W - M, y)
y -= 30
y = step_head(7, "Cable cleat", y)
bullets(["Plug in the USB-C cable and bring it round to the back.",
         "Give it one turn round the cleat at the bottom of the back wall: under one horn, over the other.",
         "A tug on the cable now pulls on the cleat, not the USB-C plug.",
         "No cleat on your base? Nothing to do."], M, y - 4, colw, 10, gap=3)
img("step_cleat.png", M + colw + 15, y - 235, W - 2 * M - colw - 15, 245, anchor="c")
footer()
c.showPage()
c.save()
print("saved", OUT)
