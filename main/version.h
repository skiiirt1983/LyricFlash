#pragma once
/* Private alpha hardware recovery candidate, never public/stable. */
#define LF_VERSION "0.8.11-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.11"
/* Emergency recovery: a single-root Home startup; independently navigable
   Clock/Settings; protected wake/input lifecycle and cached Basic ring.
   Animated three-page pager is disabled until hardware recovery validates.
   Preserves NVS preferences and OTA partition compatibility. */
/* Build checkpoint: single-root recovery with independent navigation, clock cache, OTA preservation. */
/* Touch threshold build checkpoint: tap ownership audited and source regressions green. */
