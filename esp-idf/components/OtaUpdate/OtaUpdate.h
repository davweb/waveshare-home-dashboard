#pragma once

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Callbacks — register these to hook OTA events into EEZ Flow actions
// ---------------------------------------------------------------------------

// Called when an update begins; new_version is the version string from server.
typedef void (*ota_start_cb_t)(const char *new_version);

// Called periodically during download; percent is 0–100.
// percent is -1 when the server did not supply a Content-Length.
typedef void (*ota_progress_cb_t)(int percent);

// Called when the update finishes or fails.
// success == true  → update flashed, device is about to reboot.
// success == false → something went wrong; message describes the failure.
typedef void (*ota_complete_cb_t)(bool success, const char *message);

void ota_set_callbacks(ota_start_cb_t    on_start,
                       ota_progress_cb_t  on_progress,
                       ota_complete_cb_t  on_complete);

// ---------------------------------------------------------------------------
// Core API
// ---------------------------------------------------------------------------

// Returns the version string compiled into the running firmware.
const char *ota_current_version(void);

// Called when OTA version info arrives via MQTT.
// Compares server_version to the running firmware; if different, downloads firmware_url via HTTP.
// Both strings are copied internally — safe to call with temporary buffers.
// Runs in a background task; caches the last seen values for ota_recheck_from_mqtt().
void ota_apply_mqtt_update(const char *server_version, const char *firmware_url);

#ifdef __cplusplus
}
#endif
