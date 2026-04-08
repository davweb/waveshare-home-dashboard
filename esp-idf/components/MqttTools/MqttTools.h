#pragma once

#include <functional>
#include <string>
#include <cJSON.h>

enum class MqttTopic {
    BUS_STOPS,
    WEATHER,
    RECYCLING,
    PRESENCE,
    OTA,
};

// Callback invoked on the MQTT client task when a message arrives on a known topic.
// root is the parsed cJSON tree — the callback must NOT call cJSON_Delete on it.
using MqttMessageCallback = std::function<void(MqttTopic topic, cJSON *root)>;

// Start the MQTT client and register the message callback.
// Call once after WiFi is started; esp-mqtt handles reconnection automatically.
void mqtt_init(MqttMessageCallback cb);
