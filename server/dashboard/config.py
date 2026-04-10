
"""Merge configuration from environment variables and the command line"""

import argparse
import logging
import os

FIRMWARE_PATH = '/ota/firmware'


def _get_arguments() -> argparse.Namespace:
    """Parse command line arguments"""

    parser = argparse.ArgumentParser(prog='dashboard', description='Server for Home Dashboard.')
    parser.add_argument('-p', '--port', action='store', type=int, help='Port to listen on')
    parser.add_argument('-b', '--bus-stop-id', action='append', help='Bus Stop ID')
    parser.add_argument('-l', '--lat-long', action='store', help='Latitude and Longitude for weather')
    parser.add_argument('-k', '--pirate-api-key', action='store', help='API Key for Pirate Weather')
    parser.add_argument('-r', '--recycling-calendar-url', action='store', help='URL for recycling collection calendar')
    parser.add_argument('--unifi-url', action='store',
                        help='Base URL of the UniFi controller (e.g. https://192.168.1.1)')
    parser.add_argument('--unifi-username', action='store', help='UniFi controller username')
    parser.add_argument('--unifi-password', action='store', help='UniFi controller password')
    parser.add_argument('--unifi-site', action='store', help='UniFi site name (default: default)')
    parser.add_argument('--unifi-client', action='append', help='Client to watch, format: mac=name')
    parser.add_argument('--mqtt-broker-host', action='store', help='MQTT broker hostname')
    parser.add_argument('--mqtt-broker-port', action='store', type=int, help='MQTT broker port')
    parser.add_argument('--mqtt-topic-prefix', action='store', help='MQTT topic prefix (e.g. dashboard)')
    parser.add_argument('--server-base-url', action='store',
                        help='Publicly accessible base URL of this server '
                             '(e.g. http://dashboard-server:8000), used in OTA MQTT messages')

    return parser.parse_args()


class Config:
    """Configuration class"""

    def __init__(self) -> None:
        self._args = _get_arguments()

    @property
    def port(self) -> int:
        """Return the port to listen on"""

        if self._args.port is not None:
            return self._args.port

        if 'DASHBOARD_PORT' in os.environ:
            return int(os.environ['DASHBOARD_PORT'])

        return 8000

    @property
    def bus_stop_ids(self) -> list[str]:
        """Bus Stops to fetch data for"""

        if self._args.bus_stop_id:
            return self._args.bus_stop_id

        if 'BUS_STOP_IDS' in os.environ:
            return os.environ['BUS_STOP_IDS'].split(',')

        raise ValueError('No bus stop IDs provided')

    @property
    def bus_stop_names(self) -> dict[str, str]:
        """Bus Stops to fetch data for"""

        # Bus stop names are stored in BUS_STOP_NAMES as a comma-separated list of the format id=name
        if 'BUS_STOP_NAMES' in os.environ and os.environ['BUS_STOP_NAMES']:
            return dict(pair.split('=', 1) for pair in os.environ['BUS_STOP_NAMES'].split(',') if '=' in pair)

        return {}

    @property
    def lat_long(self) -> str:
        """Lat-long for weather"""

        if self._args.lat_long:
            return self._args.lat_long

        if 'LAT_LONG' in os.environ:
            return os.environ['LAT_LONG']

        raise ValueError('No lat-long provided')

    @property
    def recycling_calendar_url(self) -> str:
        """URL for Recycling Collection Calendar"""

        if self._args.recycling_calendar_url:
            return self._args.recycling_calendar_url

        if 'RECYCLING_CALENDAR_URL' in os.environ:
            return os.environ['RECYCLING_CALENDAR_URL']

        raise ValueError('No recycling calendar provided')

    @property
    def pirate_api_key(self) -> str:
        """API Key for Pirate Weather"""

        if self._args.pirate_api_key:
            return self._args.pirate_api_key

        if 'PIRATE_API_KEY' in os.environ:
            return os.environ['PIRATE_API_KEY']

        raise ValueError('No Pirate API Key provided')

    @property
    def unifi_url(self) -> str:
        """Base URL of the UniFi controller (e.g. https://192.168.1.1)"""

        if self._args.unifi_url:
            return self._args.unifi_url

        if 'UNIFI_URL' in os.environ:
            return os.environ['UNIFI_URL']

        raise ValueError('No UniFi controller URL provided')

    @property
    def unifi_username(self) -> str:
        """UniFi controller username"""

        if self._args.unifi_username:
            return self._args.unifi_username

        if 'UNIFI_USERNAME' in os.environ:
            return os.environ['UNIFI_USERNAME']

        raise ValueError('No UniFi username provided')

    @property
    def unifi_password(self) -> str:
        """UniFi controller password"""

        if self._args.unifi_password:
            return self._args.unifi_password

        if 'UNIFI_PASSWORD' in os.environ:
            return os.environ['UNIFI_PASSWORD']

        raise ValueError('No UniFi password provided')

    @property
    def unifi_site(self) -> str:
        """UniFi site name"""

        if self._args.unifi_site:
            return self._args.unifi_site

        if 'UNIFI_SITE' in os.environ:
            return os.environ['UNIFI_SITE']

        return 'default'

    @property
    def unifi_client_names(self) -> dict[str, str]:
        """MAC-to-name mapping. Also defines which clients are watched."""

        if self._args.unifi_client:
            pairs = self._args.unifi_client
        elif 'UNIFI_CLIENT_NAMES' in os.environ:
            pairs = os.environ['UNIFI_CLIENT_NAMES'].split(',')
        else:
            return {}

        result = dict(pair.split('=', 1) for pair in pairs if '=' in pair)
        return {mac.strip().lower(): name for mac, name in result.items()}

    @property
    def mqtt_broker_host(self) -> str:
        """MQTT broker hostname"""

        if self._args.mqtt_broker_host:
            return self._args.mqtt_broker_host

        return os.environ.get('MQTT_BROKER_HOST', 'mosquitto.home.arpa')

    @property
    def mqtt_broker_port(self) -> int:
        """MQTT broker port"""

        if self._args.mqtt_broker_port:
            return self._args.mqtt_broker_port

        return int(os.environ.get('MQTT_BROKER_PORT', '1883'))

    @property
    def mqtt_topic_prefix(self) -> str:
        """MQTT topic prefix"""

        if self._args.mqtt_topic_prefix:
            return self._args.mqtt_topic_prefix

        return os.environ.get('MQTT_TOPIC_PREFIX', 'dashboard')

    @property
    def server_base_url(self) -> str:
        """Publicly accessible base URL of this server, used in OTA MQTT messages."""

        if self._args.server_base_url:
            return self._args.server_base_url

        return os.environ.get('SERVER_BASE_URL', 'http://dashboard.home.arpa')

    @property
    def log_level(self) -> int:
        """Log Level"""

        log_level_name = os.environ.get('LOG_LEVEL', 'INFO')
        log_level = logging.getLevelName(log_level_name)

        if isinstance(log_level, str):
            raise ValueError(f'Invalid LOG_LEVEL: {log_level_name}')

        return log_level


CONFIG = Config()
