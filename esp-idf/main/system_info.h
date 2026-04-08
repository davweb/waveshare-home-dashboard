#pragma once

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_log.h"
#include "driver/temperature_sensor.h"
#include <lvgl_v8_port.h>
#include <esp_panel_versions.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include <structs.h>
#include <vars.h>
#include <eez-flow.h>
#include <OtaUpdate.h>

#include "time_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace eez;
using namespace esp_panel::drivers;

extern Board *g_panel;

static const int SOC_SRAM_TOTAL_KB = 512;

static temperature_sensor_handle_t s_temp_sensor = nullptr;

inline void init_cpu_statistics() {
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    temperature_sensor_install(&cfg, &s_temp_sensor);
    temperature_sensor_enable(s_temp_sensor);
}

inline CPUStatsValue buildCPUStats() {
    CPUStatsValue stats;
    stats.frequency_mhz(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);

    if (s_temp_sensor) {
        float temp;
        if (temperature_sensor_get_celsius(s_temp_sensor, &temp) == ESP_OK) {
            stats.temperature_celsius((int)temp);
        }
    }

    return stats;
}

inline void set_system_information() {
    const esp_app_desc_t *desc = esp_app_get_description();

    struct tm build_tm = {};
    char build_combined[32];
    snprintf(build_combined, sizeof(build_combined), "%s %s", desc->date, desc->time);
    strptime(build_combined, "%b %d %Y %H:%M:%S", &build_tm);
    build_tm.tm_isdst = -1;
    char build_date[64];
    format_date_time(build_date, sizeof(build_date), mktime(&build_tm));

    char panel_version[16];
    snprintf(panel_version, sizeof(panel_version), "%d.%d.%d",
        ESP_PANEL_VERSION_MAJOR, ESP_PANEL_VERSION_MINOR, ESP_PANEL_VERSION_PATCH);

    char lvgl_version[16];
    snprintf(lvgl_version, sizeof(lvgl_version), "%d.%d.%d",
        LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    const char *chip_model_name;
    switch (chip_info.model) {
        case CHIP_ESP32:   chip_model_name = "ESP32";    break;
        case CHIP_ESP32S2: chip_model_name = "ESP32-S2"; break;
        case CHIP_ESP32S3: chip_model_name = "ESP32-S3"; break;
        case CHIP_ESP32C3: chip_model_name = "ESP32-C3"; break;
        default:           chip_model_name = "Unknown";  break;
    }
    char chip_model_str[24];
    snprintf(chip_model_str, sizeof(chip_model_str), "%s rev v%d.%d",
        chip_model_name, chip_info.revision / 100, chip_info.revision % 100);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    SoftwareInformationValue sw;
    sw.app_version(desc->version);
    sw.build_date(build_date);
    sw.sdk_version(esp_get_idf_version());
    sw.esp_panel_version(panel_version);
    sw.lvgl_version(lvgl_version);

    HardwareInformationValue hw;
    hw.board(g_panel->getConfig().name);
    hw.cpu_model(chip_model_str);
    hw.cpu_core_count(chip_info.cores);
    hw.sram_size_kb(SOC_SRAM_TOTAL_KB);
    hw.psram_size_kb((int)(esp_psram_get_size() / 1024));
    hw.flash_size_kb((int)(flash_size / 1024));

    const esp_partition_t *running_partition = esp_ota_get_running_partition();

    ArrayOfPartitionInformationValue partitions(2);
    int idx = 0;
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL && idx < 2) {
        const esp_partition_t *p = esp_partition_get(it);
        esp_app_desc_t p_desc;
        PartitionInformationValue entry;
        entry.name(p->label);
        entry.current(p == running_partition);
        entry.app_version(esp_ota_get_partition_description(p, &p_desc) == ESP_OK ? p_desc.version : "");
        partitions.at(idx++, entry);
        it = esp_partition_next(it);
    }
    if (it != NULL) esp_partition_iterator_release(it);

    SystemInformationValue info;
    info.software_info(sw);
    info.hardware_info(hw);
    info.timezone(CONFIG_CLOCK_POSIX_TZ);
    info.server_url(CONFIG_MQTT_BROKER_URL);
    info.partitions(partitions);

    flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SYSTEM_INFO, info);
}

inline ArrayOfMemoryUsageValue buildMemoryUsage() {
    int sram_free   = (int)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    int psram_total = (int)(esp_psram_get_size() / 1024);
    int psram_free  = (int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);

    ArrayOfMemoryUsageValue memory(2);

    MemoryUsageValue sram;
    sram.type("SRAM");
    sram.current_kb(SOC_SRAM_TOTAL_KB - sram_free);
    sram.total_kb(SOC_SRAM_TOTAL_KB);
    memory.at(0, sram);

    MemoryUsageValue psram;
    psram.type("PSRAM");
    psram.current_kb(psram_total - psram_free);
    psram.total_kb(psram_total);
    memory.at(1, psram);

    return memory;
}

inline void set_ota_information(const char *new_version) {
    OtaInformationValue ota;
    ota.server_version(new_version);

    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_OTA_INFO, ota);
        lvgl_port_unlock();
    }
    else {
        ESP_LOGE("system_info", "Failed to lock LVGL mutex to update OTA info");
    }
}
