#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include "HttpTools.h"
#include "Wireless.h"

static const char *TAG = "HttpTools";

struct http_response_buf {
    char *data;
    size_t len;
    size_t capacity;
};

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_buf *buf = (http_response_buf *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0) {
                size_t new_len = buf->len + evt->data_len;
                if (new_len >= buf->capacity) {
                    size_t new_cap = new_len + 1024;
                    char *new_data = (char *)heap_caps_realloc(buf->data, new_cap, MALLOC_CAP_SPIRAM);
                    if (!new_data) {
                        ESP_LOGE(TAG, "Failed to allocate response buffer");
                        return ESP_FAIL;
                    }
                    buf->data = new_data;
                    buf->capacity = new_cap;
                }
                memcpy(buf->data + buf->len, evt->data, evt->data_len);
                buf->len = new_len;
                buf->data[buf->len] = '\0';
            }
            break;
        case HTTP_EVENT_ON_FINISH:
        case HTTP_EVENT_DISCONNECTED:
        case HTTP_EVENT_ERROR:
        default:
            break;
    }
    return ESP_OK;
}

cJSON *getCJsonFromUrl(const char *url) {
    if (!isWiFiConnected()) {
        ESP_LOGE(TAG, "Not connected to WiFi network");
        return nullptr;
    }

    ESP_LOGD(TAG, "Getting JSON from URL %s", url);

    http_response_buf buf = {};
    buf.capacity = 4096;
    buf.data = (char *)heap_caps_malloc(buf.capacity, MALLOC_CAP_SPIRAM);
    if (!buf.data) {
        ESP_LOGE(TAG, "Failed to allocate initial response buffer");
        return nullptr;
    }
    buf.data[0] = '\0';

    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.user_data = &buf;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(buf.data);
        return nullptr;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err == ESP_ERR_HTTP_INCOMPLETE_DATA && buf.len > 0) {
        ESP_LOGW(TAG, "Server closed without chunked terminator, got %d bytes — continuing", buf.len);
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
        free(buf.data);
        return nullptr;
    }

    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP GET failed with status code %d", status_code);
        free(buf.data);
        return nullptr;
    }

    cJSON *root = cJSON_ParseWithLength(buf.data, buf.len);
    free(buf.data);

    if (!root) {
        ESP_LOGE(TAG, "cJSON_Parse() failed");
        return nullptr;
    }

    return root;
}
