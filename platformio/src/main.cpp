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

using namespace esp_panel::drivers;

void fetchData() {
    LOG_DEBUG("fetch data");
    JsonDocument doc;

    if (!getJsonFromUrl(doc, "http://192.168.1.17")) {
        LOG_WARN("Failed to get data from dashboard server.");
        return;
    }

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
}

void setup()
{
    Serial.begin(115200);

    // Wait for serial port to connect
    delay(5000);

    Board *board = initialiseBoard();

    startWiFi();

    setRtcClock();

    LOG_DEBUG("Initializing LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    LOG_DEBUG("Creating UI");
    ui_init();
    fetchData();
}


void loop()
 {
    static unsigned long lastFetchTime = ::millis();
    const unsigned long fetchInterval = 30000;
    unsigned long currentTime = ::millis();

    if (currentTime - lastFetchTime >= fetchInterval) {
        lastFetchTime = currentTime;
        fetchData();
    }

    ui_tick();
 }

