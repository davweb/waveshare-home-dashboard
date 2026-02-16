#include <WiFi.h>
#include <DebugLog.h>
#include "Wireless.h"
#include "WirelessConfig.h"

// If we have a static IP we don't need to wait as long for the connection
#ifdef WIFI_IP_ADDRESS
    #define CONNECTION_INTERVAL_MS 40
    #define CONNECTION_TIMEOUT_MS 400
#else
    #define CONNECTION_INTERVAL_MS 500
    #define CONNECTION_TIMEOUT_MS 10000
#endif


bool startWiFi() {
    #ifndef WIFI_SSID
        LOG_ERROR("No WiFi credentials provided");
        return false;
    #else
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

        int time = 0;

        while (!isWiFiConnected() && time < CONNECTION_TIMEOUT_MS) {
            delay(CONNECTION_INTERVAL_MS);
            time += CONNECTION_INTERVAL_MS;
            LOG_TRACE("Have waited", time, "ms for WiFi connection");
        }

        if (!isWiFiConnected()) {
            LOG_ERROR("Failed to connect to WiFi network", WIFI_SSID);
            return false;
        }

        LOG_INFO("Connected to WiFi network", WIFI_SSID, "with IP address", getLocalIpAddress());
        return true;
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
