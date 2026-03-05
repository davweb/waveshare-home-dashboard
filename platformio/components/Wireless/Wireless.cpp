#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "Wireless.h"
#include "WirelessConfig.h"

static const char *TAG = "Wireless";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group = NULL;
static bool s_wifi_connected = false;
static esp_netif_t *s_sta_netif = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_connected = true;
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

bool startWiFi() {
    #ifndef WIFI_SSID
        ESP_LOGE(TAG, "No WiFi credentials provided");
        return false;
    #else
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase();
            nvs_flash_init();
        }

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        s_sta_netif = esp_netif_create_default_wifi_sta();

        #ifdef WIFI_IP_ADDRESS
            esp_netif_dhcpc_stop(s_sta_netif);

            esp_netif_ip_info_t ip_info = {};
            ip4addr_aton(WIFI_IP_ADDRESS, (ip4_addr_t *)&ip_info.ip);
            ip4addr_aton(WIFI_GATEWAY, (ip4_addr_t *)&ip_info.gw);
            ip4addr_aton(WIFI_SUBNET, (ip4_addr_t *)&ip_info.netmask);
            ESP_ERROR_CHECK(esp_netif_set_ip_info(s_sta_netif, &ip_info));

            esp_netif_dns_info_t dns1 = {};
            ip4addr_aton(WIFI_DNS1, (ip4_addr_t *)&dns1.ip.u_addr.ip4);
            dns1.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns1);

            esp_netif_dns_info_t dns2 = {};
            ip4addr_aton(WIFI_DNS2, (ip4_addr_t *)&dns2.ip.u_addr.ip4);
            dns2.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_BACKUP, &dns2);

            ESP_LOGD(TAG, "Using static IP address: %s", WIFI_IP_ADDRESS);
        #else
            ESP_LOGD(TAG, "Using DHCP IP address");
        #endif

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        s_wifi_event_group = xEventGroupCreate();

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                            &wifi_event_handler, NULL, NULL));

        wifi_config_t wifi_config = {};
        strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdFALSE, pdFALSE,
                                               pdMS_TO_TICKS(30000));

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to WiFi network");
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to connect to WiFi network");
            return false;
        }
    #endif
}

bool stopWiFi() {
    #ifndef WIFI_SSID
        ESP_LOGE(TAG, "No WiFi credentials provided");
        return false;
    #else
        esp_err_t err = esp_wifi_disconnect();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Disconnected from WiFi network");
            return true;
        } else {
            ESP_LOGE(TAG, "Failed to disconnect from WiFi network");
            return false;
        }
    #endif
}

bool isWiFiConnected() {
    #ifndef WIFI_SSID
        return false;
    #else
        return s_wifi_connected;
    #endif
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
