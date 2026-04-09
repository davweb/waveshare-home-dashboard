#include "MqttTools.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include <cJSON.h>

#include <string>

static const char *TAG = "MqttTools";

static MqttMessageCallback s_callback;
static esp_mqtt_client_handle_t s_client = nullptr;

static std::string s_topic_bus_stops;
static std::string s_topic_weather;
static std::string s_topic_recycling;
static std::string s_topic_presence;
static std::string s_topic_ota;
static std::string s_topic_server;

static bool topic_from_event(const char *topic, int len, MqttTopic &out)
{
    std::string t(topic, (size_t)len);
    if (t == s_topic_bus_stops) { out = MqttTopic::BUS_STOPS; return true; }
    if (t == s_topic_weather)   { out = MqttTopic::WEATHER;   return true; }
    if (t == s_topic_recycling) { out = MqttTopic::RECYCLING; return true; }
    if (t == s_topic_presence)  { out = MqttTopic::PRESENCE;  return true; }
    if (t == s_topic_ota)       { out = MqttTopic::OTA;       return true; }
    if (t == s_topic_server)    { out = MqttTopic::SERVER;    return true; }
    return false;
}

static void mqtt_event_handler(void * /*handler_args*/, esp_event_base_t /*base*/,
                                int32_t event_id, void *event_data)
{
    auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to %s — subscribing (prefix: %s)",
                     CONFIG_MQTT_BROKER_URL, CONFIG_MQTT_TOPIC_PREFIX);
            esp_mqtt_client_subscribe(s_client, s_topic_bus_stops.c_str(), 1);
            esp_mqtt_client_subscribe(s_client, s_topic_weather.c_str(),   1);
            esp_mqtt_client_subscribe(s_client, s_topic_recycling.c_str(), 1);
            esp_mqtt_client_subscribe(s_client, s_topic_presence.c_str(),  1);
            esp_mqtt_client_subscribe(s_client, s_topic_ota.c_str(),       1);
            esp_mqtt_client_subscribe(s_client, s_topic_server.c_str(),    1);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected — will reconnect automatically");
            break;

        case MQTT_EVENT_DATA: {
            if (!s_callback || !event->topic) break;

            MqttTopic topic;
            if (!topic_from_event(event->topic, event->topic_len, topic)) {
                ESP_LOGW(TAG, "Message on unknown topic, ignoring");
                break;
            }

            std::string payload(event->data, (size_t)event->data_len);
            ESP_LOGD(TAG, "MQTT message on %.*s: %s",
                     event->topic_len, event->topic, payload.c_str());
            cJSON *root = cJSON_Parse(payload.c_str());
            if (!root) {
                ESP_LOGW(TAG, "Failed to parse JSON payload on topic %.*s",
                         event->topic_len, event->topic);
                break;
            }
            s_callback(topic, root);
            cJSON_Delete(root);
            break;
        }

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error event");
            break;

        default:
            break;
    }
}

void mqtt_init(MqttMessageCallback cb)
{
    s_callback = cb;

    const char *prefix = CONFIG_MQTT_TOPIC_PREFIX;
    s_topic_bus_stops = std::string(prefix) + "/bus_stops";
    s_topic_weather   = std::string(prefix) + "/weather";
    s_topic_recycling = std::string(prefix) + "/recycling";
    s_topic_presence  = std::string(prefix) + "/presence";
    s_topic_ota       = std::string(prefix) + "/ota";
    s_topic_server    = std::string(prefix) + "/server";

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = CONFIG_MQTT_BROKER_URL;
    cfg.buffer.size = 4096;

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr);
    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "MQTT client started");
}
