#pragma once
/* LyricFlash 0.8.60 private alpha: touch feedback and lighter lyrics overlay.
 * Based on user-tested 0.8.59 cancel recovery (f9f21a4e1d2a).
 * Song ID, AudD, audio capture, LRCLIB, network and song cache unchanged.
 * Rollback: safe/0.8.59-hardware-tested and safe/0.8.57-hardware-verified. */
#define LF_VERSION "0.8.60-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.60"

/* UI: remove trailing cancel status; 72x64 + / - touch areas, obvious
 * on-contact highlight and 280ms confirmation glow; lyrics timing overlay
 * is 15% opaque. Existing reliable deferred scan cancellation preserved.
 * Build verification does not establish hardware reliability. */
