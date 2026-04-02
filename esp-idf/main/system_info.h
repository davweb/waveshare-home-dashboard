#pragma once

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include <esp_panel_versions.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include <structs.h>
#include <vars.h>
#include <eez-flow.h>

#include <stdio.h>
#include <stdlib.h>

using namespace eez;
using namespace esp_panel::drivers;

extern Board *g_panel;

inline void set_system_information() {
    const esp_app_desc_t *desc = esp_app_get_description();

    char build_date[32];
    snprintf(build_date, sizeof(build_date), "%s %s", desc->date, desc->time);

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

    SystemInformationValue info;
    info.app_version(desc->version);
    info.build_date(build_date);
    info.sdk_version(esp_get_idf_version());
    info.esp_panel_version(panel_version);
    info.lvgl_version(lvgl_version);
    info.board(g_panel->getConfig().name);
    info.chip_model(chip_model_str);
    info.timezone(CONFIG_CLOCK_POSIX_TZ);

    flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SYSTEM_INFO, info);
}
