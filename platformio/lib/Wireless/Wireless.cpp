#include <WiFi.h>
#include <DebugLog.h>
#include "Wireless.h"
#include "WirelessConfig.h"
#include "esp_bt.h"
#include "esp_wifi.h"

bool startWiFi() {
    #ifndef WIFI_SSID
        LOG_ERROR("No WiFi credentials provided");
        return false;
    #else
        // Release Bluetooth memory as we are not using Bluetooth
        esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);

        // Disable Wifi sleep to prevent interrupt storms on wake
        WiFi.setSleep(false);
        esp_wifi_set_ps(WIFI_PS_NONE);

        // Save credentials to NVS flash and enable auto reconnect
        WiFi.persistent(true);
        WiFi.setAutoReconnect(true);

        //Disable IPv6 to speed up connection time as we are not using it
        WiFi.enableIPv6(false);

        WiFi.mode(WIFI_MODE_STA);

        #ifdef WIFI_IP_ADDRESS
            IPAddress ipAddress, gateway, subnet, dns1, dns2;
            ipAddress.fromString(WIFI_IP_ADDRESS);
            gateway.fromString(WIFI_GATEWAY);
            subnet.fromString(WIFI_SUBNET);
            dns1.fromString(WIFI_DNS1);
            dns2.fromString(WIFI_DNS2);

            if (WiFi.config(ipAddress, gateway, subnet, dns1, dns2)) {
                LOG_DEBUG("Using static IP address", ipAddress);
            }
            else {
                LOG_ERROR("Failed to set static IP address", ipAddress);
                return false;
            }
        #else
            LOG_DEBUG("Using DHCP IP address");
        #endif

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        uint8_t result = WiFi.waitForConnectResult();

        if (result == WL_CONNECTED) {
            LOG_INFO("Connected to WiFi network");
        }
        else {
            LOG_ERROR("Failed to connect to WiFi network");
        }

        return result == WL_CONNECTED;
    #endif
}

bool stopWiFi() {
    #ifndef WIFI_SSID
        LOG_ERROR("No WiFi credentials provided");
        return false;
    #else
        bool result = WiFi.disconnect();

        if (result) {
            LOG_INFO("Disconnected from WiFi network");
        }
        else {
            LOG_ERROR("Failed to disconnect from WiFi network");
        }

        return result;
    #endif
}

bool isWiFiConnected() {
    #ifndef WIFI_SSID
        return false;
    #else
        return WiFi.status() == WL_CONNECTED;
    #endif
}

String getLocalIpAddress() {
    return isWiFiConnected() ? WiFi.localIP().toString() : "";
}

String getMacAddress() {
    return isWiFiConnected() ? WiFi.macAddress() : "";
}
