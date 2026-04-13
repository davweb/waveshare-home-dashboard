#include "LogBuffer.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

static const char *TAG = "LogBuffer";

LogBuffer *g_log_buffer = nullptr;
static vprintf_like_t s_original_vprintf = nullptr;

// Per-core re-entrancy guard to prevent infinite recursion if the hook itself
// triggers a log call (e.g. if an internal function called from the hook logs).
static volatile bool s_in_hook[2] = {false, false};

// ---------------------------------------------------------------------------
// Ring buffer write
// ---------------------------------------------------------------------------

static void store_log_entry(char level, const char *tag, const char *msg)
{
    // Atomically claim a write slot — critical section is tiny (index arithmetic only)
    portENTER_CRITICAL(&g_log_buffer->spinlock);
    uint32_t slot = g_log_buffer->head;
    g_log_buffer->head = (slot + 1) % LOG_BUFFER_CAPACITY;
    if (g_log_buffer->count < LOG_BUFFER_CAPACITY) g_log_buffer->count++;
    portEXIT_CRITICAL(&g_log_buffer->spinlock);

    // Write entry outside critical section. A concurrent HTTP reader may see a
    // partially-written entry — acceptable for a debug log viewer.
    LogEntry *e = &g_log_buffer->entries[slot];
    e->wall_time = time(NULL);
    e->level     = level;
    strlcpy(e->tag,     tag, LOG_TAG_MAX);
    strlcpy(e->message, msg, LOG_MSG_MAX);
}

// ---------------------------------------------------------------------------
// Log line parser
// ---------------------------------------------------------------------------
// ESP-IDF log format (with or without ANSI color codes):
//   [ESC[x;ym]L (ticks) TAG: message[ESC[0m]\n
// Returns true and fills out_* on success.
// Ideally we'd use ESP-IDF Log v2 which allows access to structured data but
// it's not a simple switch.

static bool parse_log_line(const char *line,
                            char *out_level,
                            char *out_tag,  size_t tag_len,
                            char *out_msg,  size_t msg_len)
{
    const char *p = line;

    // Skip ANSI escape prefix: ESC[ ... m
    if (*p == '\033') {
        const char *m = strchr(p, 'm');
        if (m) p = m + 1;
    }

    // Level character
    char lv = *p;
    if (lv != 'E' && lv != 'W' && lv != 'I' && lv != 'D' && lv != 'V') {
        return false;
    }
    *out_level = lv;
    p++;

    // Skip " (ticks) " — find the closing ")"
    const char *paren = strchr(p, ')');
    if (!paren) return false;
    const char *tag_start = paren + 2;  // skip ") "

    // Tag ends at the first ": "
    const char *colon = strstr(tag_start, ": ");
    if (!colon) return false;

    size_t tag_bytes = (size_t)(colon - tag_start);
    if (tag_bytes >= tag_len) tag_bytes = tag_len - 1;
    memcpy(out_tag, tag_start, tag_bytes);
    out_tag[tag_bytes] = '\0';

    // Message starts after ": "
    const char *msg_start = colon + 2;
    strlcpy(out_msg, msg_start, msg_len);

    // Strip trailing ANSI reset sequence (ESC[...m) and whitespace
    char *ansi = strstr(out_msg, "\033[");
    if (ansi) *ansi = '\0';
    size_t mlen = strlen(out_msg);
    while (mlen > 0 && (out_msg[mlen-1] == '\n' || out_msg[mlen-1] == '\r'
                        || out_msg[mlen-1] == ' ')) {
        out_msg[--mlen] = '\0';
    }

    return true;
}

// ---------------------------------------------------------------------------
// vprintf hook
// ---------------------------------------------------------------------------

static int log_vprintf_hook(const char *fmt, va_list args)
{
    // Forward to the original UART output first (consumes args).
    va_list copy;
    va_copy(copy, args);
    int ret = s_original_vprintf(fmt, args);

    if (g_log_buffer) {
        int core = (int)xPortGetCoreID();
        if (core < 0 || core > 1) core = 0;  // defensive clamp
        if (!s_in_hook[core]) {
            s_in_hook[core] = true;

            // Format the line into a stack buffer (no heap allocation).
            char line[LOG_MSG_MAX + 80];
            vsnprintf(line, sizeof(line), fmt, copy);

            char level = '?';
            char parsed_tag[LOG_TAG_MAX];
            char parsed_msg[LOG_MSG_MAX];
            if (parse_log_line(line, &level,
                               parsed_tag, sizeof(parsed_tag),
                               parsed_msg, sizeof(parsed_msg))) {
                store_log_entry(level, parsed_tag, parsed_msg);
            }

            s_in_hook[core] = false;
        }
    }

    va_end(copy);
    return ret;
}

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------

void log_buffer_init(void)
{
    g_log_buffer = static_cast<LogBuffer *>(
        heap_caps_calloc(1, sizeof(LogBuffer), MALLOC_CAP_SPIRAM));
    if (!g_log_buffer) {
        // No ESP_LOGE here — hook not yet installed, safe to log directly
        return;
    }
    portMUX_INITIALIZE(&g_log_buffer->spinlock);
    s_original_vprintf = esp_log_set_vprintf(log_vprintf_hook);
    ESP_LOGI(TAG, "Log buffer initialised: %d entries (%.1f KB PSRAM)",
             LOG_BUFFER_CAPACITY,
             (float)(sizeof(LogBuffer)) / 1024.0f);
}
