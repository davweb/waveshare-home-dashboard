#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include <lvgl_v8_port.h>
#include <DisplayPanel.h>
#include <Wireless.h>
#include <ClockTools.h>
#include <HttpTools.h>
#include <ui.h>
#include <vars.h>
#include <structs.h>
#include <images.h>
#include <screens.h>

static const char *TAG = "main";

using namespace esp_panel::drivers;

char bus_routes[2][3][4];
char bus_destinations[2][3][32];
char bus_due_times[2][3][8];

void fetchData() {
    ESP_LOGD(TAG, "fetch data");
    JsonDocument doc;

    if (!getJsonFromUrl(doc, "http://192.168.1.17")) {
        ESP_LOGW(TAG, "Failed to get data from dashboard server.");

        if (lvgl_port_lock(portMAX_DELAY)) {
            flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SERVER_CONNECTED, Value(false));
            lvgl_port_unlock();
        }
        else {
            ESP_LOGE(TAG, "Failed to lock LVGL mutex to update server connection status");
        }

        return;
    }

    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 3; i++) {
            strlcpy(bus_routes[j][i], doc["bus_stops"][j]["buses"][i]["route"] | "", sizeof(bus_routes[j][i]));
            strlcpy(bus_destinations[j][i], doc["bus_stops"][j]["buses"][i]["destination"] | "", sizeof(bus_destinations[j][i]));
            strlcpy(bus_due_times[j][i], doc["bus_stops"][j]["buses"][i]["due"] | "", sizeof(bus_due_times[j][i]));
        }
    }

    //Lock the LVGL mutex
    if (lvgl_port_lock(portMAX_DELAY)) {

        for (int j = 0; j < 2; j++) {
            ArrayOfString eez_routes(3);
            ArrayOfString eez_destinations(3);
            ArrayOfString eez_due_times(3);

            for (int i = 0; i < 3; i++) {
                eez_routes.at(i, bus_routes[j][i]);
                eez_destinations.at(i, bus_destinations[j][i]);
                eez_due_times.at(i, bus_due_times[j][i]);
            }

            flow::setGlobalVariable(j == 0 ? FLOW_GLOBAL_VARIABLE_BUS_STOP_1_NAME : FLOW_GLOBAL_VARIABLE_BUS_STOP_2_NAME, StringValue(doc["bus_stops"][j]["name"] | ""));
            flow::setGlobalVariable(j == 0 ? FLOW_GLOBAL_VARIABLE_BUS_STOP_1_ROUTES : FLOW_GLOBAL_VARIABLE_BUS_STOP_2_ROUTES, eez_routes);
            flow::setGlobalVariable(j == 0 ? FLOW_GLOBAL_VARIABLE_BUS_STOP_1_DESTINATIONS : FLOW_GLOBAL_VARIABLE_BUS_STOP_2_DESTINATIONS, eez_destinations);
            flow::setGlobalVariable(j == 0 ? FLOW_GLOBAL_VARIABLE_BUS_STOP_1_TIMES : FLOW_GLOBAL_VARIABLE_BUS_STOP_2_TIMES, eez_due_times);
        }

        // Use StringValue rather than Value directly to ensure the string is copied into the global variable rather than just referencing the temporary string in the JsonDocument
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_TEMPERATURE, StringValue(doc["weather"]["temperature"] | ""));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_FEELS_LIKE, StringValue(doc["weather"]["feels_like"] | ""));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RAIN_CHANCE, StringValue(doc["weather"]["rain"] | ""));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING_DATE, StringValue(doc["recycling"]["date"] | ""));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING_SHORT_DATE, StringValue(doc["recycling"]["short_date"] | ""));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING_TYPE, StringValue(doc["recycling"]["type"] | ""));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SUN_TYPE, StringValue(doc["weather"]["sun"]["event"] | ""));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SUN_TIME, StringValue(doc["weather"]["sun"]["time"] | ""));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SERVER_CONNECTED, Value(true));

        lvgl_port_unlock();
    }
    else {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex to update data");
        return;
    }
}

const char *get_var_time() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    static char time_str[10];
    snprintf(time_str, sizeof(time_str), "%d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    return time_str;
}

void set_var_time(const char *value) {
    // Do nothing
}

const char *get_var_date() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    static char date_str[64];
    char weekday[24];
    char month[24];

    strftime(weekday, sizeof(weekday), "%A", &timeinfo);
    strftime(month, sizeof(month), "%B", &timeinfo);

    // We have to use snprintf to get the day of the month without leading zeros or leading spaces
    snprintf(date_str, sizeof(date_str), "%s %d %s", weekday, timeinfo.tm_mday, month);
    return date_str;
}

void set_var_date(const char *value) {
    // Do nothing
}

extern "C" void app_main(void)
{
    // Start WiFi synchronously so we don't have DMA contention with the initial UI setup

    Board *board = initialiseDisplayPanel();

    ESP_LOGD(TAG, "Initializing LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    ESP_LOGD(TAG, "Creating UI");

    while (!lvgl_port_lock(portMAX_DELAY)) {
        ESP_LOGW(TAG, "Failed to lock LVGL mutex, retrying...");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ui_init();
    lvgl_port_set_ui_tick_cb(ui_tick);
    lvgl_port_unlock();

    static bool prevWifiConnected = false;
    static uint64_t fetchInterval = 30000; // Fetch data every 30 seconds
    static uint64_t lastFetchTime = 0;

    startWiFi();

    while (true) {
        uint64_t currentTime = esp_timer_get_time() / 1000ULL;

        if (!isWiFiConnected()) {
            //If WiFi was connected but now isn't, update the global variable
            if (prevWifiConnected) {
                ESP_LOGW(TAG, "WiFi Disconnected");

                if (lvgl_port_lock(portMAX_DELAY)) {
                    flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_CONNECTED, Value(false));
                    lvgl_port_unlock();
                    prevWifiConnected = false;
                }
            }
        }
        else {
            if (!prevWifiConnected) {
                ESP_LOGI(TAG, "WiFi Connected");

                if (lvgl_port_lock(portMAX_DELAY)) {
                    flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_CONNECTED, Value(true));
                    lvgl_port_unlock();
                    prevWifiConnected = true;
                }

                setRtcClock();
            }

            if (lastFetchTime == 0 || (currentTime - lastFetchTime >= fetchInterval)) {
                lastFetchTime = currentTime;
                fetchData();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
