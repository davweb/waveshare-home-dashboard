#include "http_data_handlers.h"
#include "HtmlUtils.h"

#include <string.h>
#include <stdio.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include <eez-flow.h>
#include <vars.h>
#include <structs.h>

#include <atomic>

static const char *TAG = "DataHandlers";

extern std::atomic<bool> g_server_connected;

// ---------------------------------------------------------------------------
// GET /system  — HTML tables
// ---------------------------------------------------------------------------

static esp_err_t system_handler(httpd_req_t *req)
{
    // --- Live data (not held in EEZ globals) ---

    uint64_t uptime_s = esp_timer_get_time() / 1000000ULL;
    char uptime_str[24];
    snprintf(uptime_str, sizeof(uptime_str), "%uh %02um %02us",
             (unsigned)(uptime_s / 3600),
             (unsigned)((uptime_s % 3600) / 60),
             (unsigned)(uptime_s % 60));

    bool srv_conn = g_server_connected.load();

    // --- EEZ flow globals ---

    SystemInformationValue   sys_info(flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_SYSTEM_INFO));
    HardwareStatsValue       hw_stats(flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_HW_STATISTICS));
    NetworkInformationValue  net_info(flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_NETWORK_INFO));
    ArrayOfMemoryUsageValue  mem     (flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_MEMORY_USAGE));
    OtaInformationValue      ota_info(flow::getGlobalVariable(FLOW_GLOBAL_VARIABLE_OTA_INFO));

    SoftwareInformationValue software_info = sys_info.software_info();
    HardwareInformationValue hardware_info = sys_info.hardware_info();
    MemoryUsageValue         sram_usage    = mem.at(0);
    MemoryUsageValue         psram_usage   = mem.at(1);

    int sram_free_kb  = sram_usage.total_kb()  - sram_usage.current_kb();
    int psram_free_kb = psram_usage.total_kb() - psram_usage.current_kb();
    char sram_str[32], psram_str[32];
    snprintf(sram_str,  sizeof(sram_str),  "%d KB free / %d KB", sram_free_kb,  sram_usage.total_kb());
    snprintf(psram_str, sizeof(psram_str), "%d KB free / %d KB", psram_free_kb, psram_usage.total_kb());

    char rssi_str[96];
    int rssi = hw_stats.network_rssi();
    if (rssi != 0) {
        const char *quality = hw_stats.network_quality();
        http_badge_color_t quality_color;
        if      (strcmp(quality, "Excellent") == 0) quality_color = HTTP_BADGE_SUCCESS;
        else if (strcmp(quality, "Good")      == 0) quality_color = HTTP_BADGE_SUCCESS;
        else if (strcmp(quality, "Fair")      == 0) quality_color = HTTP_BADGE_WARNING;
        else                                        quality_color = HTTP_BADGE_DANGER;
        char badge[64];
        snprintf(rssi_str, sizeof(rssi_str), "%d dBm %s", rssi,
                 http_format_badge(badge, sizeof(badge), quality_color, quality));
    } else {
        strlcpy(rssi_str, "—", sizeof(rssi_str));
    }

    char flash_str[16], cores_str[8], freq_str[16], temp_str[16];
    snprintf(flash_str, sizeof(flash_str), "%d KB", hardware_info.flash_size_kb());
    snprintf(cores_str, sizeof(cores_str), "%d",    hardware_info.cpu_core_count());
    snprintf(freq_str,  sizeof(freq_str),  "%d MHz", hw_stats.frequency_mhz());
    snprintf(temp_str,  sizeof(temp_str),  "%d °C",  hw_stats.temperature_celsius());

    // --- Send HTML ---

    http_send_html_head(req, "System",
        "<div class=\"container-fluid py-3\">"
        "<h1 class=\"h3\">System Information</h1>");

    // Live
    http_send_section_open(req, "Status");
    http_send_row(req, "Uptime", uptime_str);
    {
        char badge[64];
        http_send_row(req, "Server", http_format_badge(badge, sizeof(badge),
            srv_conn ? HTTP_BADGE_SUCCESS : HTTP_BADGE_DANGER,
            srv_conn ? "Connected" : "Disconnected"));
    }
    http_send_section_close(req);

    // Software
    http_send_section_open(req, "Software");
    http_send_row(req, "App Version",    software_info.app_version());
    http_send_row(req, "Build Date",      software_info.build_date());
    http_send_row(req, "SDK Version",        software_info.sdk_version());
    http_send_row(req, "LVGL Version",       software_info.lvgl_version());
    http_send_row(req, "ESP Panel Version",  software_info.esp_panel_version());
    http_send_section_close(req);

    // Hardware
    http_send_section_open(req, "Hardware");
    http_send_row(req, "Board",             hardware_info.board());
    http_send_row(req, "CPU Model",         hardware_info.cpu_model());
    http_send_row(req, "CPU Core Count",       cores_str);
    http_send_row(req, "CPU Frequency",   freq_str);
    http_send_row(req, "CPU Temperature", temp_str);
    http_send_row(req, "SRAM",        sram_str);
    http_send_row(req, "PSRAM",       psram_str);
    http_send_row(req, "Flash",       flash_str);
    http_send_section_close(req);

    // Network
    http_send_section_open(req, "Network");
    http_send_row(req, "Network Name",    net_info.wireless_ssid());
    http_send_row(req, "Signal Strength",    rssi_str);
    http_send_row(req, "MAC Address",     net_info.mac_address());
    http_send_row(req, "IP Address",      net_info.ip_address());
    http_send_row(req, "Subnet Mask",  net_info.subnet_mask());
    http_send_row(req, "Default Gateway", net_info.default_gatway());
    http_send_section_close(req);

    // MQTT
    http_send_section_open(req, "MQTT");
    http_send_row(req, "Broker Address",  sys_info.mqtt().broker_address());
    http_send_row(req, "Broker Version", sys_info.mqtt().broker_version());
    http_send_row(req, "Topic Prefix",  sys_info.mqtt().topic_prefix());
    http_send_section_close(req);

    // Updates
    http_send_section_open(req, "Updates");
    ArrayOfPartitionInformationValue parts = sys_info.partitions();

    for (int i = 0; i < 2; i++) {
        PartitionInformationValue p = parts.at(i);
        char badge[64], val[128];
        if (p.current()) {
            snprintf(val, sizeof(val), "%s %s", p.app_version(),
                http_format_badge(badge, sizeof(badge), HTTP_BADGE_PRIMARY, "Running"));
        } else {
            snprintf(val, sizeof(val), "%s", p.app_version());
        }
        http_send_row(req, p.name(), val);
    }

    http_send_row(req, "Server Version", ota_info.server_version());
    http_send_section_close(req);

    http_send_html_foot(req);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_data_handlers(httpd_handle_t server)
{
    static const httpd_uri_t system_uri = {
        .uri      = "/system",
        .method   = HTTP_GET,
        .handler  = system_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server, &system_uri);

    ESP_LOGI(TAG, "Data handler registered: /system");
}
