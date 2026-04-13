#include "HtmlUtils.h"

#include "esp_http_server.h"

#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// http_send_html_head
// ---------------------------------------------------------------------------

void http_send_html_head(httpd_req_t *req, const char *title,
                         const char *extra_head)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req,
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    char buf[128];
    snprintf(buf, sizeof(buf), "<title>%s</title>", title);
    httpd_resp_sendstr_chunk(req, buf);
    httpd_resp_sendstr_chunk(req,
        "<link rel=\"stylesheet\" href=\"" BOOTSTRAP_CSS_URL "\">");
    if (extra_head) httpd_resp_sendstr_chunk(req, extra_head);
    httpd_resp_sendstr_chunk(req, "</head><body>");
}

// ---------------------------------------------------------------------------
// html_escape
// ---------------------------------------------------------------------------

size_t html_escape(char *dst, size_t dst_size, const char *src)
{
    size_t out = 0;
    while (*src && out + 8 < dst_size) {
        if (*src == '&') {
            memcpy(dst + out, "&amp;", 5);
            out += 5;
        } else if (*src == '<') {
            memcpy(dst + out, "&lt;", 4);
            out += 4;
        } else if (*src == '>') {
            memcpy(dst + out, "&gt;", 4);
            out += 4;
        } else {
            dst[out++] = *src;
        }
        src++;
    }
    dst[out] = '\0';
    return out;
}

// ---------------------------------------------------------------------------
// Table section helpers
// ---------------------------------------------------------------------------

// Sends a key/value table row. val is rendered as raw HTML (use pre-formatted strings).
void http_send_row(httpd_req_t *req, const char *key, const char *val)
{
    char buf[384];
    snprintf(buf, sizeof(buf),
        "<tr><th scope=\"row\" class=\"w-25\">%s</th>"
        "<td>%s</td></tr>",
        key, val);
    httpd_resp_sendstr_chunk(req, buf);
}

void http_send_section_open(httpd_req_t *req, const char *title)
{
    char buf[192];
    snprintf(buf, sizeof(buf),
        "<h2 class=\"h5\">%s</h2>"
        "<table class=\"table table-sm table-bordered\"><tbody>",
        title);
    httpd_resp_sendstr_chunk(req, buf);
}

void http_send_section_close(httpd_req_t *req)
{
    httpd_resp_sendstr_chunk(req, "</tbody></table>");
}

char *http_format_badge(char *buf, size_t buf_size,
                        http_badge_color_t color, const char *text)
{
    static const char * const color_names[] = {
        "primary", "secondary", "success", "danger",
        "warning", "info", "light", "dark",
    };
    snprintf(buf, buf_size, "<span class=\"badge text-bg-%s\">%s</span>",
             color_names[color], text);
    return buf;
}

void http_send_html_foot(httpd_req_t *req)
{
    httpd_resp_sendstr_chunk(req, "</div></body></html>");
    httpd_resp_send_chunk(req, NULL, 0);
}
