#include <Arduino.h>
#include <DebugLog.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include <lvgl_v8_port.h>
#include <Board.h>
#include <Wireless.h>
#include <ClockTools.h>
#include <HttpTools.h>
#include <ui.h>
#include <vars.h>
#include <structs.h>
#include <images.h>
#include <screens.h>

using namespace esp_panel::drivers;

void fetchData() {
    LOG_DEBUG("fetch data");
    JsonDocument doc;

    if (!getJsonFromUrl(doc, "http://192.168.1.17")) {
        LOG_WARN("Failed to get data from dashboard server.");
        return;
    }

    //Lock the LVGL mutex
    if (lvgl_port_lock(portMAX_DELAY)) {

        for (int j = 0; j < 2; j++) {
            ArrayOfString routes(3);
            ArrayOfString destinations(3);
            ArrayOfString dueTimes(3);

            for (int i = 0; i < 3; i++) {
                routes.at(i, doc["bus_stops"][j]["buses"][i]["route"] | "");
                destinations.at(i, doc["bus_stops"][j]["buses"][i]["destination"] | "");
                dueTimes.at(i, doc["bus_stops"][j]["buses"][i]["due"] | "");
            }

            flow::setGlobalVariable(j == 0 ? FLOW_GLOBAL_VARIABLE_BUS_STOP_1_NAME : FLOW_GLOBAL_VARIABLE_BUS_STOP_2_NAME, Value(doc["bus_stops"][j]["name"] | ""));
            flow::setGlobalVariable(j == 0 ? FLOW_GLOBAL_VARIABLE_BUS_STOP_1_ROUTES : FLOW_GLOBAL_VARIABLE_BUS_STOP_2_ROUTES, routes);
            flow::setGlobalVariable(j == 0 ? FLOW_GLOBAL_VARIABLE_BUS_STOP_1_DESTINATIONS : FLOW_GLOBAL_VARIABLE_BUS_STOP_2_DESTINATIONS, destinations);
            flow::setGlobalVariable(j == 0 ? FLOW_GLOBAL_VARIABLE_BUS_STOP_1_TIMES : FLOW_GLOBAL_VARIABLE_BUS_STOP_2_TIMES, dueTimes);
        }

        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_TEMPERATURE, Value(doc["weather"]["temperature"] | "", VALUE_TYPE_STRING));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RAIN_CHANCE, Value(doc["weather"]["rain"] | "", VALUE_TYPE_STRING));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING_DATE, Value(doc["recycling"]["date"] | "", VALUE_TYPE_STRING));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING_SHORT_DATE, Value(doc["recycling"]["short_date"] | "", VALUE_TYPE_STRING));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_RECYCLING_TYPE, Value(doc["recycling"]["type"] | "", VALUE_TYPE_STRING));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SUN_TYPE, Value(doc["weather"]["sun"]["event"] | "", VALUE_TYPE_STRING));
        flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_SUN_TIME, Value(doc["weather"]["sun"]["time"] | "", VALUE_TYPE_STRING));

        lvgl_port_unlock();
    }
    else {
        LOG_ERROR("Failed to lock LVGL mutex to update data");
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

    snprintf(date_str, sizeof(date_str), "%s %d %s", weekday, timeinfo.tm_mday, month);
    return date_str;
}

void set_var_date(const char *value) {
    // Do nothing
}

void setup()
{
    Serial.begin(115200);

    #ifndef NDEBUG
        // Wait for serial port to connect
        delay(5000);
    #endif

    Board *board = initialiseBoard();

    LOG_DEBUG("Initializing LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    startWiFi();

    LOG_DEBUG("Creating UI");
    ui_init();
}


void loop()
 {
    static boolean wifiConnected = false;
    static unsigned long fetchInterval = 30000; // Fetch data every 30 seconds
    static unsigned long lastFetchTime = 0;
    unsigned long currentTime = ::millis();

    if (!isWiFiConnected()) {
        //If WiFi was connected but now isn't, update the global variable
        if (wifiConnected) {
            LOG_WARN("WiFi Disconnected");

            if (lvgl_port_lock(portMAX_DELAY)) {
                flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_CONNECTED, Value(false));
                lvgl_port_unlock();
                wifiConnected = false;
            }
        }
    }
    else {
        if (!wifiConnected) {
            LOG_INFO("WiFi Connected");

            if (lvgl_port_lock(portMAX_DELAY)) {
                flow::setGlobalVariable(FLOW_GLOBAL_VARIABLE_WIFI_CONNECTED, Value(true));
                lvgl_port_unlock();
                wifiConnected = true;

                setRtcClock();
            }
        }

        // currentTime will be in 1970 before we set the clock
        if (lastFetchTime == 0 || (currentTime - lastFetchTime >= fetchInterval)) {
            lastFetchTime = currentTime;
            fetchData();
        }
    }

    uint32_t time_till_next_ms = 10;

    // Lock the mutex with a reasonable timeout so we don't randomly skip UI updates
    if (lvgl_port_lock(10)) {
        ui_tick();
        lvgl_port_unlock();
    }
 }

