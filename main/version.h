#pragma once
/* 0.8.31: independently test local microphone audio input after 0.8.30's
 * physical display/touch recovery and ES7210 I2C detection passed.
 * User-initiated, 3-second bounded PCM amplitude check with early stop;
 * no raw audio persistence or network uploads. This stage uses existing
 * 22050 Hz mono Waveshare BSP codec path and reports aggregate levels only.
 * BSP keeps I2S allocated after codec close; idle clock power requires
 * a separate later teardown milestone. Preserve exact 0.8.26 display
 * driver, OTA and all saved settings. Hardware audio still unverified. */
#define LF_VERSION "0.8.31-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.31"
