import os
HERE = os.path.dirname(os.path.abspath(__file__))
from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import HexColor, white

pdfmetrics.registerFont(TTFont("Sans", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
pdfmetrics.registerFont(TTFont("SansB", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"))

OUT = os.path.join(HERE, "LyricFlash_Checklist_for_Install.pdf")
W, H = letter
M = 40
INK = HexColor("#1d2330")
GREY = HexColor("#6b7280")
LIGHT = HexColor("#f3f4f6")
GOLD = HexColor("#c9962e")

c = canvas.Canvas(OUT, pagesize=letter)
c.setTitle("LyricFlash Case: Checklist for Install")

c.setFillColor(INK)
c.rect(0, H - 62, W, 62, stroke=0, fill=1)
c.setFillColor(GOLD)
c.rect(0, H - 66, W, 4, stroke=0, fill=1)
c.setFillColor(white)
c.setFont("SansB", 20)
c.drawString(M, H - 40, "Checklist for install")
c.setFont("Sans", 10)
c.setFillColor(HexColor("#cbd5e1"))
c.drawRightString(W - M, H - 38, "LyricFlash case · design v10.1")

y = H - 96
c.setFillColor(INK)
c.setFont("Sans", 10)
c.drawString(M, y, "Unit / print: ______________________     Base style: ribbed / panels / smooth,  cleat: yes / no     Date: ____________")
y -= 16

rows = [
    ("Magnets: all 8 seated flush, each shell magnet attracts the base magnet under it", "OK / one loose (glued) / wrong way round"),
    ("Switch: presses into the wall firmly, knob stands out and slides end to end, cuts power", "OK / too tight / loose / rubs"),
    ("Battery goes in and comes out by hand, no forcing; doesn't rattle when closed", "OK / tight where / rattles"),
    ("Board screws to the plate with the 3 M2 x 3; heads flush in the countersinks", "OK / proud / stripped"),
    ("Board + plate go in USB-C first and settle on the posts without forcing", "OK / rubs where"),
    ("Three M2.5 x 6 bite and hold; plate doesn't move", "OK / stripped / plate rocks"),
    ("Shell: seats flush all round, magnets hold it firmly, comes off by hand, no rattle", "flush: y / gap where · hold: firm / weak / too strong"),
    ("USB-C mouth flush with the face, gap even all round", "sunk / level / slightly proud / clearly proud · gap even: y / touches where"),
    ("USB-C opening top edge printed clean", "clean / droops"),
    ("Plug a cable in 20 times while holding the case", "board moves: no / yes"),
    ("Mics: play a song from ~1 m, LyricFlash identifies it as reliably as with the bare board", "same / worse / much worse"),
    ("Cable cleat: one turn holds; a light tug from any side doesn't pull on the plug, slide or tip the case", "OK / slips / too fiddly"),
    ("Touch works right to the edge, no lit pixels hidden by the lip", "OK / hidden where"),
]


def wrap(text, font, size, width):
    out, cur = [], ""
    for w_ in text.split(" "):
        t = (cur + " " + w_).strip()
        if pdfmetrics.stringWidth(t, font, size) <= width:
            cur = t
        else:
            out.append(cur)
            cur = w_
    out.append(cur)
    return out


colA = 345
for i, (a, b) in enumerate(rows):
    la = wrap(a, "Sans", 10, colA - 60)
    lb = wrap(b, "Sans", 8.5, W - 2 * M - colA - 10)
    h = max(len(la) * 13, len(lb) * 11) + 24
    y -= h
    if i % 2 == 0:
        c.setFillColor(LIGHT)
        c.rect(M, y, W - 2 * M, h, stroke=0, fill=1)
    c.setStrokeColor(INK)
    c.setLineWidth(1.2)
    c.rect(M + 8, y + h - 22, 12, 12, stroke=1, fill=0)
    c.setFillColor(INK)
    c.setFont("SansB", 9)
    c.drawString(M + 26, y + h - 20, str(i + 1))
    c.setFont("Sans", 10)
    yy = y + h - 20
    for ln in la:
        c.drawString(M + 42, yy, ln)
        yy -= 13
    c.setFillColor(GREY)
    c.setFont("Sans", 8.5)
    yy = y + h - 20
    for ln in lb:
        c.drawString(M + colA, yy, ln)
        yy -= 11
    c.setStrokeColor(HexColor("#c7cbd1"))
    c.setLineWidth(0.6)
    c.line(M + colA, y + 8, W - M - 10, y + 8)

y -= 30
c.setFillColor(INK)
c.setFont("SansB", 11)
c.drawString(M, y, "Notes")
c.setStrokeColor(HexColor("#c7cbd1"))
for k in range(max(0, int((y - 50) // 22))):
    c.line(M, y - 22 * (k + 1), W - M, y - 22 * (k + 1))

c.setFont("Sans", 8)
c.setFillColor(GREY)
c.drawString(M, 22, "LyricFlash case · checklist for install · design v10.1 (2026-11-11)")
c.showPage()
c.save()
print("saved", OUT)
