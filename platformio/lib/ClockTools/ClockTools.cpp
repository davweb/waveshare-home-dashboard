#include <esp_display_panel.hpp>
#include <DebugLog.h>
#include <ArduinoJson.h>
#include "ClockTools.h"
#include "HttpTools.h"
#include <time.h>

String timezone = "";

// Fetch time from NTP server and set the RTC
bool setRtcClock() {
    JsonDocument doc;
    int offset;

    if (!getJsonFromUrl(doc, "http://ip-api.com/json/?fields=status,timezone,offset")) {
        LOG_WARN("Failed to get timezone from IP API. Using UTC.");
        timezone = "UTC";
        offset = 0;
    }
    else {
        timezone = (String)doc["timezone"];
        offset = doc["offset"];
        LOG_INFO("Timezone", timezone, "with offset", offset);
    }

    const char* ntpServer = "pool.ntp.org";
    configTime(offset, 0, ntpServer);

    LOG_INFO("RTC clock set to", getCurrentDateTime());
    return true;
}

// Get the current time and store in a state variable
String getCurrentTime() {
    struct tm timeinfo;

    if(!getLocalTime(&timeinfo)) {
        LOG_WARN("Failed to obtain time");
        return "";
    }

    char currentTime[6];
    sprintf(currentTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    LOG_TRACE("Current time", currentTime);
    return String(currentTime);
}

String getCurrentDateTime() {
    struct tm timeinfo;

    if(!getLocalTime(&timeinfo)) {
        LOG_WARN("Failed to obtain time");
        return "";
    }

    char dateTime[20];
    sprintf(dateTime, "%4d-%02d-%02d %02d:%02d:%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    LOG_TRACE("Current date time", dateTime);
    return String(dateTime);
}

String getTimezone() {
    return timezone;
}
