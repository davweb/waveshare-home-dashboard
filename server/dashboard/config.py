
"""Merge configuration from environment variables and the command line"""

import argparse
import logging
import os


def _get_arguments() -> argparse.Namespace:
    """Parse command line arguments"""

    parser = argparse.ArgumentParser(prog='dashboard', description='Server for Home Dashboard.')
    parser.add_argument('-p', '--port', action='store', type=int, help='Port to listen on')
    parser.add_argument('-b', '--bus-stop-id', action='append', help='Bus Stop ID')
    parser.add_argument('-l', '--lat-long', action='store', help='Latitude and Longitude for weather')
    parser.add_argument('-k', '--pirate-api-key', action='store', help='API Key for Pirate Weather')
    parser.add_argument('-r', '--recycling-calendar-url', action='store', help='URL for recycling collection calendar')

    return parser.parse_args()


class Config:
    """Configuration class"""

    def __init__(self) -> None:
        self._args = _get_arguments()

    @property
    def port(self) -> int:
        """Return the port to listen on"""

        if self._args.port:
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

        if self._args.lat_long:
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
    def log_level(self) -> int:
        """Log Level"""

        log_level_name = os.environ.get('LOG_LEVEL', 'INFO')
        log_level = logging.getLevelName(log_level_name)

        if isinstance(log_level, str):
            raise ValueError(f'Invalid LOG_LEVEL: {log_level_name}')

        return log_level


CONFIG = Config()
