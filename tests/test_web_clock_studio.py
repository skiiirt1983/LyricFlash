#!/usr/bin/env python3
"""Validate the local Clock Studio HTML/JS embedded in a C string literal."""
import ast
from pathlib import Path
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/"main/web_basic_html.h").read_text()
lines=source.splitlines()
start=next(i for i,line in enumerate(lines) if 'lf_basic_settings_html[] =' in line)+1
parts=[]
for line in lines[start:]:
    line=line.strip()
    if line == ";": break
    assert line.startswith('"') and line.endswith('"'), f"Invalid C string on line {line[:50]}"
    parts.append(ast.literal_eval(line))
html="".join(parts)
assert "<!doctype html>" in html and "</html>" in html
assert "<script>" in html and "</script>" in html
ids=set(re.findall(r'\bid="([^"]+)"',html))
js=html.split("<script>",1)[1].split("</script>",1)[0]
fields=re.search(r'const fields=\[(.*?)\];',js)
assert fields, "Missing JS form field schema"
names=re.findall(r"'([^']+)'",fields.group(1))
for name in names:
    assert name in ids, f"Missing input for field {name}"
for essential in ('clock_face','weather_enabled','city','interval_minutes','ring_mode',
                  'font_style','font_size','show_date','weather','background',
                  'foreground','ring_a','ring_b','ring_c'):
    assert essential in names, f"Clock setting omitted: {essential}"
assert len(names)==len(set(names)), "Duplicate form field"
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/"clock-studio.js"
    path.write_text(js)
    result=subprocess.run(["node","--check",str(path)],capture_output=True,text=True)
    if result.returncode: raise AssertionError("Embedded web JS syntax invalid: "+result.stderr)
print("PASS: embedded Clock Studio HTML, all settings inputs and JS syntax")
