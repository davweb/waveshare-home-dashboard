#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include <lvgl_v8_port.h>
#include <DisplayPanel.h>
#include <Wireless.h>
#include <ClockTools.h>
#include <HttpServer.h>
#include <OtaUpdate.h>
#include <MqttTools.h>
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
// OTA hooks
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

// ---------------------------------------------------------------------------
// Per-type dashboard data — written by MQTT callback, bus data also read
// by recalculateDueTimes() in the main loop (hence the mutex).
// ---------------------------------------------------------------------------

Board *g_panel;

static BusData      g_bus_data      = {};
static SunData      g_sun_data      = {};
static WeatherData  g_weather_data  = {};
static RecyclingData g_recycling_data = {};
static PresenceData  g_presence_data  = {};

// Protects g_bus_data between the MQTT task and the main loop.
static SemaphoreHandle_t s_bus_mutex = nullptr;

// Set by the MQTT callback when new bus data arrives so the main loop
// recalculates immediately rather than waiting for the next 5-second tick.
static volatile bool s_bus_updated = false;

// ---------------------------------------------------------------------------
// Bus due-time recalculation — only called from the main loop task.
// ---------------------------------------------------------------------------

static void recalculateDueTimes()
{
    // Static so labels remain valid while referenced by the EEZ value.
    static char bus_due_labels[2][3][8];
    static int  bus_due_seconds[2][3];

    // Snapshot under mutex so the MQTT task can safely update g_bus_data.
    BusData bus;
    xSemaphoreTake(s_bus_mutex, portMAX_DELAY);
    bus = g_bus_data;
    xSemaphoreGive(s_bus_mutex);

    time_t now = time(nullptr);
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 3; i++) {
            time_t epoch = bus.stops[j].arrivals[i].due_epoch;
            bus_due_seconds[j][i] = epoch > 0 ? (int)(epoch - now) : 0;
            if (bus.stops[j].arrivals[i].route[0] == '\0') {
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
            arrival.route(bus.stops[j].arrivals[i].route);
            arrival.destination(bus.stops[j].arrivals[i].destination);
            arrival.due_label(bus_due_labels[j][i]);
            arrival.due_seconds(bus_due_seconds[j][i]);
            arrivals.at(i, arrival);
        }
        BusStopValue bus_stop;
        bus_stop.name(bus.stops[j].name);
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

// ---------------------------------------------------------------------------
// Weather icon helper
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// MQTT message callback — runs on the MQTT client task
// ---------------------------------------------------------------------------

static void onMqttMessage(MqttTopic topic, cJSON *json)
{
    switch (topic) {

        case MqttTopic::BUS_STOPS: {
            BusData new_bus = {};
            parse_bus_stops(json, new_bus);
            xSemaphoreTake(s_bus_mutex, portMAX_DELAY);
            g_bus_data = new_bus;
            xSemaphoreGive(s_bus_mutex);
            s_bus_updated = true; // trigger immediate recalculate in main loop
            break;
        }

        case MqttTopic::WEATHER: {
            parse_weather(json, g_sun_data, g_weather_data);

            WeatherValue weather;

            WeatherCurrentValue current;
            current.temperature(g_weather_data.current_temperature);
            current.feels_like(g_weather_data.current_feels_like);
            current.icon(icon_string_to_weather_type(g_weather_data.current_icon));
            weather.current(current);

            WeatherDayValue day;
            day.min_temperature(g_weather_data.day_min_temperature);
            day.max_temperature(g_weather_data.day_max_temperature);
            day.precip_chance(g_weather_data.day_precip_chance);
            day.precip_type(g_weather_data.day_precip_type);
            weather.day(day);

            ArrayOfWeatherHourValue hours(g_weather_data.num_hours);
            for (int i = 0; i < g_weather_data.num_hours; i++) {
                WeatherHourValue hour_val;
                hour_val.hour(g_weather_data.hours[i].hour);
                hour_val.icon(icon_string_to_weather_type(g_weather_data.hours[i].icon));
                hour_val.temperature(g_weather_data.hours[i].temperature);
                hour_val.feels_like(g_weather_data.hours[i].feels_like);
                hour_val.precip_chance(g_weather_data.hours[i].precip_chance);
                hour_val.wind_speed(g_weather_data.hours[i].wind_speed);
                hour_val.uv_index(g_weather_data.hours[i].uv_index);
                hours.at(i, hour_val);
            }
            weather.hours(hours);

            SunTimeValue sun;
            sun.is_sunrise(g_sun_data.is_sunrise);
            sun.time(g_sun_data.time);

            if (lvgl_port_lock(portMAX_DELAY)) {
                flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WEATHER, weather);
                flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SUN, sun);
                lvgl_port_unlock();
            }
            else {
                ESP_LOGE(TAG, "Failed to lock LVGL mutex to update weather");
            }
            break;
        }

        case MqttTopic::RECYCLING: {
            parse_recycling(json, g_recycling_data);

            ArrayOfRecyclingValue recycling(g_recycling_data.num_collections);
            for (int i = 0; i < g_recycling_data.num_collections; i++) {
                RecyclingValue item;
                item.type(strcmp(g_recycling_data.items[i].type, "Recycling") == 0
                    ? RecyclingType_RECYCLING
                    : RecyclingType_GENERAL_WASTE);
                item.date(g_recycling_data.items[i].date);
                item.short_date(g_recycling_data.items[i].short_date);
                item.lead_time(g_recycling_data.items[i].lead_time);
                recycling.at(i, item);
            }

            if (lvgl_port_lock(portMAX_DELAY)) {
                flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING, recycling);
                lvgl_port_unlock();
            }
            else {
                ESP_LOGE(TAG, "Failed to lock LVGL mutex to update recycling");
            }
            break;
        }

        case MqttTopic::PRESENCE: {
            parse_presence(json, g_presence_data);

            ArrayOfPresenceValue presence(g_presence_data.num_people);
            for (int i = 0; i < g_presence_data.num_people; i++) {
                PresenceValue item;
                item.name(g_presence_data.items[i].name);
                item.connected(g_presence_data.items[i].connected);
                item.last_seen(g_presence_data.items[i].last_seen);
                presence.at(i, item);
            }

            if (lvgl_port_lock(portMAX_DELAY)) {
                flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_PRESENCE, presence);
                lvgl_port_unlock();
            }
            else {
                ESP_LOGE(TAG, "Failed to lock LVGL mutex to update presence");
            }
            break;
        }

        case MqttTopic::SYS_BROKER_VERSION: {
            // json is a cJSON string node wrapping the plain-text payload
            if (cJSON_IsString(json) && json->valuestring) {
                ESP_LOGI(TAG, "MQTT broker version: %s", json->valuestring);
                update_mqtt_broker_version(json->valuestring);
            }
            break;
        }

        case MqttTopic::OTA: {
            const char *version = cjson_str(json, "version");
            const char *url     = cjson_str(json, "url");

            set_ota_information(version);

            if (version[0] != '\0' && url[0] != '\0') {
                ota_apply_mqtt_update(version, url);
            } else {
                ESP_LOGW(TAG, "OTA payload missing version or url");
            }
            break;
        }

        case MqttTopic::SERVER: {
            cJSON *connected_item = cJSON_GetObjectItem(json, "connected");
            bool is_connected = connected_item && cJSON_IsTrue(connected_item);

            if (lvgl_port_lock(portMAX_DELAY)) {
                flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_STATE,
                    is_connected ? SystemState_RUNNING : SystemState_NO_DATA);
                lvgl_port_unlock();
            } else {
                ESP_LOGE(TAG, "Failed to lock LVGL mutex to update server state");
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// app_main
// ---------------------------------------------------------------------------

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

    ota_set_callbacks(on_ota_start, on_ota_progress, on_ota_complete);

    ui_init();
    lvgl_port_set_ui_tick_cb(ui_tick);
    set_system_information();
    lvgl_port_unlock();
    init_cpu_statistics();

    s_bus_mutex = xSemaphoreCreateMutex();

    mqtt_init(onMqttMessage);

    static bool prevWifiConnected = false;
    static uint64_t recalculateInterval = 5000;
    static uint64_t lastRecalculateTime = 0;
    static uint64_t statsInterval = 5000;
    static uint64_t lastStatsTime = 0;

    while (true) {
        uint64_t currentTime = esp_timer_get_time() / 1000ULL;

        if (!isWiFiConnected()) {
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

            if (s_bus_updated || (currentTime - lastRecalculateTime >= recalculateInterval)) {
                s_bus_updated = false;
                lastRecalculateTime = currentTime;
                recalculateDueTimes();
            }

            if (lastStatsTime == 0 || (currentTime - lastStatsTime >= statsInterval)) {
                lastStatsTime = currentTime;
                ArrayOfMemoryUsageValue memory = buildMemoryUsage();
                CPUStatsValue cpu_stats = buildCPUStats();
                if (lvgl_port_lock(portMAX_DELAY)) {
                    flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_MEMORY_USAGE, memory);
                    flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_CPU_STATISTICS, cpu_stats);
                    lvgl_port_unlock();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
