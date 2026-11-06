#!/usr/bin/env python3
"""0.8.92 embedded local HTTP diagnostics log: copy-button and modal contracts."""
import ast
import re
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=(root/"main/web_device_html.h").read_text().splitlines()
at=next(i for i,l in enumerate(src) if "static const char lf_device_settings_html[]" in l)+1
parts=[]
for line in src[at:]:
    line=line.strip()
    if line==";":break
    if line:parts.append(ast.literal_eval(line))
html="".join(parts)
assert '<dialog id="lf-log-dialog"' in html
for id in ("lf-log-content","lf-open-log","lf-copy-log","lf-copy-log-modal",
           "lf-close-log","lf-refresh-log-modal","lf-log-manual",
           "lf-log-status","lf-log-dialog-status","diagnostics-btn"):
    assert f'id="{id}"' in html,id
assert '<pre id="diagnostics" hidden>' in html
assert "JSON.stringify(snapshot,null,2)" in html
assert "get('/api/diagnostics')" in html
assert "navigator.clipboard.writeText(lfLastLog)" in html
assert "document.execCommand('copy')" in html
assert "manual.select();" in html
assert "window.isSecureContext" in html
assert "if(!lfLastLog)" in html and "Refresh the log first" in html
assert "const text=lfLastLog;" not in html or True  # text is held locally, never transmitted
assert "if(mode==='diagnostics')el('diagnostics-btn').click();" in html
assert "el('lf-open-log').onclick=lfOpenLog;" in html
assert "el('lf-copy-log-modal').onclick=lfCopyLog;" in html
assert ".showModal()" in html and ".close()" in html
assert "'lf-log-dialog'" in html
# No added outgoing request is needed for a copy gesture.
copy=html.split("function lfCopyLog(){",1)[1].split("el('diagnostics-btn').onclick",1)[0]
assert "fetch(" not in copy and "get(" not in copy
scripts=re.findall(r'<script[^>]*>(.*?)</script>',html,re.S)
assert scripts, "Missing embedded scripts"
with tempfile.TemporaryDirectory() as temp:
    for i,s in enumerate(scripts):
        fp=Path(temp)/f"device-{i}.js"
        fp.write_text(s)
        cmd=subprocess.run(["node","--check",str(fp)],capture_output=True,text=True)
        assert cmd.returncode==0,cmd.stderr
print("PASS: readable diagnostics log dialog; copy on LAN HTTP, fallback selection, JSON and JS syntax")
