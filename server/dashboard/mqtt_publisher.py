"""MQTT publisher for dashboard data.

Maintains a persistent connection to the broker and publishes each data
type to its own retained topic so devices receive the latest value
immediately on subscribe.
"""

import json
import logging
import threading
import paho.mqtt.client as mqtt

from .config import CONFIG

logger = logging.getLogger(__name__)

_connected = threading.Event()
_last_published: dict[str, str] = {}
_publish_lock = threading.Lock()


def _on_connect(client: mqtt.Client, _userdata: object, _flags: object,
                _reason_code: object, _props: object) -> None:
    with _publish_lock:
        _last_published.clear()
    _connected.set()
    logger.info('MQTT connected to %s:%d (prefix: %s)',
                CONFIG.mqtt_broker_host, CONFIG.mqtt_broker_port,
                CONFIG.mqtt_topic_prefix)
    client.publish(f"{CONFIG.mqtt_topic_prefix}/server",
                   json.dumps({"connected": True}), qos=1, retain=True)


def _on_disconnect(_client: mqtt.Client, _userdata: object, _flags: object,
                   reason_code: object, _props: object) -> None:
    _connected.clear()
    logger.warning('MQTT disconnected (reason: %s) — will reconnect', reason_code)


_CLIENT = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
_CLIENT.on_connect = _on_connect
_CLIENT.on_disconnect = _on_disconnect
_CLIENT.will_set(f"{CONFIG.mqtt_topic_prefix}/server",
                 json.dumps({"connected": False}), qos=1, retain=True)
_CLIENT.connect_async(CONFIG.mqtt_broker_host, CONFIG.mqtt_broker_port)
_CLIENT.loop_start()


def publish(topic_suffix: str, payload: object) -> None:
    """Publish *payload* as JSON to ``{prefix}/{topic_suffix}`` with retain=True."""
    topic = f"{CONFIG.mqtt_topic_prefix}/{topic_suffix}"
    data = json.dumps(payload)
    try:
        if not _connected.wait(timeout=10):
            logger.warning('MQTT not connected after 10s, skipping publish to %s', topic)
            return
        with _publish_lock:
            if _last_published.get(topic) == data:
                logger.debug('Skipping unchanged publish to %s', topic)
                return
            result = _CLIENT.publish(topic, data, retain=True, qos=1)
            if result.rc != mqtt.MQTT_ERR_SUCCESS:
                logger.warning('MQTT publish to %s returned rc=%d', topic, result.rc)
            else:
                _last_published[topic] = data
                logger.debug('Published to %s', topic)
    except Exception:  # pylint: disable=broad-exception-caught
        logger.exception('Failed to publish to MQTT topic %s', topic)
