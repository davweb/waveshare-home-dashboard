#pragma once

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

void ota_set_callbacks(ota_start_cb_t  on_start,
                       ota_progress_cb_t on_progress,
                       ota_complete_cb_t on_complete);

// ---------------------------------------------------------------------------
// Core API
// ---------------------------------------------------------------------------

// Returns the version string compiled into the running firmware.
const char *ota_current_version(void);

// Contacts <server_url>/ota/version, compares with the running version.
// If a different version is available, streams the firmware from
// <server_url>/ota/firmware and applies it, then calls esp_restart().
// Returns false if no update is needed or an error occurred (no reboot).
bool ota_check_and_update(const char *server_url);

#ifdef __cplusplus
}
#endif
