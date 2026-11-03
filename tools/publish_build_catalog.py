#!/usr/bin/env python3
"""Publish a public, usable OTA selector catalog from approved GitHub Releases.

GitHub repo may be PRIVATE. The device browser cannot query private Releases
with no token. This runs INSIDE the release workflow using GITHUB_TOKEN,
copies approved application-only OTA .bin assets into public Pages hosting,
then writes beta/catalog.json. Never publish full-image recovery as OTA.
Only GitHub Releases are used for historical versions (not transient Actions).
"""
import json
import os
from pathlib import Path
import re
import sys
from urllib.request import Request, urlopen
from urllib.error import HTTPError

REPO = "skiiirt1983/LyricFlash"
BASE = "https://skiiirt1983.github.io/LyricFlash/beta/"
RELEASE_API = f"https://api.github.com/repos/{REPO}/releases?per_page=25"
NAME = re.compile(r"^lyricflash.*ota.*\.bin$", re.I)
TAG = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
MAX_BYTES = 5 * 1024 * 1024

def get(url, token, accept="application/vnd.github+json"):
    headers={"Accept":accept, "User-Agent":"LyricFlash-Build-Catalog"}
    if token:
        headers["Authorization"]="Bearer "+token
    with urlopen(Request(url,headers=headers),timeout=30) as response:
        return response.read(MAX_BYTES+1)

def app_version(data):
    if len(data)<0x70 or data[0]!=0xE9:
        raise ValueError("Not an ESP application image")
    # Espressif app descriptor: app_desc starts at 0x20; version at 0x30,
    # project_name at 0x50. Verify these, not just the release tag.
    version=data[0x30:0x50].split(b"\0",1)[0].decode("ascii")
    project=data[0x50:0x70].split(b"\0",1)[0].decode("ascii")
    if project!="lyricflash" or not version or not re.fullmatch(r"[A-Za-z0-9._-]{1,32}",version):
        raise ValueError("Invalid LyricFlash app descriptor")
    return version

def publish(site, token):
    folder=site/"beta/firmware"
    current=folder/"lyricflash-ota.bin"
    manifest=json.loads((site/"beta/manifest.json").read_text())
    current_version=app_version(current.read_bytes())
    if current_version!=manifest["version"]:
        raise ValueError("Current Pages beta and app descriptor version differ")
    entries=[{
        "version":current_version,"channel":"Beta",
        "label":"BETA "+current_version,
        "url":BASE+"firmware/lyricflash-ota.bin"
    }]
    if token:
        try:
            releases=json.loads(get(RELEASE_API,token))
        except HTTPError as e:
            raise ValueError(f"GitHub release discovery failed: {e.code}") from e
        for release in releases:
            tag=release.get("tag_name","")
            if release.get("draft") or not TAG.fullmatch(tag):continue
            assets=[a for a in release.get("assets",[]) if NAME.fullmatch(a.get("name",""))]
            if len(assets)!=1:continue
            asset=assets[0]
            if int(asset.get("size",0))>MAX_BYTES:continue
            data=get(asset["url"],token,"application/octet-stream")
            if len(data)>MAX_BYTES:continue
            # GitHub's asset REST endpoint sometimes responds with JSON;
            # in that case do NOT publish unverified content.
            try:version=app_version(data)
            except (UnicodeError,ValueError):continue
            destination=folder/"releases"/tag/"lyricflash-ota.bin"
            destination.parent.mkdir(parents=True,exist_ok=True)
            destination.write_bytes(data)
            entries.append({
                "version":version,
                "channel":"Beta" if release.get("prerelease") else "Stable",
                "label":("BETA " if release.get("prerelease") else "STABLE ")+tag,
                "url":BASE+"firmware/releases/"+tag+"/lyricflash-ota.bin"
            })
    # Avoid duplicate current version, which can also have a Release.
    unique=[];versions=set()
    for item in entries:
        if item["version"] in versions:continue
        versions.add(item["version"]);unique.append(item)
    catalog={"product":"LyricFlash","chipFamily":"ESP32-S3","builds":unique}
    (site/"beta/catalog.json").write_text(json.dumps(catalog,indent=2)+"\n")
    print("Published OTA build selector versions:",", ".join(x["version"] for x in unique))
    return catalog

if __name__=="__main__":
    if len(sys.argv)!=2:raise SystemExit("Usage: publish_build_catalog.py <site directory>")
    publish(Path(sys.argv[1]),os.environ.get("GITHUB_TOKEN",""))
