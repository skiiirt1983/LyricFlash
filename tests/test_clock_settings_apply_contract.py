#!/usr/bin/env python3
"""1.0.3: every clock field the web Clock Studio save writes must reach the
running device. 1.0.1 and earlier validated colours, sizes, date format and
weather placement but dropped them in lf_ui_apply_basic_settings()."""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
network = (root / "main/network.c").read_text()
ui = (root / "main/ui.c").read_text()

post = network.split("static esp_err_t handle_basic_post(httpd_req_t *req)", 1)[1]
post = post.split("lf_ui_apply_basic_settings(&current)", 1)[0]
written = set(re.findall(r"current\.(face_[a-z0-9_]+)\[i\]\s*=", post))
written |= set(re.findall(r"current\.(basic_[a-z0-9_]+)\s*=", post))
assert len(written) > 20, written

apply = ui.split("esp_err_t lf_ui_apply_basic_settings(const lf_settings_t *basic)", 1)[1]
apply = apply.split("err = lf_settings_save(prefs);", 1)[0]
applied = set(re.findall(r"memcpy\(prefs->([a-z0-9_]+),", apply))
applied |= set(re.findall(r"LF_COPY_FACE\(([a-z0-9_]+)\)", apply))
applied |= set(re.findall(r"prefs->([a-z0-9_]+)\s*=\s*basic->", apply))

missing = sorted(written - applied)
assert not missing, "Web clock save drops these fields: " + ", ".join(missing)

# The device's own Clock Settings buttons must edit what the clock draws.
assert "case 1: return &prefs->face_time_color[5];" in ui
assert "prefs->face_show_date[5] = !prefs->face_show_date[5];" in ui
print("PASS: every web clock field is applied; device Time color and Date edit the drawn fields")
