#pragma once

#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#define LOG_TAG_MAX          24
#define LOG_MSG_MAX         128
#define LOG_BUFFER_CAPACITY 400

typedef struct {
    time_t wall_time;           // time(NULL) at capture; 0 or invalid before NTP sync
    char   level;               // 'E', 'W', 'I', 'D', 'V'
    char   tag[LOG_TAG_MAX];
    char   message[LOG_MSG_MAX];
} LogEntry;

typedef struct {
    LogEntry     entries[LOG_BUFFER_CAPACITY];
    uint32_t     head;          // index of the next slot to write (wraps)
    uint32_t     count;         // number of valid entries, capped at LOG_BUFFER_CAPACITY
    portMUX_TYPE spinlock;
} LogBuffer;

// Allocated in PSRAM by log_buffer_init(). NULL until then.
extern LogBuffer *g_log_buffer;

// Install the vprintf hook and allocate the PSRAM ring buffer.
// Call once, as early as possible in app_main, before startHttpServer().
void log_buffer_init(void);
