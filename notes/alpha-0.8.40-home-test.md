# LyricFlash Alpha 0.8.40 — visual Home test candidate

Built from the proven Alpha 0.8.39 manual lyric timeline on the separate development branch.

## Physical Home expectations

- A single large, circular blue Play symbol on a black screen with two subtle blue rings.
- No rectangular Clock, Settings, Lyrics or OTA menu buttons on Home.
- Short tap Play begins an explicitly authorized microphone scan. Icon switches to Stop while the recognition worker is busy. No background listening.
- Song title and artist appear below the central control when identified.
- Recognized timed lyrics: tap the song information to open the 0.8.39 manual synchronized-lyrics viewer; when manual playback time has started, Home can display the currently selected lyric.
- Clock and Settings remain reachable with directional page swipes, with no new root screen.
- Charge indicator preserves the existing AXP2101 charging color.
- Avoid changing image downloads or spin-loop rendering; Home updates only changed LVGL labels and icon color.
- The existing preflight suite passed before compiling.

## Physical verification after successful build

1. Check first boot and swipe Clock <-> Home <-> Settings repeatedly, including tap-to-wake.
2. Verify large Play is tappable and a swipe that starts over it navigates rather than scanning.
3. Verify Play -> Stop during scan and Stop cancels a scan, with microphone off when idle.
4. Confirm AudD identification, LRCLIB lyrics lookup, and tap-song-to-Lyrics manual timeline.
5. Verify battery percent and color on USB/charging.
6. Confirm OTA and existing Wi-Fi / API credentials are preserved after app-only update.

**Release channel:** Private experimental alpha only. Not promoted to public stable v1.0.
