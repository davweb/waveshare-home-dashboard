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
#include <actions.h>
#include "time_utils.h"
#include "user_actions.h"
#include "global_vars.h"

static const char *TAG = "main";

using namespace esp_panel::drivers;

char bus_stop_names[2][32];
char bus_routes[2][3][4];
char bus_destinations[2][3][32];
time_t bus_due_epochs[2][3];

static void recalculateDueTimes() {
    // Static so the variables are not destroyed when function exits, as they are referenced by the bus stop global variable
    static char bus_due_labels[2][3][8];
    static int bus_due_seconds[2][3];

    time_t now = time(nullptr);
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 3; i++) {
            time_t epoch = bus_due_epochs[j][i];
            bus_due_seconds[j][i] = epoch > 0 ? (int)(epoch - now) : 0;
            if (bus_routes[j][i][0] == '\0') {
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
            arrival.route(bus_routes[j][i]);
            arrival.destination(bus_destinations[j][i]);
            arrival.due_label(bus_due_labels[j][i]);
            arrival.due_seconds(bus_due_seconds[j][i]);
            arrivals.at(i, arrival);
        }

        BusStopValue bus_stop;
        bus_stop.name(bus_stop_names[j]);
        bus_stop.arrivals(arrivals);
        bus_stops.at(j, bus_stop);
    }

    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_BUS_STOPS, bus_stops);
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
    JsonDocument doc;

    if (!getJsonFromUrl(doc, CONFIG_DASHBOARD_SERVER_URL)) {
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
        strlcpy(bus_stop_names[j], doc["bus_stops"][j]["name"] | "", sizeof(bus_stop_names[j]));
        for (int i = 0; i < 3; i++) {
            strlcpy(bus_routes[j][i], doc["bus_stops"][j]["buses"][i]["route"] | "", sizeof(bus_routes[j][i]));
            strlcpy(bus_destinations[j][i], doc["bus_stops"][j]["buses"][i]["destination"] | "", sizeof(bus_destinations[j][i]));
            bus_due_epochs[j][i] = doc["bus_stops"][j]["buses"][i]["due_time"] | 0L;
        }
    }

    recalculateDueTimes();

    WeatherValue weather;

    WeatherCurrentValue current;
    current.temperature(doc["weather"]["current"]["temperature"] | 0);
    current.feels_like(doc["weather"]["current"]["feels_like"] | 0);
    current.rain_chance(doc["weather"]["current"]["rain_chance"] | 0);
    current.icon(icon_string_to_weather_type(doc["weather"]["current"]["icon"] | ""));
    weather.current(current);

    WeatherDayValue day;
    day.min_temperature(doc["weather"]["day"]["min_temperature"] | 0);
    day.max_temperature(doc["weather"]["day"]["max_temperature"] | 0);
    weather.day(day);

    int num_hours = doc["weather"]["hours"].size();
    if (num_hours > 24) num_hours = 24;
    ArrayOfWeatherHourValue hours(num_hours);
    for (int i = 0; i < num_hours; i++) {
        WeatherHourValue hour_val;
        hour_val.hour(doc["weather"]["hours"][i]["hour"] | 0);
        hour_val.icon(icon_string_to_weather_type(doc["weather"]["hours"][i]["icon"] | ""));
        hour_val.temperature(doc["weather"]["hours"][i]["temperature"] | 0);
        hour_val.feels_like(doc["weather"]["hours"][i]["feels_like"] | 0);
        hour_val.rain_chance(doc["weather"]["hours"][i]["rain_chance"] | 0);
        hour_val.wind_speed(doc["weather"]["hours"][i]["wind_speed"] | 0);
        hour_val.uv_index(doc["weather"]["hours"][i]["uv_index"] | 0);
        hours.at(i, hour_val);
    }
    weather.hours(hours);

    RecyclingValue recycling;
    recycling.type(doc["recycling"]["type"] | "");

    time_t recycling_epoch = doc["recycling"]["date_epoch"] | 0L;
    char recycling_date_str[64] = "";
    char recycling_short_date_str[16] = "";

    if (recycling_epoch > 0) {
        struct tm recycling_tm;
        localtime_r(&recycling_epoch, &recycling_tm);
        format_long_date(recycling_date_str, sizeof(recycling_date_str), &recycling_tm);

        time_t now = time(nullptr);
        format_short_date(recycling_short_date_str, sizeof(recycling_short_date_str), recycling_epoch, now);
    }

    recycling.date(recycling_date_str);
    recycling.short_date(recycling_short_date_str);

    SunTimeValue sun;
    sun.type(doc["weather"]["sun"]["event"] | "");
    sun.time(doc["weather"]["sun"]["time"] | "");

    //Lock the LVGL mutex
    if (lvgl_port_lock(portMAX_DELAY)) {
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WEATHER, weather);
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SUN, sun);
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING, recycling);
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SERVER_CONNECTED, Value(true));

        lvgl_port_unlock();
    }
    else {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex to update data");
        return;
    }
}

extern "C" void app_main(void)
{
    Board *board = initialiseDisplayPanel();

    ESP_LOGD(TAG, "Initializing LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    ESP_LOGD(TAG, "Creating UI");

    while (!lvgl_port_lock(portMAX_DELAY)) {
        ESP_LOGW(TAG, "Failed to lock LVGL mutex, retrying...");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    //Start wifi before LVGL so SRAM is available
    startWiFi();

    ui_init();
    lvgl_port_set_ui_tick_cb(ui_tick);
    lvgl_port_unlock();

    static bool prevWifiConnected = false;
    static uint64_t fetchInterval = 30000; // Fetch data every 30 seconds
    static uint64_t lastFetchTime = 0;
    static uint64_t recalculateInterval = 5000; // Recalculate due times every 5 seconds
    static uint64_t lastRecalculateTime = 0;

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
                lastRecalculateTime = currentTime;
                fetchData();
            }
            else if (currentTime - lastRecalculateTime >= recalculateInterval) {
                lastRecalculateTime = currentTime;
                recalculateDueTimes();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
