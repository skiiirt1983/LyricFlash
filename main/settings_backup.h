#pragma once
#include <stdbool.h>
#include <stddef.h>

/* 1.1.1 settings backup.
 *
 * Every saved setting (Wi-Fi, clock, weather, display, Song ID keys, Spotify
 * link) is exported as one JSON document. It can be downloaded as a file, or
 * kept on the device in the "sounds" data partition (LittleFS), which firmware
 * updates and a normal full USB install do not overwrite. The lyrics cache
 * and song history are not included.
 *
 * The backup contains the Wi-Fi password and service tokens in plain text. */

/* JSON text of all settings (heap, free() it), or NULL. */
char *lf_backup_export(void);
/* Write the settings in json back to NVS. Returns keys restored, or -1 if the
   document is not a LyricFlash backup. The caller restarts the device. */
int lf_backup_import(const char *json);

/* On-device copy. */
bool lf_backup_device_save(void);
int lf_backup_device_restore(void);          /* keys restored, or -1 */
/* True when a device copy exists; version receives the firmware that made it. */
bool lf_backup_device_info(char *version, size_t size);
