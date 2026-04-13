#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "Wireless.h"

static const char *TAG = "Wireless";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static bool s_wifi_connected = false;
static esp_netif_t *s_sta_netif = NULL;
static WifiStateCallback s_wifi_state_callback = NULL;

void setWiFiStateCallback(WifiStateCallback callback) {
    s_wifi_state_callback = callback;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        if (s_wifi_state_callback) s_wifi_state_callback(false);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_connected = true;
        if (s_wifi_state_callback) s_wifi_state_callback(true);
    }
}

void startWiFi() {
    if (CONFIG_WIFI_SSID[0] == '\0') {
        ESP_LOGE(TAG, "No WiFi credentials configured.");
        return;
    }

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();

    #if CONFIG_WIFI_USE_STATIC_IP
        esp_netif_dhcpc_stop(s_sta_netif);

        esp_netif_ip_info_t ip_info = {};
        ip_info.ip.addr      = esp_ip4addr_aton(CONFIG_WIFI_IP_ADDRESS);
        ip_info.gw.addr      = esp_ip4addr_aton(CONFIG_WIFI_GATEWAY);
        ip_info.netmask.addr = esp_ip4addr_aton(CONFIG_WIFI_SUBNET);
        ESP_ERROR_CHECK(esp_netif_set_ip_info(s_sta_netif, &ip_info));

        esp_netif_dns_info_t dns1 = {};
        dns1.ip.u_addr.ip4.addr = esp_ip4addr_aton(CONFIG_WIFI_DNS1);
        dns1.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns1);

        esp_netif_dns_info_t dns2 = {};
        dns2.ip.u_addr.ip4.addr = esp_ip4addr_aton(CONFIG_WIFI_DNS2);
        dns2.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_BACKUP, &dns2);

        ESP_LOGD(TAG, "Using static IP address: %s", CONFIG_WIFI_IP_ADDRESS);
    #else
        ESP_LOGD(TAG, "Using DHCP IP address");
    #endif

    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid,     CONFIG_WIFI_SSID,     sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool stopWiFi() {
    esp_err_t err = esp_wifi_disconnect();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Disconnected from WiFi network");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to disconnect from WiFi network");
        return false;
    }
}

bool isWiFiConnected() {
    return s_wifi_connected;
}

const char* getLocalIpAddress() {
    static char ip_str[16] = "";
    if (!isWiFiConnected() || s_sta_netif == NULL) {
        return "";
    }
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
        return ip_str;
    }
    return "";
}

const char* getMacAddress() {
    static char mac_str[18] = "";
    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return mac_str;
    }
    return "";
}

const char* getSubnetMask() {
    static char mask_str[16] = "";
    if (!isWiFiConnected() || s_sta_netif == NULL) return "";
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.netmask, mask_str, sizeof(mask_str));
        return mask_str;
    }
    return "";
}

const char* getDefaultGateway() {
    static char gw_str[16] = "";
    if (!isWiFiConnected() || s_sta_netif == NULL) return "";
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.gw, gw_str, sizeof(gw_str));
        return gw_str;
    }
    return "";
}

const char* getWiFiSSID() {
    static wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return (const char*)ap_info.ssid;
    }
    return "";
}

int getWiFiRSSI() {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return (int)ap_info.rssi;
    }
    return 0;
}
