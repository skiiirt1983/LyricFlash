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
    c.drawString(M, 22, "LyricFlash case: assembly guide · design v10.1 (2026-11-11)")
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
c.drawString(M, H - 160, "For the Waveshare ESP32-S3-Touch-AMOLED-1.75 · 20° tilted desk case · design v10.1")
c.drawString(M, H - 176, "9 steps · 6 screws, 8 magnets · no glue")
img("hero.png", M, 250, W - 2 * M, H - 214 - 270, anchor="c")
box(M, 60, W - 2 * M, 170)
img("hero_back.png", M + 10, 70, 230, 150, anchor="c")
y = para("How it goes together", M + 260, 205, 260, 13, "SansB")
y = bullets(["Magnets, the switch and the battery go into the black base.",
             "The display board screws onto a printed carrier plate, on the bench.",
             "Board and plate go into the base USB-C first; three screws hold the plate down.",
             "The gold shell goes on last, held by the magnets. The cable turns round the cleat."], M + 262, y - 4, 268, 9.8, gap=2)
footer()
c.showPage()

# =============================================================== page 2: parts
header("What you need", "Parts, hardware, tools and print settings")
y0 = H - 92
c.setFont("SansB", 13)
c.setFillColor(INK)
c.drawString(M, y0, "Printed parts")
cards = [("part_top.png", "Top shell", "Gold PLA Metal · ×1", "Holds the screen, 4 magnets"),
         ("part_base.png", "Base", "Black PETG · ×1", "Ribbed, panels or smooth; with or without the cleat"),
         ("part_carrier.png", "Carrier plate", "Black PETG · ×1", "The board screws to it")]
cw = (W - 2 * M - 2 * 10) / 3
for i, (f, t, s1, s2) in enumerate(cards):
    x = M + i * (cw + 10)
    box(x, y0 - 185, cw, 170)
    img(f, x + 8, y0 - 110, cw - 16, 90, anchor="c")
    c.setFillColor(INK)
    c.setFont("SansB", 10.5)
    c.drawString(x + 10, y0 - 128, t)
    para(s1, x + 10, y0 - 143, cw - 20, 8.5, "Sans", GREY)
    para(s2, x + 10, y0 - 158, cw - 20, 8.5, "Sans", GREY, 1.3)

y1 = y0 - 205
c.setFont("SansB", 13)
c.setFillColor(INK)
c.drawString(M, y1, "Electronics")
cards = [("board_back.png", "Display board", "Waveshare ESP32-S3-Touch-AMOLED-1.75", "Battery plug J1"),
         ("part_battery.png", "LiPo battery", "3.7 V 2000 mAh · 56 × 34.5 × 10.3 mm", "With the plug that fits J1"),
         ("part_switch.png", "Slide switch", "SS-12D00 (optional)", "Hard power-off for storage")]
for i, (f, t, s1, s2) in enumerate(cards):
    x = M + i * (cw + 10)
    box(x, y1 - 175, cw, 160)
    img(f, x + 8, y1 - 100, cw - 16, 80, anchor="c")
    c.setFillColor(INK)
    c.setFont("SansB", 10.5)
    c.drawString(x + 10, y1 - 118, t)
    para(s1, x + 10, y1 - 133, cw - 20, 8.5, "Sans", GREY, 1.3)
    para(s2, x + 10, y1 - 157, cw - 20, 8.5, "Sans", GREY, 1.3)

y2 = y1 - 195
colw = (W - 2 * M - 20) / 2
c.setFont("SansB", 13)
c.setFillColor(INK)
c.drawString(M, y2, "Hardware")
yb = bullets(["3× M2 × 3 flat head (countersunk): board to the plate",
              "3× M2.5 × 6 pan head: plate to the base (they cut their own thread, no nuts)",
              "8× 4 × 2 mm disc magnets: 4 in the base, 4 in the shell"], M, y2 - 22, colw, 9.8, gap=2)
c.setFont("SansB", 13)
c.setFillColor(INK)
c.drawString(M, yb - 10, "Tools")
bullets(["Soft cloth (the screen goes face-down on it)",
         "Small Phillips / hex driver for M2 and M2.5",
         "Multimeter (battery polarity check)",
         "Flush cutters (support clean-up)",
         "Only for the switch: soldering iron and heat-shrink"], M, yb - 32, colw, 9.8, gap=2)
x2 = M + colw + 20
c.setFont("SansB", 13)
c.drawString(x2, y2, "Print settings")
bullets(["0.2 mm layers, 4 walls, 15 % gyroid, 0.4 mm nozzle",
         "Top shell: face-down (screen ring on the plate), no supports",
         "Base: floor down, supports off. Paint a support enforcer on the top edge of the USB-C opening only.",
         "Carrier plate: flat, board side down (countersinks facing up), no supports"], x2, y2 - 22, colw, 9.8, gap=2)
footer()
c.showPage()

# =============================================================== page 3: map
header("Where everything goes", "Base seen from above, and the three layers")
img("map.png", M - 10, 150, 320, 540, anchor="c")
img("exploded.png", M + 300, 150, W - 2 * M - 300, 540, anchor="c")
note("Three layers: the gold top shell, the display board on its carrier plate, and the black base. The battery "
     "stands on its edge right of centre. The switch is in the left wall under the USB-C port and the two microphone "
     "holes. Magnets sit in all four corners, and the plate screws to three posts on the left and at the back.",
     M, 140, W - 2 * M, BLUE)
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
    ("Never press on the screen", "Every push and every screw in this guide goes plastic-to-plastic, outside the "
     "screen. The shell's lip stands clear of the glass, so even the magnets don't press on it."),
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
note("Which base? All six bases (ribbed, panels or smooth, each with or without the cleat) assemble the same way. "
     "Bases without the cleat skip step 9.", M, y - 4, W - 2 * M, GOLD, HexColor("#fbf5e6"))
footer()
c.showPage()

# =============================================================== page 5: steps 1-2
header("Into the base", "Steps 1–2")
colw = 230
y = H - 100
y = step_head(1, "Magnets", y)
bullets(["Press one 4 × 2 magnet into each corner pocket of the base, flush with the top. Do the same in the four "
         "corner pockets on the underside of the gold shell.",
         "!Check polarity first: stack each shell magnet on its base magnet so they attract, mark the faces that meet, "
         "and press them in that way round.",
         "A snug press-fit. A drop of CA glue if one is loose."], M, y - 4, colw, 10, gap=3)
img("step_magnets.png", M + colw + 15, y - 225, W - 2 * M - colw - 15, 235, anchor="c")
y -= 250
c.setStrokeColor(HexColor("#e5e7eb"))
c.line(M, y, W - M, y)
y -= 30
y = step_head(2, "Slide switch", y, optional=True)
bullets(["Solder the switch into the battery's red (+) lead. Insulate both joints.",
         "From inside the case, push it into its pocket in the left wall under the USB-C, knob first through the slot.",
         "It's a firm press-fit, which is meant to be. The knob ends up about 1.85 mm out of the wall and slides end to end.",
         "Not fitting a switch? Skip to step 3."], M, y - 4, colw, 10, gap=3)
img("step_switch.png", M + colw + 15, y - 215, 150, 215, anchor="c")
img("step_switch_out.png", M + colw + 175, y - 215, W - 2 * M - colw - 175, 215, anchor="c")
footer()
c.showPage()

# =============================================================== page 6: step 3
header("Into the base", "Step 3")
y = H - 100
y = step_head(3, "Battery", y)
bullets(["Stand the battery on its thin edge and lower it straight down into its slot, right of centre.",
         "The leads go at the back end, at the top.",
         "Nothing hangs over the slot, so it goes in without tilting. It should drop in by hand, with no forcing."],
        M, y - 4, W - 2 * M, 10, gap=3)
img("step_battery.png", M, 70, W - 2 * M, y - 100 - 70, anchor="c")
footer()
c.showPage()

# =============================================================== page 7: steps 4-5
header("Board onto its plate", "Steps 4–5")
y = H - 100
y = step_head(4, "Screw the board to the carrier plate (on the bench)", y)
bullets(["Lay the board glass-side down on a soft cloth.",
         "Put the carrier plate on its back, countersinks facing up, holes over the board's three metal standoffs.",
         "Fit the three M2 × 3 flat heads. Snug them, don't crank them: they go into the board's own standoffs."],
        M, y - 4, W - 2 * M, 10, gap=3)
img("step_carrier.png", M, y - 330, W - 2 * M, 270, anchor="c")
y -= 350
y = step_head(5, "Plug in the battery", y)
bullets(["Plug the battery into J1 on the back of the board.",
         "!Check the battery polarity with a meter first (see Before you start)."], M, y - 4, (W - 2 * M) / 2 - 10, 10, gap=3)
img("board_back.png", M + (W - 2 * M) / 2 + 8, y - 160, (W - 2 * M) / 2 - 8, 170, anchor="c")
footer()
c.showPage()

# =============================================================== page 8: step 6
header("Board into the base", "Step 6")
y = H - 100
y = step_head(6, "USB-C first, then lower it down", y)
bullets(["Hold the board by its plate, glass up, tilted.",
         "Put the board's USB-C socket into its opening in the left wall first.",
         "Then lower the right side down. The three plate tabs land on their posts. Nothing should rub on the way.",
         "Keep the battery wires clear so they don't get pinched under the plate."],
        M, y - 4, W - 2 * M, 10, gap=3)
y -= 120
img("step_board_in_a.png", M, y - 330, (W - 2 * M) / 2 - 8, 330, anchor="c")
img("step_board_in_b.png", M + (W - 2 * M) / 2 + 8, y - 330, (W - 2 * M) / 2 - 8, 330, anchor="c")
y -= 350
note("If it doesn't settle by itself, don't push on the screen. Lift it out and look for a pinched wire.",
     M, y, W - 2 * M, ORANGE, HexColor("#fef3e8"))
footer()
c.showPage()

# =============================================================== page 9: steps 7-8
header("Fix it and close it", "Steps 7–8")
y = H - 100
y = step_head(7, "Screw the plate down", y)
bullets(["Three M2.5 × 6 pan heads, straight down through the plate's tabs into the posts. All three sit outside the screen.",
         "They cut their own thread the first time, so it's a little stiff. Stop as soon as the head seats.",
         "No nuts needed."], M, y - 4, colw, 10, gap=3)
img("step_screws.png", M + colw + 15, y - 250, W - 2 * M - colw - 15, 260, anchor="c")
y -= 275
c.setStrokeColor(HexColor("#e5e7eb"))
c.line(M, y, W - M, y)
y -= 30
y = step_head(8, "Gold shell on", y)
bullets(["Put the shell straight down from above, square to the screen.",
         "The rim under the shell drops inside the base walls and lines it up. The magnets pull it home with a click.",
         "The lip stands 0.25 mm clear of the glass, so nothing presses on the screen.",
         "Check: the seam is even all round, with no rattle."], M, y - 4, colw, 10, gap=3)
img("step_shell.png", M + colw + 15, y - 250, W - 2 * M - colw - 15, 260, anchor="c")
footer()
c.showPage()

# =============================================================== page 10: step 9 + opening
header("Route the cable", "Step 9")
y = H - 100
y = step_head(9, "Cable cleat", y)
bullets(["Plug in the USB-C cable and bring it round to the back.",
         "Give it one turn round the cleat at the bottom of the back wall: under one horn, over the other.",
         "A tug on the cable now pulls on the cleat, not the USB-C plug.",
         "No cleat on your base? Nothing to do."], M, y - 4, colw, 10, gap=3)
img("step_cleat.png", M + colw + 15, y - 260, W - 2 * M - colw - 15, 270, anchor="c")
y -= 300
c.setFont("SansB", 12)
c.setFillColor(INK)
c.drawString(M, y, "Opening the case later")
bullets(["Pull the gold shell straight up off the magnets.",
         "Undo the three M2.5 screws. Lift the board and plate out, right side first, unhooking the USB-C last.",
         "Lift the battery straight out."], M, y - 18, W - 2 * M, 10, gap=2)
footer()
c.showPage()
c.save()
print("saved", OUT)
