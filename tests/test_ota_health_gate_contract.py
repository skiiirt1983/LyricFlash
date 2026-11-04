#!/usr/bin/env python3
"""New OTA images must prove a live display before becoming permanent."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/"main/main.c").read_text()
guard=(root/"main/ota_guard.c").read_text()
logic=(root/"main/ota_health_logic.c").read_text()
cmake=(root/"main/CMakeLists.txt").read_text()
assert "lf_ota_guard_start_health_check();" in main
assert "lf_ota_guard_confirm_startup();" not in main
assert main.index("lf_network_start()") < main.index("lf_ota_guard_start_health_check()")
assert "if (!pending_verification()) return true;" in guard
assert "LF_OTA_HEALTH_REQUIRED_SAMPLES 3U" in guard
assert "LF_OTA_HEALTH_SAMPLE_MS 2500U" in guard
assert "LF_OTA_HEALTH_DEADLINE_MS 40000U" in guard
assert "lf_ota_health_sample_ok(&sample)" in guard
assert "stable_samples=0;" in guard
assert "if (stable_samples>=LF_OTA_HEALTH_REQUIRED_SAMPLES)" in guard
assert guard.index("if (stable_samples>=LF_OTA_HEALTH_REQUIRED_SAMPLES)") < guard.index("lf_ota_guard_confirm_startup();")
assert "lf_ota_guard_failure(\"Post-boot UI health check failed\")" in guard
assert "lf_network_is_connected()" not in guard
assert "ota_health_logic.c" in cmake
assert "ui_tick_age_ms<=1500U" in logic
assert "refresh_ready>0" in logic
assert "free_heap_bytes>=16384U" in logic
print("PASS: pending-only post-boot OTA health/rollback and no cloud dependency")
