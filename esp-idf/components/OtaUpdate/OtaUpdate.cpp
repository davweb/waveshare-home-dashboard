#include "OtaUpdate.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_app_format.h"
#include "cJSON.h"
#include "Wireless.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

static const char *TAG = "OtaUpdate";

// ---------------------------------------------------------------------------
// Callback registry
// ---------------------------------------------------------------------------

static ota_start_cb_t    s_on_start    = nullptr;
static ota_progress_cb_t s_on_progress = nullptr;
static ota_complete_cb_t s_on_complete = nullptr;
static ota_checked_cb_t  s_on_checked  = nullptr;

void ota_set_callbacks(ota_start_cb_t    on_start,
                       ota_progress_cb_t  on_progress,
                       ota_complete_cb_t  on_complete,
                       ota_checked_cb_t   on_checked)
{
    s_on_start    = on_start;
    s_on_progress = on_progress;
    s_on_complete = on_complete;
    s_on_checked  = on_checked;
}

// ---------------------------------------------------------------------------
// Running version
// ---------------------------------------------------------------------------

const char *ota_current_version(void)
{
    return esp_app_get_description()->version;
}

// ---------------------------------------------------------------------------
// Step 1 — query the server for the available firmware version
// ---------------------------------------------------------------------------
// Returns true and writes the version string into out_version on success.

static bool fetch_server_version(const char *url, char *out_version, size_t out_len)
{
    esp_http_client_config_t cfg = {};
    cfg.url        = url;
    cfg.timeout_ms = 10000;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "HTTP init failed for version check");
        return false;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "Version check returned HTTP %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    // Small JSON response — 256 bytes is plenty
    char buf[256] = {};
    int read = esp_http_client_read(client, buf, sizeof(buf) - 1);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (read <= 0) {
        ESP_LOGE(TAG, "Empty version response");
        return false;
    }

    cJSON *root = cJSON_ParseWithLength(buf, (size_t)read);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse version JSON");
        return false;
    }

    bool ok = false;
    cJSON *version_item = cJSON_GetObjectItem(root, "version");
    cJSON *available = cJSON_GetObjectItem(root, "available");
    if (cJSON_IsTrue(available)) {
        if (cJSON_IsString(version_item) && version_item->valuestring) {
            strlcpy(out_version, version_item->valuestring, out_len);
            ok = true;
        }
    } else {
        ESP_LOGI(TAG, "Server reports no firmware available");
    }

    cJSON_Delete(root);
    return ok;
}

// ---------------------------------------------------------------------------
// Step 2 — stream firmware from server and flash to the next OTA partition
// ---------------------------------------------------------------------------

#define OTA_BUF_SIZE 4096

static bool download_and_flash(const char *url, const char *new_version)
{
    if (s_on_start) s_on_start(new_version);

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA update partition found — check partition table");
        if (s_on_complete) s_on_complete(false, "No OTA partition");
        return false;
    }
    ESP_LOGI(TAG, "Flashing to partition: %s", update_partition->label);

    esp_http_client_config_t cfg = {};
    cfg.url        = url;
    cfg.timeout_ms = 120000;   // 2 min — large image on slow link
    cfg.buffer_size = OTA_BUF_SIZE;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        if (s_on_complete) s_on_complete(false, "HTTP init failed");
        return false;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        if (s_on_complete) s_on_complete(false, "HTTP open failed");
        return false;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "Firmware download returned HTTP %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        if (s_on_complete) s_on_complete(false, "HTTP error");
        return false;
    }
    ESP_LOGI(TAG, "Downloading %d bytes of firmware", content_length);

    esp_ota_handle_t ota_handle;
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        if (s_on_complete) s_on_complete(false, "OTA begin failed");
        return false;
    }

    char *buf = static_cast<char *>(malloc(OTA_BUF_SIZE));
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate OTA buffer");
        esp_ota_abort(ota_handle);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        if (s_on_complete) s_on_complete(false, "Out of memory");
        return false;
    }

    int total_received = 0;
    int last_percent   = -1;
    int read_len;

    while ((read_len = esp_http_client_read(client, buf, OTA_BUF_SIZE)) > 0) {
        err = esp_ota_write(ota_handle, buf, (size_t)read_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            free(buf);
            esp_ota_abort(ota_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            if (s_on_complete) s_on_complete(false, "Flash write failed");
            return false;
        }
        total_received += read_len;

        if (s_on_progress && content_length > 0) {
            int percent = (total_received * 100) / content_length;
            if (percent != last_percent) {
                last_percent = percent;
                s_on_progress(percent);
            }
        }
    }

    free(buf);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total_received == 0) {
        ESP_LOGE(TAG, "No data received during download");
        esp_ota_abort(ota_handle);
        if (s_on_complete) s_on_complete(false, "Download empty");
        return false;
    }

    ESP_LOGI(TAG, "Download complete: %d bytes", total_received);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end (validation) failed: %s", esp_err_to_name(err));
        if (s_on_complete) s_on_complete(false, "Image validation failed");
        return false;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        if (s_on_complete) s_on_complete(false, "Set boot partition failed");
        return false;
    }

    if (s_on_complete) s_on_complete(true, new_version);
    ESP_LOGI(TAG, "OTA to %s complete — rebooting", new_version);
    esp_restart(); // does not return
    return true;   // unreachable
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

static bool do_ota_check_and_update(const char *server_url)
{
    OtaLastCheck last_check = {};
    last_check.check_time = time(nullptr);

    if (!isWiFiConnected()) {
        ESP_LOGW(TAG, "WiFi not connected — skipping OTA check");
        if (s_on_checked) s_on_checked(last_check);
        return false;
    }

    char version_url[128];
    snprintf(version_url, sizeof(version_url), "%s/ota/version", server_url);

    if (!fetch_server_version(version_url, last_check.server_version, sizeof(last_check.server_version))) {
        if (s_on_checked) s_on_checked(last_check);
        return false;
    }

    if (s_on_checked) s_on_checked(last_check);

    const char *running = ota_current_version();
    ESP_LOGI(TAG, "Running firmware: %s  Server firmware: %s", running, last_check.server_version);

    if (strcmp(last_check.server_version, running) == 0) {
        ESP_LOGI(TAG, "Firmware is up to date");
        return false;
    }

    char firmware_url[128];
    snprintf(firmware_url, sizeof(firmware_url), "%s/ota/firmware", server_url);

    return download_and_flash(firmware_url, last_check.server_version);
}

static volatile bool s_ota_running = false;

void ota_check_and_update(const char *server_url)
{
    if (s_ota_running) {
        ESP_LOGW(TAG, "OTA check already in progress — ignoring");
        return;
    }
    s_ota_running = true;

    xTaskCreate([](void *arg) {
        do_ota_check_and_update(static_cast<const char *>(arg));
        s_ota_running = false;
        vTaskDelete(nullptr);
    }, "ota_check", 8192, const_cast<char *>(server_url), tskIDLE_PRIORITY + 1, nullptr);
}
