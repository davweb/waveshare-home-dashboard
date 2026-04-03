#pragma once

#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_log.h"
#include "soc/soc.h"
#include <lvgl_v8_port.h>
#include <structs.h>
#include <vars.h>
#include <eez-flow.h>

using namespace eez;

// SRAM always 512kb on an ESP32
static const int SOC_SRAM_TOTAL_KB = 512;

inline void updateMemoryUsage() {
    int sram_total  = SOC_SRAM_TOTAL_KB;
    int sram_free   = (int)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    int sram_used   = sram_total - sram_free;

    int psram_total = (int)(esp_psram_get_size() / 1024);
    int psram_free  = (int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    int psram_used  = psram_total - psram_free;

    ArrayOfMemoryUsageValue memory(2);

    MemoryUsageValue sram;
    sram.type("SRAM");
    sram.current_kb(sram_used);
    sram.total_kb(sram_total);
    memory.at(0, sram);

    MemoryUsageValue psram;
    psram.type("PSRAM");
    psram.current_kb(psram_used);
    psram.total_kb(psram_total);
    memory.at(1, psram);

    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_MEMORY_USAGE, memory);
        lvgl_port_unlock();
    }
    else {
        ESP_LOGE("stats", "Failed to lock LVGL mutex to update memory usage");
    }
}
