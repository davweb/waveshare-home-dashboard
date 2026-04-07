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
#include <HttpServer.h>
#include <OtaUpdate.h>
#include <ui.h>
#include <vars.h>
#include <structs.h>
#include <images.h>
#include <screens.h>
#include <actions.h>
#include "time_utils.h"
#include "user_actions.h"
#include "global_vars.h"
#include "data_fetcher.h"
#include "system_info.h"

static const char *TAG = "main";

using namespace esp_panel::drivers;

// ---------------------------------------------------------------------------
// OTA hooks — implement bodies here to wire events into EEZ Flow later
// ---------------------------------------------------------------------------

static void on_ota_start(const char *new_version)
{
    ESP_LOGI(TAG, "OTA update starting: new version %s", new_version);

    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_FIRMWARE_PROGRESS, Value(0));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_STATE, SystemState_FIRMWARE_UPDATE);
        lvgl_port_unlock();
    }
    else {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex to show firmware update screen");
    }
}

static void on_ota_progress(int percent)
{
    ESP_LOGI(TAG, "OTA progress: %d%%", percent);
    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_FIRMWARE_PROGRESS, Value(percent));
        lvgl_port_unlock();
    }
}

static void on_ota_complete(bool success, const char *message)
{
    if (success) {
        ESP_LOGI(TAG, "OTA complete (%s) — rebooting", message);
    } else {
        ESP_LOGE(TAG, "OTA failed: %s", message);
    }

    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_STATE, SystemState_RUNNING);
        lvgl_port_unlock();
    }
    else {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex to dismiss firmware update screen");
    }
}

static void on_ota_checked(OtaLastCheck result)
{
    set_ota_information(result);
}

static BusData g_bus_data;
Board *g_panel;

static void recalculateDueTimes() {
    // Static so the variables are not destroyed when function exits, as they are referenced by the bus stop global variable
    static char bus_due_labels[2][3][8];
    static int bus_due_seconds[2][3];

    time_t now = time(nullptr);
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 3; i++) {
            time_t epoch = g_bus_data.stops[j].arrivals[i].due_epoch;
            bus_due_seconds[j][i] = epoch > 0 ? (int)(epoch - now) : 0;
            if (g_bus_data.stops[j].arrivals[i].route[0] == '\0') {
                bus_due_labels[j][i][0] = '\0';
            } else {
                format_due_label(bus_due_labels[j][i], sizeof(bus_due_labels[j][i]), epoch, now);
            }
        }
    }

    ArrayOfBusStopValue bus_stops(2);

    for (int j = 0; j < 2; j++) {
        ArrayOfBusArrivalValue arrivals(3);
        for (int i = 0; i < 3; i++) {
            BusArrivalValue arrival;
            arrival.route(g_bus_data.stops[j].arrivals[i].route);
            arrival.destination(g_bus_data.stops[j].arrivals[i].destination);
            arrival.due_label(bus_due_labels[j][i]);
            arrival.due_seconds(bus_due_seconds[j][i]);
            arrivals.at(i, arrival);
        }

        BusStopValue bus_stop;
        bus_stop.name(g_bus_data.stops[j].name);
        bus_stop.arrivals(arrivals);
        bus_stops.at(j, bus_stop);
    }

    CPUStatsValue cpu_stats = buildCPUStats();

    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_BUS_STOPS, bus_stops);
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_CPU_STATISTICS, cpu_stats);
        lvgl_port_unlock();
    }
    else {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex to recalculate due times");
    }
}

static WeatherType icon_string_to_weather_type(const char *icon) {
    if (strcmp(icon, "clear-day") == 0)          return WeatherType_CLEAR_DAY;
    if (strcmp(icon, "clear-night") == 0)         return WeatherType_CLEAR_NIGHT;
    if (strcmp(icon, "thunderstorm") == 0)        return WeatherType_THUNDERSTORM;
    if (strcmp(icon, "rain") == 0)                return WeatherType_RAIN;
    if (strcmp(icon, "snow") == 0)                return WeatherType_SNOW;
    if (strcmp(icon, "sleet") == 0)               return WeatherType_SLEET;
    if (strcmp(icon, "wind") == 0)                return WeatherType_WIND;
    if (strcmp(icon, "fog") == 0)                 return WeatherType_FOG;
    if (strcmp(icon, "cloudy") == 0)              return WeatherType_CLOUDY;
    if (strcmp(icon, "partly-cloudy-day") == 0)   return WeatherType_PARTLY_CLOUDY_DAY;
    if (strcmp(icon, "partly-cloudy-night") == 0) return WeatherType_PARTLY_CLOUDY_NIGHT;
    if (strcmp(icon, "hail") == 0)                return WeatherType_HAIL;
    return WeatherType_NONE;
}

static void fetchData() {
    ESP_LOGD(TAG, "fetch data");

    static SunData sun_data;
    static WeatherData weather_data;
    static RecyclingData recycling_data;
    static PresenceData presence_data;

    if (!fetch_dashboard_data(g_bus_data, sun_data, weather_data, recycling_data, presence_data)) {
        ESP_LOGW(TAG, "Failed to get data from dashboard server.");

        if (lvgl_port_lock(portMAX_DELAY)) {
            flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_STATE, SystemState_NO_DATA);
            lvgl_port_unlock();
        }
        else {
            ESP_LOGE(TAG, "Failed to lock LVGL mutex to update server connection status");
        }

        return;
    }

    recalculateDueTimes();

    WeatherValue weather;

    WeatherCurrentValue current;
    current.temperature(weather_data.current_temperature);
    current.feels_like(weather_data.current_feels_like);
    current.icon(icon_string_to_weather_type(weather_data.current_icon));
    weather.current(current);

    WeatherDayValue day;
    day.min_temperature(weather_data.day_min_temperature);
    day.max_temperature(weather_data.day_max_temperature);
    day.precip_chance(weather_data.day_precip_chance);
    day.precip_type(weather_data.day_precip_type);
    weather.day(day);

    ArrayOfWeatherHourValue hours(weather_data.num_hours);
    for (int i = 0; i < weather_data.num_hours; i++) {
        WeatherHourValue hour_val;
        hour_val.hour(weather_data.hours[i].hour);
        hour_val.icon(icon_string_to_weather_type(weather_data.hours[i].icon));
        hour_val.temperature(weather_data.hours[i].temperature);
        hour_val.feels_like(weather_data.hours[i].feels_like);
        hour_val.precip_chance(weather_data.hours[i].precip_chance);
        hour_val.wind_speed(weather_data.hours[i].wind_speed);
        hour_val.uv_index(weather_data.hours[i].uv_index);
        hours.at(i, hour_val);
    }
    weather.hours(hours);

    ArrayOfRecyclingValue recycling(recycling_data.num_collections);
    for (int i = 0; i < recycling_data.num_collections; i++) {
        RecyclingValue item;
        item.type(strcmp(recycling_data.items[i].type, "Recycling") == 0
            ? RecyclingType_RECYCLING
            : RecyclingType_GENERAL_WASTE);
        item.date(recycling_data.items[i].date);
        item.short_date(recycling_data.items[i].short_date);
        item.lead_time(recycling_data.items[i].lead_time);
        recycling.at(i, item);
    }

    SunTimeValue sun;
    sun.is_sunrise(sun_data.is_sunrise);
    sun.time(sun_data.time);

    ArrayOfPresenceValue presence(presence_data.num_people);
    for (int i = 0; i < presence_data.num_people; i++) {
        PresenceValue item;
        item.name(presence_data.items[i].name);
        item.connected(presence_data.items[i].connected);
        item.last_seen(presence_data.items[i].last_seen);
        presence.at(i, item);
    }

    ArrayOfMemoryUsageValue memory = buildMemoryUsage();

    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WEATHER, weather);
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SUN, sun);
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING, recycling);
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_PRESENCE, presence);
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_MEMORY_USAGE, memory);
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_STATE, SystemState_RUNNING);
        lvgl_port_unlock();
    }
    else {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex to update data");
        return;
    }
}


extern "C" void app_main(void)
{
    g_panel = initialiseDisplayPanel();

    ESP_LOGD(TAG, "Initializing LVGL");
    lvgl_port_init(g_panel->getLCD(), g_panel->getTouch());

    ESP_LOGD(TAG, "Creating UI");

    while (!lvgl_port_lock(portMAX_DELAY)) {
        ESP_LOGW(TAG, "Failed to lock LVGL mutex, retrying...");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    //Start wifi before LVGL so SRAM is available
    startWiFi();
    startHttpServer();

    ota_set_callbacks(on_ota_start, on_ota_progress, on_ota_complete, on_ota_checked);

    ui_init();
    lvgl_port_set_ui_tick_cb(ui_tick);
    set_system_information();
    lvgl_port_unlock();
    init_cpu_statistics();

    static bool prevWifiConnected = false;
    static uint64_t fetchInterval = 30000; // Fetch data every 30 seconds
    static uint64_t lastFetchTime = 0;
    static uint64_t recalculateInterval = 5000; // Recalculate due times every 5 seconds
    static uint64_t lastRecalculateTime = 0;
    static uint64_t otaCheckInterval = 30 * 60 * 1000; // Check for OTA updates every 30 minutes
    static uint64_t lastOtaCheckTime  = 0;

    while (true) {
        uint64_t currentTime = esp_timer_get_time() / 1000ULL;

        if (!isWiFiConnected()) {
            //If WiFi was connected but now isn't, update the global variable
            if (prevWifiConnected) {
                ESP_LOGW(TAG, "WiFi Disconnected");

                if (lvgl_port_lock(portMAX_DELAY)) {
                    flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_STATE, SystemState_NO_WIFI);
                    lvgl_port_unlock();
                    prevWifiConnected = false;
                }
            }
        }
        else {
            if (!prevWifiConnected) {
                ESP_LOGI(TAG, "WiFi Connected");

                if (lvgl_port_lock(portMAX_DELAY)) {
                    flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_STATE, SystemState_NO_DATA);
                    lvgl_port_unlock();
                    prevWifiConnected = true;
                }

                setRtcClock();
            }

            if (lastFetchTime == 0 || (currentTime - lastFetchTime >= fetchInterval)) {
                lastFetchTime = currentTime;
                lastRecalculateTime = currentTime;
                fetchData();
            }
            else if (currentTime - lastRecalculateTime >= recalculateInterval) {
                lastRecalculateTime = currentTime;
                recalculateDueTimes();
            }

            // OTA check — runs on first WiFi connect, then every 30 minutes
            if (lastOtaCheckTime == 0 || (currentTime - lastOtaCheckTime >= otaCheckInterval)) {
                lastOtaCheckTime = currentTime;
                ota_check_and_update(CONFIG_DASHBOARD_SERVER_URL);
                // If an update was applied the device reboots inside ota_check_and_update;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
