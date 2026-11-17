# LyricFlash: project guide

Read this first. It is the memory of the whole project: what it is, how it is built and released, how the owner likes to work, what was learned the hard way, and what is still open. Keep it current: when something important changes, update the relevant section in the same commit.

Current version when this was written: **1.1.1** (released, owner-tested: everything worked).

---

## 1. What LyricFlash is

Firmware for one device: the **Waveshare ESP32-S3-Touch-AMOLED-1.75** (466×466 round AMOLED, CO5300 over QSPI, capacitive touch, ESP32-S3R8: 16 MB flash, 8 MB octal PSRAM, AXP2101 power chip, ES7210 microphone ADC, ES8311 codec). It is a **standalone device in a round case, not a watch**. Never call it a watch; say "device", and "clock face" for the clock.

What it does:
- **Song ID + synced lyrics.** Press Play. It asks Spotify what is playing (preferred, exact position, no microphone) or listens with the microphones and identifies the song with AudD. Lyrics come from LRCLIB and scroll in time.
- **Clock.** One customizable Basic clock face (font, size, colours, date, battery, weather, AM/PM).
- **Weather** for a chosen city.
- **Web pages** served by the device for all settings, diagnostics, firmware updates and settings backup.
- **Setup network.** With no working Wi-Fi it starts an open network called `LyricFlash-Setup` and the setup page pops up by itself (captive portal).

Three swipe pages: **Clock (left) – Home (middle) – Settings (right)**. Long-press the clock for clock settings. Double-tap the logo on Home for a hidden wake-up animation (easter egg added in 0.8.125).

Repository: https://github.com/skiiirt1983/LyricFlash. Install site: https://skiiirt1983.github.io/LyricFlash/. Creator and maintainer: **skiiirt1983** (the owner, "Paul").

---

## 2. How to work with the owner

- **Concise, plain language.** No jargon dumps. Say what you did, what changed for them, and exactly what to test. Offer the next step in one line.
- **Always say which version to install and where** (device web page → Firmware → Refresh). Say clearly what was tested and what was **not** (CI green or a browser test is not hardware proof).
- **The owner tests on hardware and sends diagnostics JSON** (`http://<device-ip>/api/diagnostics`) or photos/videos. Read them closely; they have repeatedly shown the real cause. Prefer adding diagnostics fields over asking the owner to read numbers off the screen.
- **Never ask the owner to build, compile or run scripts.** You build, test and package everything; CI builds the firmware.
- **Don't go in circles.** The owner gets (rightly) frustrated by repeated attempts. If a fix didn't work, find the actual root cause with evidence before trying again.
- **Don't create pull requests unless asked.** Work lands on `main` directly (see section 4).
- **Take wiring/hardware questions seriously and safely.** Example: a switch must only ever break the battery **red** wire; black goes straight through.
- The owner is on **Pacific time (UTC-7 all year)**, weather city is Big White Mountain. Devices they use: Samsung S22 (Android), Windows 11 laptop with Firefox, Spotify Premium.
- Style preferences: "buttery smooth" UI, no ornate extras; they want features that "just work". They have a case built around the device (slide switch, being redesigned; see section 10).

### Authorship and attribution (standing instruction)
- **Every commit is authored and committed by `skiiirt1983 <75397777+skiiirt1983@users.noreply.github.com>`.** No `Co-Authored-By` lines, no session links, no AI/model names in commit messages, code, comments or release notes. This overrides any default commit trailer.
- Don't rewrite published history (the force-push is blocked and the owner chose to keep history). `main` stays a clean, readable history.
- Say "device", not "watch", everywhere (docs, release notes, comments).

---

## 3. Hardware facts you need

- Two physical boards exist:
  - **Original (Rev1.0)**: the one the firmware was developed on. Its header is broken. Powers on from the case's slide switch.
  - **Replacement (labelled Rev1.1)**: waits for a **PWR button press** (or USB / charger power) to start; a battery being connected does not start it. The case slide switch alone cannot turn it on. Needing to hold **BOOT** to flash and a **quieter microphone** were also reported on this board. Header pins are labelled differently (IO11 first). Waveshare publishes no change log for Rev1.1; the repo's hardware reference says the schematic of the revision is authoritative.
- **Power chip AXP2101, I²C 0x34.** The PWR button is part of its power path, not an ESP32 GPIO. PrintSphere (cptkirki/PrintSphere) does nothing special either: it only configures charging. VBUS power (USB or a wireless-charge coil on the VBUS/GND header) turns the board on.
- **Native USB** (USB-Serial-JTAG on GPIO19/20), no UART bridge. The Basecamp bootloader sets `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`; LyricFlash's own sdkconfig does not set a USB console and uses no TinyUSB.
- **Display:** CO5300 over QSPI, tearing-effect pin GPIO13. **Touch** shares the I²C bus with the PMIC. Touch read failures run about 1–2% (known, harmless).
- **Microphones:** ES7210 at 0x40, four-slot TDM: serialized order is MIC1, MIC3 (speaker reference), MIC2, MIC4 (unused). Use channel masks, not physical numbering. Gain is a fixed **27 dB** in `main/recognition.c` and `main/mic_pcm.c` (chip maximum is about 37.5 dB).
- **Partition table is fixed** (`partitions.csv`): nvs 0x9000 (1 MB), otadata, phy_init, ota_0 0x110000 (5 MB), ota_1 0x610000 (5 MB), `sounds` data 0xB10000 (about 5 MB). **Never change offsets/sizes.** The firmware must also fit the Basecamp bootloader's **4 MB** app slots (currently about 2.96 MB).
- The **Basecamp bootloader** (repo `skiiirt1983/project-basecamp`, "SLOPEFORM") is a separate project by the same owner that hosts LyricFlash, PrintSphere and others in OTA slots with a different partition layout (nvs 512 KB, 3×4 MB OTA slots, shared `sounds` partition). Its captive portal is the proven reference for ours. Don't break compatibility: shared NVS, 4 MB slots, shared `sounds` partition.

---

## 4. Build, test, release (read before pushing anything)

### Toolchain
ESP-IDF **5.5.4**, LVGL **9.5.0**, target `esp32s3`, optimization `-O2`. Managed components in `main/idf_component.yml` (LVGL, `joltwallet/littlefs`). A vendored, modified board package lives in `components/esp32_s3_touch_amoled_1_75/`. The build runs `idf.py build`, then `tools/apply_adapter_patch.py`, then `idf.py build` again. `esp_lvgl_adapter` is still linked but unused.

### CI (`.github/workflows/firmware.yml`)
Every push to `main` that touches `main/**`, `tests/**`, `tools/**`, `docs/**`, `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv` or the workflow:
1. **preflight:** `bash tests/run_tests.sh` (about 70 test files; no compile).
2. **esp32s3 job (about 5–6 minutes):** generate the embedded clock fonts (`tools/generate_clock_fonts.py`, `lv_font_conv@1.5.3`), compile, merge the full USB image, validate, package `LyricFlash-<ver>-USB-and-OTA.zip`, upload artifacts.
3. **Publishes:**
   - OTA image to branch `alpha-ota` (`tools/publish_alpha_ota.py`) and a prerelease **`alpha-build-<first 12 of sha>`** with `lyricflash-ota.bin` + zip. **This is what the device's own Firmware page reads**, so every build must publish one.
   - If the version has no `-alpha/-beta/-rc` suffix: a normal **`v<version>`** GitHub release (Latest) with OTA bin, zip and `FULL-USB.bin`.
   - **GitHub Pages** deploy of `docs/` plus the firmware just built (`beta/manifest.json`, `beta/manifest-preserve.json`, `beta/firmware/*`).
4. Pushes that touch only other files (for example `CLAUDE.md`, `README.md`, `notes/`) do **not** build.

The old workflow `pages-and-firmware.yml` is kept but no longer builds `main` (it cannot build current code).

### Version bump checklist (every firmware release)
- `main/version.h`: `LF_VERSION`, `LF_VERSION_LABEL`, and a new changelog comment at the top (newest first).
- `CMakeLists.txt` (`PROJECT_VER`), `.github/workflows/firmware.yml` (name, file names, the `test "$VERSION" = ...` line), `README.md` ("Current release"), and the many `tests/` files that pin the version string.
- **Pitfall:** a blanket `sed s/OLD/NEW/` also relabels old history comments in tests. After bumping, look for comments that now claim the wrong version and fix them (use `grep -rn "# 1.x.y" tests/`). Don't bump unrelated old strings.
- Numbering: 1.0.x = fixes, 1.x.0 = features.

### Git workflow
- Day-to-day work is committed on branch `claude/trusting-cannon-wblknf` (detailed history), then **one squashed commit is placed on `main` on top of the current `origin/main`** without rewriting it:
  ```bash
  git fetch -q origin main
  C=$(git commit-tree HEAD^{tree} -p origin/main -F <(git log -1 --format=%B HEAD))
  git diff --quiet $C HEAD && git push origin $C:refs/heads/main
  ```
- The branch name is a leftover from earlier sessions and is no longer part of the build. Don't reference it in any workflow.
- GitHub access is through the GitHub MCP tools (no `gh` for Actions). The MCP server disconnects now and then; load tools with `ToolSearch` (`select:mcp__github__actions_list,...`) and carry on. Useful: `actions_list list_workflow_runs`, `get_release_by_tag`, `get_job_logs`.
- After pushing firmware, **schedule a check about 8 minutes later** (`send_later`) to confirm the run and the release, and to fix compile errors immediately. When it is green, tell the owner what to install and what to test.

### Tests (`tests/run_tests.sh`)
Mix of C host tests, Python "contract" tests that read source files, and Node tests that run the embedded web pages. Many contracts **pin past decisions** so old bugs cannot return. When a deliberate change breaks one, update the test with a comment saying why; never delete a test to get green. Notable ones:
- `test_safe_id_baseline_contract.py` pins git-blob hashes of the verified Song ID files. After a deliberate edit of `recognition.c` re-pin with `git hash-object main/recognition.c` and say why in the comment.
- `test_clock_settings_apply_contract.py` fails if the web clock save writes a field that the device does not apply (see section 8).
- `test_preflight_contracts.py` pins allowed HTTPD symbols, route limit (`max_uri_handlers`), captive-portal pieces (open AP, no DHCP restart, 2 KB headers), and much more.
- `test_project_identity_contract.py` scans the whole repo: no legacy agent branding, no old product title, creator credits intact.
- Spotify and captive DNS have C host tests for their pure logic (`spotify_logic.c`, `captive_dns_logic.c`).
- **Web pages are C string arrays** (`main/web_*_html.h`). To test or screenshot them, extract the string into HTML and drive it with Playwright (installed; `require('playwright')` works, do not reinstall). Put a fake device behind it with `page.route`. This caught real bugs (clock save 400, overlapping messages).

---

## 5. Source map (`main/`)

| File | Role |
|---|---|
| `main.c` | Boot order, watchdog task (restarts if the LVGL loop stalls >4 s after 10 s uptime, or the display bus wedges). Sets TZ `PCT7` first thing. |
| `ui.c` (about 5000 lines) | All screens: Home/Clock/Settings pager, swipe snapshot compositor, lyrics viewer, device settings, clock settings, easter egg, PWR-key handling. |
| `clock_face.c`, `basic_clock.c`, `clock_fonts.*` | The single Basic clock; custom digit renderers; 16 embedded fonts. |
| `network.c` | Wi-Fi (STA + setup AP), HTTP server and all API routes, captive portal, OTA from GitHub, time sync, Wi-Fi power. |
| `captive_dns.*`, `captive_dns_logic.c` | Setup-network DNS responder (answers every A lookup with the device). |
| `recognition.c`, `recognition_audio.c` | Song ID worker: Spotify first, else microphone + AudD; then LRCLIB lyrics. |
| `spotify.c`, `spotify_logic.c` | Spotify link (PKCE), token refresh, currently-playing. |
| `lrclib_client.c`, `lrclib_match.c`, `lyrics.c`, `lyric_sync.c`, `lyric_clock.c` | Lyrics lookup, LRC parsing, timing/tempo clock. |
| `song_cache.c`, `song_history.c` | Per-song cached lyrics and saved timing/tempo (NVS namespace `lf_songs`). |
| `settings.c/.h` | NVS settings (`lyric_ui`, schema 16), defaults, validation. |
| `settings_backup.c/.h` | Export/import of all settings; on-device copy in `sounds` LittleFS. |
| `power.c`, `power_logic.c` | Battery telemetry, screen-off CPU profile, AXP2101 PWR key, software power-off. |
| `weather.c`, `weather_visual.c` | Weather fetch (city → coordinates → forecast) and display. |
| `mic_pcm.c`, `mic_probe.c` | Microphone test and ES7210 probe (diagnostics only). |
| `ota_guard.c`, `ota_health_logic.c`, `image_policy.c` | OTA health/rollback and image checks. |
| `diagnostics.c` | Error log ring and the `/api/diagnostics` data. |
| `web_device_html.h`, `web_basic_html.h`, `web_weather_html.h` | The three web apps (home/settings/firmware, Clock Studio, Weather). |

Other folders: `components/esp32_s3_touch_amoled_1_75/` (vendored display/touch/audio board package, heavily modified), `tools/` (release packaging and validation scripts), `docs/` (the GitHub Pages site), `notes/` (old alpha-era test notes, not published), `tests/`.

---

## 6. Display, touch and performance: the hard-won lessons

The 0.8.x series was a long fight with choppy swipes, black flashes and freezes. The final architecture (keep it; don't casually change it):

- **Direct LVGL ↔ esp_lcd port** in the vendored BSP (`bsp_display_start_direct`), `LV_OS_NONE` with our own recursive mutex; LVGL task on core 1, **touch sampled in its own task on core 0** (10 ms awake, 40 ms screen-off; samples older than 150 ms count as released). SPI interrupt is serviced on core 1.
- **Partial rendering** into two 24-row **internal-RAM DMA strips**. Strip transfers use **ticketed accounting** (submitted/completed/ISR-done) so completions are never lost or stolen; the code never calls into esp_lcd while a transfer is outstanding (100 ms bound; a wedge triggers an immediate restart).
- **Swipe compositor:** while the pager moves, LVGL invalidation is disabled and frames are composed row-by-row from **idle RGB565 PSRAM snapshots** of the three pages and pushed straight to the panel, synced to the TE pulse. The settle is an immediate 240–360 ms ease-out on release. Result: about 25–60 fps swipes.
  - **Snapshots must stay fresh.** They are retaken after 400 ms of quiet when a visible page changes, at the start of a swipe if stale, every minute for the off-screen Clock, and on wake. (Stale snapshots caused: the old Song ID "LISTENING" screen during a swipe, and a clock showing an old time or UTC that then jumped.)
- **Watchdog + breadcrumbs:** `bsp_ui_stage_set(n)` writes a stage into RTC_NOINIT so the next boot's diagnostics show where the UI task was when it died (`ui_stage_at_last_restart`). Stages: 0 idle, 1 timers, 2 lock wait, 10 submit, 11 bus not completing, 12 transfer wait, 13 TE wait, 20 compose, 21 push, 30 snapshot, 31 swipe start, 32 swipe end.
- **Never block LVGL** on I²C, NVS, network or the PMIC. Touch and power run in their own tasks.
- **Don't use scaled/transformed labels** for the clock (each strip re-renders them offscreen); the clock uses natively generated font sizes (40/56/72/96/120).
- The on-screen FPS/CPU meter was removed in 0.8.127 (first shipped in 0.8.128); swipe statistics live in diagnostics (`swipes_recent`, `snapshot_*`, `pager_*`, `ui_*`).
- Boot must not flash: panel on at zero brightness, black default screen.
- sdkconfig notes: `-O2` (with a few `-Wno-error=` flags for new warnings), 8192 B main task stack, 32 KB data cache, Wi-Fi IRAM optimizations off (internal RAM is tight; a 64 KB cache and extra internal buffers once broke Song ID's TLS), TLS buffers in PSRAM, `CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048`, `CONFIG_HTTPD_MAX_URI_LEN=2048`, `CONFIG_LWIP_MAX_SOCKETS=16`.
- Free internal RAM is the scarce resource (about 70–80 KB). Check `free_internal_bytes` / `min_free_internal_bytes` in diagnostics after adding anything that allocates.

---

## 7. Networking and the web server

- Boot: if saved Wi-Fi connects within 12 s → station mode. Otherwise (or none saved) → **setup AP `LyricFlash-Setup`, open (no password)**, address 192.168.4.1, up to 4 clients. Saved credentials are never erased by falling back.
- **Captive portal rules (tested in `test_preflight_contracts.py`, documented in `BUILD.md`):**
  1. DNS responder answers every IPv4 lookup with the device address.
  2. Unknown pages redirect (302) to `/`, **only on the setup network** (home Wi-Fi keeps a plain 404).
  3. **Do not touch DHCP** (no restart, no option 114): doing so (1.0.3/1.0.4) broke it. The stock SoftAP DHCP already points clients at the device.
  4. **Request headers up to 2 KB** (`CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048`). The 512 B default made Firefox and Android get "connection reset", so the page never loaded.
  - `/captive` answers in RFC 8908 format; diagnostics show `captive_dns_*` counters.
- HTTP server: port 80, `max_uri_handlers=48` (a test pins the number: raise both together), 10 open sockets with LRU purge, `open_fn` marks web activity.
- **Wi-Fi power (1.1.1):** the radio is **never stopped**. Screen on or web used in the last 60 s → `WIFI_PS_MIN_MODEM`; screen off with no web use → `WIFI_PS_MAX_MODEM`. The device always reconnects after a drop. (Earlier versions stopped the radio when the screen was off; the owner wanted the web page always reachable.)
- **Time:** TZ `PCT7` is set at the very start of `app_main`, before any clock draw, then SNTP when online.
- **Routes (summary):** `/` (main settings page), `/firmware`, `/settings` (Clock Studio), `/weather`, `/api/info`, `/api/diagnostics`, `/api/preferences`, `/api/basic`, `/api/weather`, `/api/power`, `/api/wifi-scan`, `/wifi`, `/api/recognize` (+`/token`, `/start`, `/stop`), `/api/spotify/{client,link,unlink}`, `/spotify/callback`, `/api/backup` (GET download, POST restore), `/api/backup/status`, `/api/backup/device/{save,restore}`, `/api/install` (GitHub OTA), `/update`, `/captive`, `/api/song-database`, `/api/clock-fonts`, hardware mic tests. State-changing Song ID/backup/Spotify calls require header `X-LyricFlash-Manual-Test: 1` (so a stray page can't trigger them).
- **OTA from GitHub:** the Firmware page lists `alpha-build-<sha12>` prereleases (read through the GitHub API) plus the Pages catalog; the device only accepts URLs under the project's own release downloads, the `alpha-ota` raw branch, or Pages (`allowed_github_ota_url` in `network.c`). The page's version compare ranks `1.0.0` above `x-alpha`. Updates keep NVS (settings) intact.

---

## 8. Settings model

- NVS namespace `lyric_ui` (schema **16**, additive: old settings load into new fields). Others: `lf_network` (Wi-Fi), `lf_weather`, `lf_recog` (AudD token), `lf_spotify` (Client ID, refresh token), `lf_songs` (lyrics cache and per-song timing/tempo), `lf_wake_dbg`.
- **Clock model:** only the **Basic** face remains (index 5 of the per-face arrays; the other five arrays are legacy and carried for compatibility). Background is forced black; there is no ring and no ring colour. Font styles: 0–23 are old presets; **24–39 are the 16 embedded fonts**; default is **31 (Montserrat)**; the device's Font button cycles 24–39.
- **Rule (1.0.2 lesson):** `lf_ui_apply_basic_settings()` must copy **every** field that `handle_basic_post()` accepts. For a long time colours, sizes, date format and weather placement were validated and then silently dropped, so "colour doesn't work". `test_clock_settings_apply_contract.py` now enforces it. When you add a clock field: add it to the struct, NVS load/save, the GET, the POST validator, the POST apply loop, `lf_ui_apply_basic_settings`, the web page, and the renderer.
- The web page's colour pickers are `type=text` spectrum inputs; they must be sent as **numbers**, not `#rrggbb` strings (1.0.1 fixed an "Invalid clock settings" 400).
- **Settings backup (1.1.1):** exports every key of the five namespaces above as JSON (type-tagged; 64-bit as strings; blobs base64). Stored on-device in the `sounds` partition as `/lfstore/lyricflash-settings.json` (LittleFS). It **never formats** the partition: another app (PrintSphere) may own it; format happens only if the first 4 KB is blank. The file contains the Wi-Fi password and keys in clear text (the UI says so). Restore restarts the device. Lyrics cache/history are intentionally not included.

---

## 9. Song ID, Spotify, lyrics

- **Order when Play is pressed:** if Spotify is linked → ask Spotify. Playing → title/artist/position, no microphone. Nothing playing and an AudD token exists → fall back to the microphone. Not linked → microphone + AudD (the original, owner-verified path; do not alter it without re-pinning the baseline test).
- **Spotify (1.1.0):** Authorization Code with **PKCE**, no client secret. Spotify only redirects to HTTPS, so the redirect URI is `https://skiiirt1983.github.io/LyricFlash/spotify/`: a static page that forwards the one-time code to the device at the private IPv4 carried in the OAuth `state` (`<nonce>.<ip>`); it refuses anything not `10.x`, `172.16–31.x`, `192.168.x`. Scopes: `user-read-currently-playing user-read-playback-state`. The owner creates their own Spotify developer app (Premium required since Feb 2026) and pastes its **Client ID** into the device web page; the refresh token is stored in `lf_spotify`. Linking must be done from a phone/computer on the same Wi-Fi as the device.
- **Lyric timing:** the lyric clock starts from `audd_timecode_ms`. For AudD it includes a deliberate 1 s display delay (`LF_SYNC_DISPLAY_DELAY_MS`) tuned for microphone matches. For Spotify the position is exact, so the code adds that delay back plus 250 ms and halves the request latency (1.1.1). The lyric screen's **+/−** buttons change the *speed* (tempo %, saved per song), not a time offset; the slider sets a time offset. Per-song timing/tempo are stored with the cached lyrics.
- **LRCLIB** (`lrclib.net`): synced lyrics by title+artist, polite 300 ms spacing, a single fallback with a suffix-stripped title, 429 never retried automatically.
- **Warnings:** pressing Play without Wi-Fi or Song ID setup shows a **red** message under the Play button for 4.5 s ("Wi-Fi not connected", "Link Spotify or add AudD key", "No Wi-Fi and no Song ID"). Nothing is shown otherwise. (1.0.8 lesson: a "cancel pending" id of 0 equalled the attempt counter before the first scan and blocked all Home updates; zero now means "none pending".)
- **AudD:** 300 free requests then paid (about $5 per 1000); the owner's trial ended, which is why Spotify was added. Token in `lf_recog`. Device Settings shows "Spotify linked" / key status.

---

## 10. Power behaviour

- **CPU:** 240 MHz awake; 80 MHz minimum when the screen is off (DFS, no automatic light sleep). Screen off also slows touch/LVGL polling to 40 ms and pauses the swipe compositor timer. Standby was measured around 1% battery per hour **before** 1.1.1 kept Wi-Fi on; re-measure.
- **PWR button (1.1.1, owner-confirmed):** `power.c` configures the AXP2101 power key (reg 0x27: 128 ms power-on press, 1.5 s long-press report, 4 s hardware power-off kept as a fallback; reg 0x22 hardware long-press off enabled; IRQ regs 0x41/0x49 short/long press) and polls every 100 ms. **Tap = sleep screen / wake screen** (touch also wakes), **hold about 1.5 s = power off** (`lf_power_off()` sets reg 0x10 bit 0), tap when off = power on. Holding the chip's hardware 4 s still cuts power if the firmware hangs.
- Optional deep sleep exists in Device Settings (default off, untested on hardware).
- The device settings screen shows Wi-Fi name (or the setup network to join), Song ID status, IP and version.

---

## 11. Web site (GitHub Pages) and releases

- `docs/index.html`: the installer. It reads `beta/manifest.json` for the live version, offers over-the-air instructions, USB install keeping settings (`manifest-preserve.json`, writes only the app and OTA data), USB new install (full image at 0x0, erases settings), and a local-file installer (`beta/local-flash.js`). On browsers without Web Serial (phones) it shows one note instead of the install buttons. `docs/restore/` is the Waveshare factory-restore page; `docs/spotify/` is the Spotify redirect page; `docs/beta/index.html` just redirects home; the firmware files keep their `beta/` paths because the device also reads them.
- `docs/assets/pi-theme.css` is shared: section labels use flex so wrapped text lines up; the install buttons' message spans are blocks.
- The v1.0.0 GitHub release body once said "watch"; the owner fixed it by hand (releases cannot be edited from the tools available).
- Release asset sizes to expect: OTA bin about 2.96 MB, zip about 3.06 MB, full USB about 4.07 MB.

---

## 12. History of the project (short)

- **0.3–0.8.x (alpha, Oct–Nov 2026):** a previous developer built the firmware; the owner took over. Key problems solved: swipe choppiness, black flashes, freezes (see section 6), Song ID memory exhaustion, OTA/updater bugs, copy-log button, weather on the clock, start-up flashes. Many contracts in `tests/` record those fixes by version.
- **0.8.10x–0.8.124:** the big display rewrite (direct port, compositor, ticketed DMA, watchdog, touch task). **0.8.124** fixed the last freeze (never enter esp_lcd with a transfer outstanding).
- **0.8.125:** hidden logo wake-up animation. **0.8.126–0.8.130:** fresh snapshots, FPS meter removed, combined Clock Settings list, clock weather fix, time-zone-before-first-draw, screen-off power saving.
- **1.0.0:** first stable release (alpha labels removed; builds moved to `main`; stable `v<ver>` releases).
- **1.0.1–1.0.2:** Device Settings Wi-Fi/Song ID lines, open setup network, web clock save fixed, font alignment between device and web page; then **every web clock setting actually applied**.
- **1.0.3–1.0.6:** captive portal (took four tries; see section 7 for the final rules), network renamed `LyricFlash-Setup`, 2 KB request headers.
- **1.0.7–1.0.8:** red Play warnings (and the zero-attempt guard fix).
- **1.1.0:** Spotify as a Song ID source. **1.1.1:** Spotify lyric lag fix, always-on Wi-Fi modem sleep, PWR button tap/hold, settings backup. Install site rebuilt and auto-deployed alongside.

---

## 13. Open items and ideas (not done)

1. **Replacement board (Rev1.1) issues:**
   - **Flashing without BOOT:** idea, not built: a "USB flash mode" action (Device Settings and web page) that restarts the chip into the ROM download mode (set the force-download-boot bit, `esp_restart()`), so reinstalling never needs the BOOT button. Needing BOOT usually means the running firmware is crashing or sleeping while the flasher connects; it was never diagnosed on that board. The Basecamp bootloader would need the same.
   - **Quieter microphone:** unconfirmed whether sensitivity or channel mapping. Next step is the diagnostics numbers `microphone_pcm_mic1_rms/mic2_rms/peak` during music. Idea: a "Microphone sensitivity" setting (gain currently fixed at 27 dB). Less important now that Spotify is the main path.
   - Confirm LyricFlash runs and stays up on it (unknown).
2. **Case:** the owner is redesigning the case (branch `claude/loving-heisenberg-n1judx` holds the Python-generated enclosure work in its `enclosure/` folder: a finger-pressable PWR button, no slide switch). Firmware side is done for the PWR button.
3. **True panel sleep** (put the CO5300 in its real sleep mode instead of zero brightness) for longer standby: candidate for a later release; the screen-off/wake path is where the old black-screen bugs lived, so test it on its own.
4. **Standby battery with Wi-Fi on** is unmeasured; send a morning diagnostics (`battery_percent`, `uptime_seconds`, `wifi_radio_state`, `cpu_power_profile`).
5. Old `alpha-build-*` prereleases still exist (they are how the updater finds builds; keep recent ones). Old releases still carry the previous history's credits; the owner chose not to scrub GitHub.
6. After a full USB install the **on-device backup survives**, but an install that erases the whole flash removes it; the downloaded file is the safe copy.

---

## 14. Things that burned time (don't repeat)

- Restarting or reconfiguring DHCP for the captive portal. Small request-header limit. Radio-off for screen-off (the owner wants the web page reachable).
- A "0 means none" sentinel that also meant "first attempt".
- Settings validated and accepted but never applied; colour fields sent as strings.
- Stale swipe snapshots (old screens, old time, UTC time before the TZ was set).
- Wide `sed` version replacements relabelling old comments.
- Combining a command that contains `rm` patterns with other work: the safety check blocks the whole command; keep such commands separate and simple.
- Assuming a hardware wiring problem without asking: a battery switch wired across the cell, instead of in series with the red wire, is a short circuit.
- Trusting a green build as proof: ask the owner to test on the device and say exactly what to try.
