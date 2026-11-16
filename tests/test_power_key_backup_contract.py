#!/usr/bin/env python3
"""1.1.1: PWR button, always-on Wi-Fi, Spotify timing lead and settings backup."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
power = (root / "main/power.c").read_text()
ui = (root / "main/ui.c").read_text()
net = (root / "main/network.c").read_text()
backup = (root / "main/settings_backup.c").read_text()
recog = (root / "main/recognition.c").read_text()

# PWR button: quick power-on, tap and hold reported, hardware 4 s hold kept.
assert "(v & 0xC0) | 0x10" in power, "key timing: 128 ms on, 4 s hardware off, 1.5 s long press"
assert "(v | 0x02) & ~0x01" in power, "hardware long-press power-off must stay enabled"
assert "#define AXP_IRQ_STATUS2 0x49" in power and "#define AXP_COMMON 0x10" in power
key = ui.split("static void power_key_tick(void)", 1)[1].split("static void on_tick_body", 1)[0]
assert "lf_power_off();" in key and "on_wake(NULL);" in key and "sleep_display();" in key
assert "power_key_tick();" in ui.split("static void on_tick_body(lv_timer_t *timer)", 1)[1][:200]

# Spotify lyrics: cancel the display delay tuned for microphone matches.
assert "now.progress_ms+LF_SYNC_DISPLAY_DELAY_MS+250U" in recog

# Backup: fixed namespace list, never formats a partition that holds data.
for ns in ('LF_CONFIG_NS', '"lf_network"', '"lf_weather"', '"lf_recog"', '"lf_spotify"'):
    assert ns in backup, ns
assert '"lf_songs"' not in backup
assert "if (!store_is_blank(part))" in backup and ".format_if_mount_failed = false" in backup
assert "allowed_namespace(ns->string)" in backup
for route in ('"/api/backup"', '"/api/backup/status"', '"/api/backup/device/save"', '"/api/backup/device/restore"'):
    assert route in net, route
print("PASS: PWR tap/hold, Spotify timing lead, guarded settings backup and routes")
