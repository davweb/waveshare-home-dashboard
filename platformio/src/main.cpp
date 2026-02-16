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

using namespace esp_panel::drivers;

void fetchData() {
    LOG_DEBUG("fetch data");
    JsonDocument doc;
    int offset;

    if (!getJsonFromUrl(doc, "http://192.168.1.17")) {
        LOG_WARN("Failed to get data from dashboard server.");
        return;
    }

    lvgl_port_lock(-1);

    auto topStop = doc["bus_stops"][0];
    lv_label_set_text(objects.top__stop_name, topStop["name"] | "");
    auto buses = topStop["buses"];
    lv_label_set_text(objects.top__row1__bus_route, buses[0]["route"] | "");
    lv_label_set_text(objects.top__row1__bus_destination, buses[0]["destination"] | "");
    lv_label_set_text(objects.top__row1__bus_due, buses[0]["due"] | "");
    lv_label_set_text(objects.top__row2__bus_route, buses[1]["route"] | "");
    lv_label_set_text(objects.top__row2__bus_destination, buses[1]["destination"] | "");
    lv_label_set_text(objects.top__row2__bus_due, buses[1]["due"] | "");
    lv_label_set_text(objects.top__row3__bus_route, buses[2]["route"] | "");
    lv_label_set_text(objects.top__row3__bus_destination, buses[2]["destination"] | "");
    lv_label_set_text(objects.top__row3__bus_due, buses[2]["due"] | "");

    auto bottomStop = doc["bus_stops"][1];
    lv_label_set_text(objects.bottom__stop_name, bottomStop["name"] | "");
    buses = bottomStop["buses"];
    lv_label_set_text(objects.bottom__row1__bus_route, buses[0]["route"] | "");
    lv_label_set_text(objects.bottom__row1__bus_destination, buses[0]["destination"] | "");
    lv_label_set_text(objects.bottom__row1__bus_due, buses[0]["due"] | "");
    lv_label_set_text(objects.bottom__row2__bus_route, buses[1]["route"] | "");
    lv_label_set_text(objects.bottom__row2__bus_destination, buses[1]["destination"] | "");
    lv_label_set_text(objects.bottom__row2__bus_due, buses[1]["due"] | "");
    lv_label_set_text(objects.bottom__row3__bus_route, buses[2]["route"] | "");
    lv_label_set_text(objects.bottom__row3__bus_destination, buses[2]["destination"] | "");
    lv_label_set_text(objects.bottom__row3__bus_due, buses[2]["due"] | "");

    lvgl_port_unlock();
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
    fetchData();
    ui_tick();
    LOG_TRACE("Idle loop");
    delay(30000);
}

