#include <DebugLog.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HttpTools.h"
#include "Wireless.h"


bool getJsonFromUrl(JsonDocument &doc, String url) {
    if (!isWiFiConnected()) {
        LOG_ERROR("Not connected to WiFi network");
        return false;
    }

    HTTPClient http;
    String response;
    LOG_DEBUG("Getting JSON from URL", url);

    if (http.begin(url)) {
        int statusCode = http.GET();

        if (statusCode == 200) {
            response = http.getString();
        } else {
            LOG_ERROR("HTTP GET failed with status code", statusCode);
            return false;
        }
    }
    else {
        LOG_ERROR("Failed to make HTTP request");
        return false;
    }

    deserializeJson(doc, response);
    return true;
}