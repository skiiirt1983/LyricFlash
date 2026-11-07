#!/usr/bin/env python3
"""Guard actual non-LCD font glyphs, matched Studio previews and source size."""
import base64
import re
import zlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
font=(root/"main/basic_open_face.h").read_text()
clock=(root/"main/basic_clock.c").read_text()
net=(root/"main/network.c").read_text()
page=(root/"main/web_basic_html.h").read_text()
match=re.search(r'static const char lf_open_face_b64\[\]="([A-Za-z0-9+/]+)"',font)
assert match, "Real glyph data missing"
raw=base64.b64decode(match.group(1),validate=True)
assert len(raw)==2940, len(raw)
assert zlib.adler32(raw[:1260])==1184830793, "Open-source glyph bitmaps corrupted"
family_len=10*21*2
assert len({raw[i*family_len:(i+1)*family_len] for i in range(7)})==7
assert "LF_OPEN_FACE_FAMILIES 7U" in font
assert "lf_open_face_row(" in clock
assert "open_face_family(" in clock
assert "open_face_digit(" in clock
assert 'case 3:case 9:case 11:case 13:case 19:return 0' in clock
assert 'case 5:case 7:case 8:case 16:case 17:return 1' in clock
assert 'case 6:case 10:case 14:case 15:return 2' in clock
assert "LF_OPEN_FACE_RAW_BYTES 2940U" in font
assert '"/api/clock-fonts"' in net
assert 'lf_open_face_b64' in net
assert 'openGlyphPreview(' in page
assert "request('/api/clock-fonts')" in page
print("PASS: seven distinct authentic open-source glyph families, checksum, shared firmware/WebUI preview")
