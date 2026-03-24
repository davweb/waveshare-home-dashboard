#include "HttpServer.h"

#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_v8_port.h"

#include <cstring>

static const char *TAG = "HttpServer";

// ---------------------------------------------------------------------------
// /ping
// ---------------------------------------------------------------------------

static esp_err_t ping_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// /screenshot  — returns a 24-bit BMP image of the current LVGL screen
// ---------------------------------------------------------------------------

// Write a little-endian 16-bit value into a byte buffer.
static inline void write_le16(uint8_t *buf, uint16_t v)
{
    buf[0] = v & 0xFF;
    buf[1] = (v >> 8) & 0xFF;
}

// Write a little-endian 32-bit value into a byte buffer.
static inline void write_le32(uint8_t *buf, uint32_t v)
{
    buf[0] = v & 0xFF;
    buf[1] = (v >> 8) & 0xFF;
    buf[2] = (v >> 16) & 0xFF;
    buf[3] = (v >> 24) & 0xFF;
}

static esp_err_t screenshot_handler(httpd_req_t *req)
{
    // --- 1. Take the LVGL snapshot (must hold the LVGL mutex) ---
    lv_img_dsc_t *snapshot = nullptr;

    if (!lvgl_port_lock(portMAX_DELAY)) {
        ESP_LOGE(TAG, "screenshot: failed to acquire LVGL mutex");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    snapshot = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
    lvgl_port_unlock();

    if (!snapshot) {
        ESP_LOGE(TAG, "screenshot: lv_snapshot_take failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    const uint32_t width  = snapshot->header.w;
    const uint32_t height = snapshot->header.h;

    // BMP rows must be padded to a multiple of 4 bytes.
    // At 24 bpp: bytes per row = width * 3, padded up.
    const uint32_t row_stride = (width * 3u + 3u) & ~3u;
    const uint32_t pixel_data_size = row_stride * height;
    const uint32_t file_size = 54u + pixel_data_size;

    // --- 2. Allocate the full BMP in PSRAM ---
    uint8_t *bmp = static_cast<uint8_t *>(
        heap_caps_calloc(1, file_size, MALLOC_CAP_SPIRAM));

    if (!bmp) {
        ESP_LOGE(TAG, "screenshot: failed to allocate %lu bytes for BMP", (unsigned long)file_size);
        lv_snapshot_free(snapshot);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // --- 3. Write BMP file header (14 bytes) ---
    bmp[0] = 'B';
    bmp[1] = 'M';
    write_le32(&bmp[2],  file_size);   // file size
    write_le32(&bmp[6],  0);           // reserved
    write_le32(&bmp[10], 54);          // offset to pixel data

    // --- 4. Write BITMAPINFOHEADER (40 bytes, starting at offset 14) ---
    write_le32(&bmp[14], 40);                   // header size
    write_le32(&bmp[18], width);                // image width
    // Negative height → top-down row order (matches LVGL's layout)
    write_le32(&bmp[22], static_cast<uint32_t>(-(int32_t)height));
    write_le16(&bmp[26], 1);                    // colour planes
    write_le16(&bmp[28], 24);                   // bits per pixel
    write_le32(&bmp[30], 0);                    // compression (BI_RGB)
    write_le32(&bmp[34], pixel_data_size);      // image size (may be 0 for BI_RGB, but set it)
    // Remaining fields (pixels-per-metre, palette size) left as 0.

    // --- 5. Convert RGB565 → BGR888 pixel data ---
    // LV_COLOR_DEPTH=16, LV_COLOR_16_SWAP=0 → pixels are native-endian RGB565.
    // On the little-endian ESP32: uint16_t layout is R[15:11] G[10:5] B[4:0].
    const uint16_t *src = reinterpret_cast<const uint16_t *>(snapshot->data);
    uint8_t *pixel_data = bmp + 54;

    for (uint32_t y = 0; y < height; y++) {
        uint8_t *row = pixel_data + y * row_stride;
        const uint16_t *src_row = src + y * width;

        for (uint32_t x = 0; x < width; x++) {
            const uint16_t p = src_row[x];
            const uint8_t r5 = (p >> 11) & 0x1Fu;
            const uint8_t g6 = (p >>  5) & 0x3Fu;
            const uint8_t b5 =  p        & 0x1Fu;

            // Expand 5→8 and 6→8 bits, then write in BGR order for BMP.
            row[x * 3 + 0] = (b5 << 3) | (b5 >> 2);
            row[x * 3 + 1] = (g6 << 2) | (g6 >> 4);
            row[x * 3 + 2] = (r5 << 3) | (r5 >> 2);
        }
        // Padding bytes already 0 from calloc.
    }

    lv_snapshot_free(snapshot);

    // --- 6. Send the BMP ---
    httpd_resp_set_type(req, "image/bmp");
    esp_err_t ret = httpd_resp_send(req, reinterpret_cast<const char *>(bmp),
                                    static_cast<ssize_t>(file_size));
    heap_caps_free(bmp);
    return ret;
}

// ---------------------------------------------------------------------------
// Server startup
// ---------------------------------------------------------------------------

void startHttpServer(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port  = 80;
    config.stack_size   = 8192;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    static const httpd_uri_t ping_uri = {
        .uri     = "/ping",
        .method  = HTTP_GET,
        .handler = ping_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server, &ping_uri);

    static const httpd_uri_t screenshot_uri = {
        .uri     = "/screenshot",
        .method  = HTTP_GET,
        .handler = screenshot_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server, &screenshot_uri);

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
}
