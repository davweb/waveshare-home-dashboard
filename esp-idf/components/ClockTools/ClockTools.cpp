#include "esp_log.h"
#include "esp_sntp.h"
#include <time.h>
#include "sdkconfig.h"
#include "ClockTools.h"

static const char *TAG = "ClockTools";

static void on_ntp_sync(struct timeval * /*tv*/)
{
    time_t now = time(nullptr);
    char buf[32];
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "NTP sync complete: %s", buf);
}

void setRtcClock()
{
    setenv("TZ", CONFIG_CLOCK_POSIX_TZ, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone: %s", CONFIG_CLOCK_POSIX_TZ);

    sntp_set_time_sync_notification_cb(on_ntp_sync);
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, CONFIG_CLOCK_NTP_SERVER);
    esp_sntp_init();
}
