"""LyricFlash case bill of materials -> LyricFlash_BOM.xlsx (next to the assembly guide).
Quantities and sizes come from params.py / the assembly guide; unit prices are left blank (yellow) for you to fill in."""
import os
from openpyxl import Workbook
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "LyricFlash_BOM.xlsx")
F = "Arial"
wb = Workbook()
ws = wb.active
ws.title = "BOM"

thin = Side(style="thin", color="C8CCD4")
BORDER = Border(left=thin, right=thin, top=thin, bottom=thin)
HEAD = PatternFill("solid", fgColor="1D2330")
YEL = PatternFill("solid", fgColor="FFFF00")
GREY = PatternFill("solid", fgColor="F3F4F6")

ws["A1"] = "LyricFlash case: bill of materials"
ws["A1"].font = Font(name=F, size=16, bold=True)
ws["A2"] = ("Design v11i (2026-11-17). For the Waveshare ESP32-S3-Touch-AMOLED-1.75 desk case. Yellow cells are for you: "
            "enter the unit price you paid; the totals calculate. Blue text = quantities typed in from the design.")
ws["A2"].font = Font(name=F, size=10, italic=True)
ws.merge_cells("A2:H2")
ws["A2"].alignment = Alignment(wrap_text=True, vertical="top")
ws.row_dimensions[2].height = 30

hdr = ["#", "Item", "Specification", "Qty", "Unit price ($)", "Line total ($)", "Used for", "Notes"]
R0 = 4
for i, h in enumerate(hdr, 1):
    c = ws.cell(row=R0, column=i, value=h)
    c.font = Font(name=F, bold=True, color="FFFFFF")
    c.fill = HEAD
    c.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True)
    c.border = BORDER

rows = [
    # section, item, spec, qty, used for, notes, example price
    ("Electronics", None, None, None, None, None, None),
    ("Display board", "Waveshare ESP32-S3-Touch-AMOLED-1.75 (standard board, NOT the -B cased version)", 1,
     "The device itself", "Rev 1.0 and Rev 1.1 measure the same. 466 x 466 round AMOLED, ESP32-S3R8, AXP2101.", None),
    ("LiPo battery", "LP103454, 3.7 V, about 2000 mAh, 56 x 34.5 x 10.3 mm, with MX1.25 2-pin plug (1.25 mm pitch)", 1,
     "Power", "The board's socket J1 is MX1.25, not the bigger JST-PH 2.0 mm many sellers fit. Check polarity with a meter before plugging in.", None),
    ("USB-C data cable", "USB-C to USB-A or USB-C, data capable", 1, "Power, charging", "Any decent cable. Also useful for flashing.", None),
    ("Hardware", None, None, None, None, None, None),
    ("Disc magnets", "4 x 2 mm round (N35 or better)", 8, "Shell to base (4 pairs)",
     "4 in the base corner pockets, 4 in the shell. Pockets: 4.10 mm in PETG bases, 4.20 mm in PLA bases. Check polarity before pressing in.", None),
    ("Flat head (countersunk) screw", "M2 x 3, Phillips or hex", 3, "Board to carrier plate", "Go into the board's own metal standoffs. Snug, not tight.", None),
    ("Pan head self-tapping screw", "M2.5 x 6", 2, "Carrier plate to base posts", "Cut their own thread in the printed posts (2.5 mm pilot). No nuts.", None),
    ("Printed parts", None, None, None, None, None, None),
    ("Top shell", "Gold PLA (Bambu PLA Metal) - STL: top_shell.stl", 1, "Holds the screen", "Print face-down, no supports.", None),
    ("Base", "Black PETG (Bambu PETG Matte) or PLA - one of 6 variants: ribbed / panels / smooth, with or without cleat", 1,
     "Main body, PWR button, battery slot", "Supports only where painted at the USB-C opening (in the project files). PLA version has larger magnet pockets.", None),
    ("Carrier plate", "Black PETG - carrier_plate.stl", 1, "Board mount", "Print flat, board side down.", None),
    ("Consumables (optional)", None, None, None, None, None, None),
    ("CA glue", "Cyanoacrylate, a drop", 1, "Loose magnet only", "Optional.", None),
]

r = R0 + 1
n = 0
first_data = r
data_rows = []
for sec_or_item in rows:
    if sec_or_item[1] is None:                       # section header row
        c = ws.cell(row=r, column=1, value=sec_or_item[0])
        c.font = Font(name=F, bold=True)
        for col in range(1, 9):
            ws.cell(row=r, column=col).fill = GREY
            ws.cell(row=r, column=col).border = BORDER
        ws.merge_cells(start_row=r, start_column=1, end_row=r, end_column=8)
        r += 1
        continue
    n += 1
    item, spec, qty, used, notes, _ = sec_or_item
    vals = [n, item, spec, qty, None, f"=D{r}*E{r}", used, notes]
    for col, v in enumerate(vals, 1):
        c = ws.cell(row=r, column=col, value=v)
        c.font = Font(name=F, size=10)
        c.border = BORDER
        c.alignment = Alignment(wrap_text=True, vertical="top", horizontal="center" if col in (1, 4) else "left")
    ws.cell(row=r, column=4).font = Font(name=F, size=10, color="0000FF")      # typed-in quantity
    ws.cell(row=r, column=5).fill = YEL
    ws.cell(row=r, column=5).number_format = '$#,##0.00;($#,##0.00);-'
    ws.cell(row=r, column=6).number_format = '$#,##0.00;($#,##0.00);-'
    data_rows.append(r)
    r += 1
last_data = r - 1

r += 1
ws.cell(row=r, column=5, value="Total").font = Font(name=F, bold=True)
t = ws.cell(row=r, column=6, value=f"=SUM(F{first_data}:F{last_data})")
t.font = Font(name=F, bold=True)
t.number_format = '$#,##0.00;($#,##0.00);-'
ws.cell(row=r, column=5).alignment = Alignment(horizontal="right")
r += 2

ws.cell(row=r, column=1, value="Tools (not counted in the total)").font = Font(name=F, bold=True)
r += 1
for tline in ["Soft cloth (screen goes face-down on it)", "Small Phillips or hex driver for M2 and M2.5",
              "Multimeter (battery polarity)", "Flush cutters (support clean-up)"]:
    ws.cell(row=r, column=2, value=tline).font = Font(name=F, size=10)
    r += 1
r += 1
ws.cell(row=r, column=1, value="Printer used: Bambu Lab P2S, 0.4 mm nozzle, Textured PEI. Settings are stored in the Bambu project files in the print pack.").font = Font(name=F, size=9, italic=True)

for i, w in enumerate([5, 30, 46, 7, 13, 14, 26, 60], 1):
    ws.column_dimensions[get_column_letter(i)].width = w
ws.freeze_panes = ws.cell(row=R0 + 1, column=1)
ws.sheet_view.showGridLines = False

# ---- sheet 2: printed parts detail
p = wb.create_sheet("Printed parts")
p["A1"] = "Printed parts and files"
p["A1"].font = Font(name=F, size=14, bold=True)
hdr2 = ["Part", "File(s) in the print pack", "Filament", "Print orientation", "Supports", "Notes"]
for i, h in enumerate(hdr2, 1):
    c = p.cell(row=3, column=i, value=h)
    c.font = Font(name=F, bold=True, color="FFFFFF")
    c.fill = HEAD
    c.border = BORDER
    c.alignment = Alignment(horizontal="center", wrap_text=True)
parts = [
    ("Top shell", "stl/top_shell.stl", "PLA Metal gold (slot 1)", "Face down (screen ring on the plate)", "None", "One per device."),
    ("Base (PETG)", "stl/base_ribbed.stl, base_panels.stl, base_smooth.stl, plus _nocleat versions", "PETG Matte black (slot 2)",
     "Floor down", "Painted tree supports at the USB-C opening", "Pick ONE of six. Cleat = cable holder on the back."),
    ("Base (PLA)", "stl_pla/*_pla.stl", "Any PLA", "Floor down", "Painted tree supports at the USB-C opening",
     "Magnet pockets 4.20 mm (PETG 4.10 mm). Select a PLA profile for slot 2 in Bambu Studio."),
    ("Carrier plate", "stl/carrier_plate.stl", "PETG Matte black", "Flat, board side down", "None", "One per device."),
    ("Bambu projects", "lyricflash_print_plates.3mf (PETG) / lyricflash_print_plates_PLA.3mf (PLA)", "-", "-", "-",
     "8 plates: shell, six bases, carrier."),
]
for ri, row in enumerate(parts, 4):
    for ci, v in enumerate(row, 1):
        c = p.cell(row=ri, column=ci, value=v)
        c.font = Font(name=F, size=10)
        c.border = BORDER
        c.alignment = Alignment(wrap_text=True, vertical="top")
for i, w in enumerate([18, 52, 26, 30, 36, 56], 1):
    p.column_dimensions[get_column_letter(i)].width = w
p.sheet_view.showGridLines = False

wb.save(OUT)
print("saved", OUT)
