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

static const char *TAG = "main";

void action_temperature_to_color(lv_event_t *e) {
    int temp = flow::getUserProperty(ACTION_TEMPERATURE_TO_COLOR_PROPERTY_TEMPERATURE).getInt();

    int color;
    if      (temp <  0) color = 0x7EC8E3;
    else if (temp <  5) color = 0x6B9FE4;
    else if (temp < 10) color = 0x3DBFB8;
    else if (temp < 15) color = 0x5DD96B;
    else if (temp < 20) color = 0xC8D44A;
    else if (temp < 25) color = 0xF5A623;
    else if (temp < 30) color = 0xF46B1A;
    else                color = 0xFF3B3B;

    flow::setUserProperty(ACTION_TEMPERATURE_TO_COLOR_PROPERTY_COLOR, IntegerValue(color));

    ESP_LOGD(TAG, "Converted temperature %d to color 0x%06X", temp, color);
}

void action_rain_chance_to_color(lv_event_t *e) {
    int chance = flow::getUserProperty(ACTION_RAIN_CHANCE_TO_COLOR_PROPERTY_RAIN_CHANCE).getInt();

    int color;
    if      (chance <  10) color = 0xDDEEFF;
    else if (chance <  20) color = 0xBBDDFF;
    else if (chance <  30) color = 0x99CCFF;
    else if (chance <  40) color = 0x77BBFF;
    else if (chance <  50) color = 0x55AAFF;
    else if (chance <  60) color = 0x3399EE;
    else if (chance <  70) color = 0x1188DD;
    else if (chance <  80) color = 0x0077CC;
    else if (chance <  90) color = 0x0055AA;
    else                   color = 0x003388;

    flow::setUserProperty(ACTION_RAIN_CHANCE_TO_COLOR_PROPERTY_COLOR, IntegerValue(color));

    ESP_LOGD(TAG, "Converted rain chance %d%% to color 0x%06X", chance, color);
}

using namespace esp_panel::drivers;

char bus_stop_names[2][32];
char bus_routes[2][3][4];
char bus_destinations[2][3][32];
char bus_due_labels[2][3][8];
int bus_due_seconds[2][3];
time_t bus_due_epochs[2][3];




static void recalculateDueTimes() {
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

    //Lock the LVGL mutex
    if (lvgl_port_lock(portMAX_DELAY)) {

        WeatherValue weather;

        WeatherDayValue day;
        day.temperature(doc["weather"]["day"]["temperature"] | 0);
        day.feels_like(doc["weather"]["day"]["feels_like"] | 0);
        day.rain_chance(doc["weather"]["day"]["rain_chance"] | 0);
        day.icon(icon_string_to_weather_type(doc["weather"]["day"]["icon"] | ""));
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

        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WEATHER, weather);
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
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING, recycling);
        SunTimeValue sun;
        sun.type(doc["weather"]["sun"]["event"] | "");
        sun.time(doc["weather"]["sun"]["time"] | "");
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SUN, sun);
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
    format_long_date(date_str, sizeof(date_str), &timeinfo);
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
    static uint64_t recalculateInterval = 5000; // Recalculate due times every 5 seconds
    static uint64_t lastRecalculateTime = 0;

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
