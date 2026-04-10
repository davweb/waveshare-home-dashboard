#pragma once

#include "sdkconfig.h"

// ---------------------------------------------------------------------------
// Topic table — one row per topic.  To add a topic:
//   1. Add a row here
//   2. Add a case in onMqttMessage (main.cpp)
// Everything else (enum, statics, subscribe, routing) is derived automatically.
// ---------------------------------------------------------------------------

#define MQTT_TOPICS(X) \
    X(BUS_STOPS,          CONFIG_MQTT_TOPIC_PREFIX "/bus_stops") \
    X(WEATHER,            CONFIG_MQTT_TOPIC_PREFIX "/weather")   \
    X(RECYCLING,          CONFIG_MQTT_TOPIC_PREFIX "/recycling") \
    X(PRESENCE,           CONFIG_MQTT_TOPIC_PREFIX "/presence")  \
    X(OTA,                CONFIG_MQTT_TOPIC_PREFIX "/ota")       \
    X(SERVER,             CONFIG_MQTT_TOPIC_PREFIX "/server")    \
    X(SYS_BROKER_VERSION, "$SYS/broker/version")

enum class MqttTopic {
#define X(name, str) name,
    MQTT_TOPICS(X)
#undef X
};

// Callback invoked on the MQTT client task when a message arrives on a known topic.
// data is the raw payload (NOT null-terminated); len is its length.
using MqttMessageCallback = void(*)(MqttTopic topic, const char *data, int len);

// Start the MQTT client and register the message callback.
// Call once after WiFi is started; esp-mqtt handles reconnection automatically.
void mqtt_init(MqttMessageCallback cb);
