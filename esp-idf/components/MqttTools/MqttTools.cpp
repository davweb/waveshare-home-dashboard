#include "MqttTools.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>

static const char *TAG = "MqttTools";

static MqttMessageCallback s_callback;
static esp_mqtt_client_handle_t s_client = nullptr;

// One static string per topic, generated from the master table.
#define X(name, str) static const char s_topic_##name[] = str;
MQTT_TOPICS(X)
#undef X

static bool topic_from_event(const char *topic, int len, MqttTopic &out)
{
    auto matches = [topic, len](const char *s) {
        return strncmp(topic, s, len) == 0 && s[len] == '\0';
    };
#define X(name, str) if (matches(s_topic_##name)) { out = MqttTopic::name; return true; }
    MQTT_TOPICS(X)
#undef X
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
            esp_mqtt_client_subscribe(s_client, CONFIG_MQTT_TOPIC_PREFIX "/#",  1);
            esp_mqtt_client_subscribe(s_client, s_topic_SYS_BROKER_VERSION,     1);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected — will reconnect automatically");
            break;

        case MQTT_EVENT_DATA: {
            if (!s_callback || !event->topic) break;

            MqttTopic topic;
            if (!topic_from_event(event->topic, event->topic_len, topic)) {
                ESP_LOGW(TAG, "Unhandled topic: %.*s", event->topic_len, event->topic);
                break;
            }

            ESP_LOGD(TAG, "Message on %.*s (%d bytes)",
                     event->topic_len, event->topic, event->data_len);
            s_callback(topic, event->data, event->data_len);
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

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = CONFIG_MQTT_BROKER_URL;
    cfg.buffer.size = 4096;

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr);
    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "MQTT client started");
}
