#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>
#include "ClockTools.h"
#include "HttpTools.h"

static const char *TAG = "ClockTools";

static char timezone_str[64] = "UTC";

// Fetch time from NTP server and set the RTC
bool setRtcClock() {
    JsonDocument doc;
    int offset = 0;

    if (!getJsonFromUrl(doc, "http://ip-api.com/json/?fields=status,timezone,offset")) {
        ESP_LOGW(TAG, "Failed to get timezone from IP API. Using UTC.");
    }
    else {
        strlcpy(timezone_str, doc["timezone"] | "UTC", sizeof(timezone_str));
        offset = doc["offset"] | 0;
        ESP_LOGI(TAG, "Timezone %s with offset %d", timezone_str, offset);
    }

    // Build POSIX TZ string from UTC offset (sign is inverted vs UTC offset)
    char tz_posix[16];
    int hours = -offset / 3600;
    int mins = abs(offset % 3600) / 60;
    if (mins != 0) {
        snprintf(tz_posix, sizeof(tz_posix), "UTC%+d:%02d", hours, mins);
    } else {
        snprintf(tz_posix, sizeof(tz_posix), "UTC%+d", hours);
    }
    setenv("TZ", tz_posix, 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait up to 15 * 2s = 30s for time to sync
    struct tm timeinfo = {};
    for (int retry = 0; retry < 15 && timeinfo.tm_year < (2020 - 1900); retry++) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        time_t now = time(nullptr);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGW(TAG, "Failed to sync time via NTP");
        return false;
    }

    ESP_LOGI(TAG, "RTC clock set to %s", getCurrentDateTime());
    return true;
}

const char* getCurrentTime() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    static char currentTime[6];
    snprintf(currentTime, sizeof(currentTime), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    ESP_LOGV(TAG, "Current time %s", currentTime);
    return currentTime;
}

const char* getCurrentDateTime() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    static char dateTime[32];
    snprintf(dateTime, sizeof(dateTime), "%4d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    ESP_LOGV(TAG, "Current date time %s", dateTime);
    return dateTime;
}

const char* getTimezone() {
    return timezone_str;
}
