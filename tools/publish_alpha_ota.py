#!/usr/bin/env python3
"""Publish verified development OTA bytes on an isolated release-assets branch."""
import base64, hashlib, json, os, re, sys, time
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError

REPO = "skiiirt1983/LyricFlash"
BRANCH = "alpha-ota"
API = "https://api.github.com/repos/" + REPO

def request(method, path, token, data=None):
    headers = {"Accept":"application/vnd.github+json",
               "Authorization":"Bearer "+token, "User-Agent":"LyricFlash-OTA-Publisher"}
    body = json.dumps(data).encode() if data is not None else None
    if body is not None: headers["Content-Type"] = "application/json"
    try:
        with urlopen(Request(API+path, data=body, headers=headers, method=method), timeout=90) as response:
            return json.load(response)
    except HTTPError as exc:
        if exc.code == 404: return None
        raise

def publish(filename):
    token = os.environ["GITHUB_TOKEN"]
    commit = os.environ["GITHUB_SHA"]
    if not re.fullmatch("[0-9a-f]{40}", commit): raise ValueError("Invalid source SHA")
    tag = "alpha-build-" + commit[:12]
    data = Path(filename).read_bytes()
    if not (1024 < len(data) < 5*1024*1024 and data[0] == 0xE9):
        raise ValueError("Invalid OTA image size or magic")
    ver = data[0x30:0x50].split(bytes([0]),1)[0].decode("ascii")
    proj = data[0x50:0x70].split(bytes([0]),1)[0].decode("ascii")
    if proj != "lyricflash" or not re.fullmatch("[A-Za-z0-9._-]{1,32}", ver):
        raise ValueError("Invalid application descriptor")
    if request("GET","/git/ref/heads/"+BRANCH,token) is None:
        request("POST","/git/refs",token,{"ref":"refs/heads/"+BRANCH,"sha":commit})
    key = "/contents/releases/"+tag+"/lyricflash-ota.bin"
    if request("GET",key+"?ref="+BRANCH,token) is None:
        request("PUT",key,token,{"message":"Publish validated OTA "+tag,
                               "branch":BRANCH,
                               "content":base64.b64encode(data).decode("ascii")})
    url = "https://raw.githubusercontent.com/"+REPO+"/"+BRANCH+"/releases/"+tag+"/lyricflash-ota.bin"
    expected = hashlib.sha256(data).hexdigest()
    for attempt in range(6):
        try:
            with urlopen(Request(url,headers={"User-Agent":"LyricFlash-OTA-Verify"}),timeout=75) as response:
                observed = hashlib.sha256(response.read(5*1024*1024+1)).hexdigest()
            if observed == expected:
                print("PASS: independently downloaded direct OTA",tag,ver,expected)
                return
        except Exception as exc:
            print("Download check retry",attempt+1,type(exc).__name__)
        time.sleep(3)
    raise RuntimeError("Direct OTA download verification failed")

if __name__ == "__main__":
    publish(sys.argv[1])
