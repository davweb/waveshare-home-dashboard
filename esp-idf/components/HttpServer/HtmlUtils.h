#pragma once

#include "esp_http_server.h"
#include <stddef.h>

// Bootstrap CSS CDN URL — change this single constant to update the Bootstrap
// version used across all HTML pages served by this device.
#define BOOTSTRAP_CSS_URL \
    "https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css"

typedef enum {
    HTTP_BADGE_PRIMARY,
    HTTP_BADGE_SECONDARY,
    HTTP_BADGE_SUCCESS,
    HTTP_BADGE_DANGER,
    HTTP_BADGE_WARNING,
    HTTP_BADGE_INFO,
    HTTP_BADGE_LIGHT,
    HTTP_BADGE_DARK,
} http_badge_color_t;

#ifdef __cplusplus
extern "C" {
#endif

// Sends the standard HTML page header with Bootstrap and opens <body>.
// Sets Content-Type to text/html. Call at the start of any chunked HTML response.
// extra_head: optional raw HTML inserted inside <head> (e.g. a <meta refresh> tag).
void http_send_html_head(httpd_req_t *req, const char *title,
                         const char *extra_head);

// Write HTML-escaped src into dst, stopping at dst_size-1 bytes.
// Escapes &, <, >.  Returns number of bytes written.
size_t html_escape(char *dst, size_t dst_size, const char *src);

// Sends a Bootstrap table row with a header cell (key) and a data cell (val).
// val is rendered as raw HTML — use pre-formatted strings or escape manually.
void http_send_row(httpd_req_t *req, const char *key, const char *val);

// Sends an h2 heading and opens a Bootstrap table/tbody for a named section.
void http_send_section_open(httpd_req_t *req, const char *title);

// Closes the tbody/table opened by http_send_section_open.
void http_send_section_close(httpd_req_t *req);

// Writes a Bootstrap badge span into buf. Returns buf for convenience,
// allowing direct use as the val argument to http_send_row.
char *http_format_badge(char *buf, size_t buf_size,
                        http_badge_color_t color, const char *text);

// Closes the container div, body, and html elements, then terminates the
// chunked response. Call once at the end of every chunked HTML handler.
void http_send_html_foot(httpd_req_t *req);

#ifdef __cplusplus
}
#endif
