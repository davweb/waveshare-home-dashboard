#pragma once

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
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

inline void set_system_information() {
    const esp_app_desc_t *desc = esp_app_get_description();

    struct tm build_tm = {};
    char build_combined[32];
    snprintf(build_combined, sizeof(build_combined), "%s %s", desc->date, desc->time);
    strptime(build_combined, "%b %d %Y %H:%M:%S", &build_tm);
    build_tm.tm_isdst = -1;
    char build_date[32];
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
    info.app_version(desc->version);
    info.build_date(build_date);
    info.sdk_version(esp_get_idf_version());
    info.esp_panel_version(panel_version);
    info.lvgl_version(lvgl_version);
    info.board(g_panel->getConfig().name);
    info.chip_model(chip_model_str);
    info.timezone(CONFIG_CLOCK_POSIX_TZ);
    info.server_url(CONFIG_DASHBOARD_SERVER_URL);
    info.partitions(partitions);

    flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SYSTEM_INFO, info);
}

inline void set_ota_information(OtaLastCheck last) {

    char last_check_str[64];
    format_date_time(last_check_str, sizeof(last_check_str), last.check_time);

    OtaInformationValue ota;
    ota.server_version(last.server_version);
    ota.ota_last_check(last_check_str);

    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_OTA_INFO, ota);
        lvgl_port_unlock();
    }
    else {
        ESP_LOGE("system_info", "Failed to lock LVGL mutex to update OTA info");
    }
}
