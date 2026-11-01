#pragma once

// Confirms a newly installed image only after minimal startup diagnostics.
// Does not replace ROM bootloader recovery or prove existing PrintSphere rollback support.
void lf_ota_guard_confirm_startup(void);
void lf_ota_guard_failure(const char *reason);
