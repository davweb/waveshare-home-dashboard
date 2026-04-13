#include "HttpServer.h"
#include "LogBuffer.h"

#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_v8_port.h"

#include <string.h>
#include <time.h>

static const char *TAG = "HttpServer";

// ---------------------------------------------------------------------------
// GET /   — index page
// ---------------------------------------------------------------------------

static esp_err_t index_handler(httpd_req_t *req)
{
    http_send_html_head(req, "Device", NULL);
    httpd_resp_sendstr_chunk(req,
        "<div class=\"container-fluid\">"
        "<h1 class=\"h2\">Device</h1>"
        "<ul>"
        "<li><a href=\"/system\">/system</a> — system &amp; network information</li>"
        "<li><a href=\"/log\">/log</a> — device log</li>"
        "<li><a href=\"/screenshot\">/screenshot</a> — screen snapshot (BMP)</li>"
        "</ul>"
        "</div>");
    http_send_html_foot(req);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// GET /screenshot  — returns a 24-bit BMP image of the current LVGL screen
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
        ESP_LOGE(TAG, "screenshot: failed to allocate %lu bytes for BMP",
                 (unsigned long)file_size);
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
// GET /log  — HTML log viewer (chunked, auto-refreshes every 5 s)
// ---------------------------------------------------------------------------

static esp_err_t log_handler(httpd_req_t *req)
{
    http_send_html_head(req, "Device Log",
        "<div class=\"container-fluid py-3\">"
        "<h1 class=\"h3\">Device Log</h1>");

    httpd_resp_sendstr_chunk(req,
        "<table class=\"table table-sm table-hover\">"
        "<thead class=\"sticky-top\">"
        "<tr><th>Time</th><th>Level</th><th>Tag</th><th>Message</th></tr>"
        "</thead><tbody>");

    if (!g_log_buffer || g_log_buffer->count == 0) {
        httpd_resp_sendstr_chunk(req,
            "<tr><td colspan=\"4\" class=\"text-center text-secondary py-3\">No log entries yet.</td></tr>");
    } else {
        // Snapshot ring-buffer metadata under spinlock (just two ints — fast).
        portENTER_CRITICAL(&g_log_buffer->spinlock);
        uint32_t head  = g_log_buffer->head;
        uint32_t count = g_log_buffer->count;
        portEXIT_CRITICAL(&g_log_buffer->spinlock);

        // Iterate newest → oldest.
        char row[1024];
        char ts_buf[80];  // large enough for GCC worst-case format-truncation analysis

        for (uint32_t i = 0; i < count; i++) {
            uint32_t idx = (head + LOG_BUFFER_CAPACITY - 1u - i) % LOG_BUFFER_CAPACITY;
            const LogEntry *e = &g_log_buffer->entries[idx];

            // Timestamp
            if (e->wall_time > 1700000000LL) {
                struct tm t;
                localtime_r(&e->wall_time, &t);
                snprintf(ts_buf, sizeof(ts_buf),
                         "%04d-%02d-%02d %02d:%02d:%02d",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                         t.tm_hour, t.tm_min, t.tm_sec);
            } else {
                strlcpy(ts_buf, "(pre-NTP)", sizeof(ts_buf));
            }

            // Bootstrap contextual classes and level label
            char lc = e->level ? e->level : '?';
            const char *row_cls, *lv_cls, *lv_label;
            switch (lc) {
                case 'E':
                    row_cls = "table-danger";
                    lv_cls  = "text-danger fw-bold";
                    lv_label = "Error";
                    break;
                case 'W':
                    row_cls  = "table-warning";
                    lv_cls   = "text-warning fw-bold";
                    lv_label = "Warning";
                    break;
                case 'I':
                    row_cls  = "";
                    lv_cls   = "text-success";
                    lv_label = "Info";
                    break;
                case 'D':
                    row_cls  = "";
                    lv_cls   = "text-secondary";
                    lv_label = "Debug";
                    break;
                case 'V':
                    row_cls  = "";
                    lv_cls   = "text-secondary";
                    lv_label = "Verbose";
                    break;
                default:
                    row_cls  = "";
                    lv_cls   = "text-secondary";
                    lv_label = "?";
                    break;
            }

            // Write row, HTML-escaping tag and message in-place.
            int rlen = snprintf(row, sizeof(row),
                "<tr class=\"%s\"><td class=\"text-nowrap\">%s</td>"
                "<td class=\"%s text-nowrap\">%s</td><td class=\"text-nowrap\">",
                row_cls, ts_buf, lv_cls, lv_label);

            rlen += (int)html_escape(row + rlen, sizeof(row) - (size_t)rlen, e->tag);

            rlen += snprintf(row + rlen, sizeof(row) - (size_t)rlen,
                             "</td><td style=\"word-break:break-all\">");

            rlen += (int)html_escape(row + rlen, sizeof(row) - (size_t)rlen, e->message);

            strlcat(row + rlen, "</td></tr>", sizeof(row) - (size_t)rlen);

            httpd_resp_sendstr_chunk(req, row);
        }
    }

    httpd_resp_sendstr_chunk(req,
        "</tbody></table></div>");

    http_send_html_foot(req);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// GET /favicon.ico, /apple-touch-icon*.png  — suppress browser 404s with 204
// ---------------------------------------------------------------------------

static esp_err_t no_content_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Server startup
// ---------------------------------------------------------------------------

httpd_handle_t startHttpServer(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port       = 80;
    config.stack_size        = 12288;
    config.max_uri_handlers  = 10;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return nullptr;
    }

    static const httpd_uri_t index_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = index_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server, &index_uri);

    static const httpd_uri_t log_uri = {
        .uri      = "/log",
        .method   = HTTP_GET,
        .handler  = log_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server, &log_uri);

    static const httpd_uri_t screenshot_uri = {
        .uri      = "/screenshot",
        .method   = HTTP_GET,
        .handler  = screenshot_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server, &screenshot_uri);

    static const httpd_uri_t favicon_uri = {
        .uri      = "/favicon.ico",
        .method   = HTTP_GET,
        .handler  = no_content_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server, &favicon_uri);

    static const httpd_uri_t apple_touch_icon_uri = {
        .uri      = "/apple-touch-icon.png",
        .method   = HTTP_GET,
        .handler  = no_content_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server, &apple_touch_icon_uri);

    static const httpd_uri_t apple_touch_icon_precomposed_uri = {
        .uri      = "/apple-touch-icon-precomposed.png",
        .method   = HTTP_GET,
        .handler  = no_content_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server, &apple_touch_icon_precomposed_uri);

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    return server;
}
