#pragma once

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
char server_version[32]; // Last version string seen from the server ("" if never checked)
    time_t check_time;       // Time of last check (0 if never checked)
} OtaLastCheck;

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

// Called after every version check (successful or not); result contains the server version and check time.
typedef void (*ota_checked_cb_t)(OtaLastCheck result);

void ota_set_callbacks(ota_start_cb_t    on_start,
                       ota_progress_cb_t  on_progress,
                       ota_complete_cb_t  on_complete,
                       ota_checked_cb_t   on_checked);

// ---------------------------------------------------------------------------
// Core API
// ---------------------------------------------------------------------------

// Returns the version string compiled into the running firmware.
const char *ota_current_version(void);

// Runs ota_check_and_update in a background FreeRTOS task so the caller
// (e.g. an LVGL event handler) is not blocked.
void ota_check_and_update(const char *server_url);

#ifdef __cplusplus
}
#endif
