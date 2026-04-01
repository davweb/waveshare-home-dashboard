# Over-the-Air (OTA) Firmware Updates

The dashboard supports OTA firmware updates delivered through the existing Python dashboard server.  No separate update server is needed.

---

## How it works

```
┌─────────────────────────────────────────────────────────────┐
│  Developer machine                                          │
│                                                             │
│  idf.py build → waveshare_dashboard.bin                     │
│       │                                                     │
│       ▼                                                     │
│  curl POST /ota/firmware?version=1.0.1  ──────────────────► │
│                                          Dashboard server   │
│                                          stores             │
│                                          firmware.bin       │
│                                          version.txt        │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  ESP32 (every CONFIG_OTA_CHECK_INTERVAL_MINUTES)            │
│                                                             │
│  1. GET /ota/version                                        │
│       ← {"available":true,"version":"1.0.1","size":…}       │
│                                                             │
│  2. Compare server version with esp_app_get_description()   │
│       same  → done, no reboot                               │
│       diff  → proceed                                       │
│                                                             │
│  3. GET /ota/firmware  (streams binary directly to flash)   │
│       on_ota_start()  called                                │
│       on_ota_progress() called each % point                 │
│       on_ota_complete(true) called                          │
│       esp_restart()  → boots new firmware                   │
└─────────────────────────────────────────────────────────────┘
```

---

## Partition layout

The flash is split into two equal OTA slots so the device always has a known-good firmware to roll back to (via `idf.py flash` or the standard ESP-IDF rollback mechanism).

| Name     | Type | SubType | Offset     | Size       | Purpose                      |
|----------|------|---------|------------|------------|------------------------------|
| nvs      | data | nvs     | 0x009000   | 24 KB      | NVS key/value store          |
| phy_init | data | phy     | 0x00F000   | 4 KB       | RF calibration data          |
| otadata  | data | ota     | 0x010000   | 8 KB       | Active OTA slot pointer      |
| ota_0    | app  | ota_0   | 0x020000   | ~7.9 MB    | Firmware slot 0 (first boot) |
| ota_1    | app  | ota_1   | 0x810000   | ~7.9 MB    | Firmware slot 1              |

On first `idf.py flash` the binary lands in `ota_0`.  Each subsequent OTA update writes to whichever slot is *not* currently running, then sets it as the next boot target.

---

## Firmware version

The version string is set in `esp-idf/CMakeLists.txt`:

```cmake
set(PROJECT_VER "1.0.0")
```

It is compiled into the binary via the ESP-IDF `esp_app_desc_t` structure and can be read at runtime with `esp_app_get_description()->version`.  The OTA component uses this to decide whether a server-side firmware is newer.

**Bump `PROJECT_VER` before every release you intend to deploy via OTA.**  The comparison is a simple `strcmp`, so any change in the string (including `"1.0.0-rc1"`) triggers an update.

---

## Server setup

### Environment variable

| Variable          | Default    | Purpose                                  |
|-------------------|------------|------------------------------------------|
| `OTA_FIRMWARE_DIR` | `/tmp/ota` | Directory where firmware.bin is stored  |

Set this to a persistent path (e.g. `/data/ota`) if you are running the server in Docker so the firmware survives restarts.

### Uploading a new firmware build

```bash
# Build the firmware
idf.py build

# Upload to the server
curl -X POST "http://<server-host>:8000/ota/firmware?version=1.0.1" \
     --data-binary @build/waveshare_dashboard.bin

# Verify
curl http://<server-host>:8000/ota/version
# → {"available": true, "version": "1.0.1", "size": 1234567}
```

The upload replaces the stored firmware atomically (write-to-temp then rename) so a concurrent device download never sees a partial file.

### Checking what is stored

```bash
curl http://<server-host>:8000/ota/version
```

Response when firmware is present:

```json
{"available": true, "version": "1.0.1", "size": 1234567}
```

Response when no firmware has been uploaded:

```json
{"available": false, "version": null}
```

---

## Device-side configuration

| Kconfig option                   | Default | Purpose                                      |
|----------------------------------|---------|----------------------------------------------|
| `CONFIG_OTA_CHECK_INTERVAL_MINUTES` | 60   | Minutes between OTA polls                    |
| `CONFIG_DASHBOARD_SERVER_URL`    | *(set in sdkconfig.local)* | Base URL reused for OTA endpoints |

The OTA endpoints are derived from `CONFIG_DASHBOARD_SERVER_URL`:

- Version check: `<DASHBOARD_SERVER_URL>/ota/version`
- Firmware download: `<DASHBOARD_SERVER_URL>/ota/firmware`

---

## EEZ Flow hooks

Three C callback functions in `main.cpp` are called during an update.  Wire them to EEZ Flow actions to show update UI:

```cpp
static void on_ota_start(const char *new_version)
{
    // Called once when download begins.
    // Show an "Updating firmware…" overlay, disable touch, etc.
}

static void on_ota_progress(int percent)
{
    // Called each time the download progress percentage changes (0–100).
    // Update a progress bar global variable.
}

static void on_ota_complete(bool success, const char *message)
{
    // success == true  → update flashed; device reboots immediately after.
    // success == false → download or validation failed; message has details.
    // On failure you may want to show an error and resume normal operation.
}
```

The callbacks are registered in `app_main()`:

```cpp
ota_set_callbacks(on_ota_start, on_ota_progress, on_ota_complete);
```

---

## Rolling back a bad update

### Option A — re-flash via USB

```bash
idf.py -p /dev/cu.usbserial-* flash
```

This always writes to `ota_0` and marks it as the boot target.

### Option B — upload the previous firmware via the server

Upload the last known-good `.bin` with the old version string:

```bash
curl -X POST "http://<server-host>:8000/ota/firmware?version=1.0.0" \
     --data-binary @build_previous/waveshare_dashboard.bin
```

The device will detect the version mismatch on its next check interval and download the rollback image.

---

## Security note

The OTA endpoint has **no authentication**.  It is intended for use on a trusted local network only.  Do not expose the dashboard server port to the public internet.
