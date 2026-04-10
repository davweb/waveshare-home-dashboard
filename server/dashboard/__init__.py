"""Home Dashboard Server"""

from abc import ABC, abstractmethod
from datetime import datetime, timezone
import functools
import logging
import threading
import time
from typing import Any
import schedule
from . import oxontime
from . import recycling
from . import unifi
from . import weather
from .config import CONFIG
from . import mqtt_publisher


class DataSource(ABC):
    """Abstract class for data sources"""

    @abstractmethod
    def get_name(self) -> str:
        """The name of the data source, used in the JSON sent to the client"""

    @abstractmethod
    def get_data(self) -> Any:
        """Load data from the source"""

    @abstractmethod
    def format_data(self, data: Any) -> dict | list[dict]:
        """Format the data into a dictionary, run each time the JSON is sent to the client"""

    @abstractmethod
    def get_schedule(self) -> schedule.Job:
        """Return a schedule job to run the data source"""


class BusDataSource(DataSource):
    """DataSource for bus times"""

    def get_name(self) -> str:
        return 'bus_stops'

    def get_data(self) -> list[tuple[str, list[tuple[str, str, datetime]]]]:
        bus_times = []

        for bus_stop_id in CONFIG.bus_stop_ids:
            name, buses = oxontime.extract_bus_information(bus_stop_id)
            bus_times.append((name, buses))

        return bus_times

    def format_data(self, data: Any) -> Any:
        bus_times = []

        for name, buses in data:
            bus_times.append({'name': name,
                              'buses': [{'route': route,
                                         'destination': dest,
                                         'due_time': int(due.timestamp())} for route,
                                        dest,
                                        due in buses]})

        return bus_times

    def get_schedule(self) -> schedule.Job:
        return schedule.every(1).minutes


class WeatherDataSource(DataSource):
    """DataSource for weather"""

    def get_name(self) -> str:
        return 'weather'

    def get_data(self) -> dict:
        return weather.get_weather_data()

    def format_data(self, data: Any) -> dict:
        sunrise = data['sunrise']
        sunset = data['sunset']
        now = datetime.now(timezone.utc)

        if sunrise < now < sunset:
            is_sunrise = False
            event_time = sunset
        else:
            is_sunrise = True
            event_time = sunrise

        hours: list[dict[str, int | str]] = [
            {
                'hour': entry['time'].hour,
                'icon': entry.get('icon', ''),
                'temperature': round(entry['temperature']),
                'feels_like': round(entry['feels_like']),
                'precip_chance': round(entry['precip_chance_percent']),
                'wind_speed': round(entry['wind_speed']),
                'uv_index': round(entry['uv_index'])
            }
            for entry in data['hourly']
        ]

        return {
            'current': {
                'temperature': round(data['current']['temperature']),
                'feels_like': round(data['current']['feels_like']),
                'icon': data['current']['icon'],
            },
            'day': {
                'min_temperature': round(data['day']['min_temperature']),
                'max_temperature': round(data['day']['max_temperature']),
                'precip_chance': round(data['day']['precip_chance_percent']),
                'precip_type': data['day']['precip_type'],
            },
            'sun': {
                'is_sunrise': is_sunrise,
                'time_utc': int(event_time.timestamp())
            },
            'hours': hours,
        }

    def get_schedule(self) -> schedule.Job:
        return schedule.every(15).minutes


class RecyclingDataSource(DataSource):
    """DataSource to Return day and date as String"""

    def get_name(self) -> str:
        return 'recycling'

    def get_data(self) -> list[recycling.RecyclingCollection]:
        return recycling.get_next_recycling_collections()

    def format_data(self, data: list[recycling.RecyclingCollection]) -> list[dict]:
        return [
            {
                'date_epoch': int(datetime(
                    collection['date'].year,
                    collection['date'].month,
                    collection['date'].day).timestamp()),
                'type': collection['type']
            }
            for collection in data
        ]

    def get_schedule(self):
        return schedule.every().day.at('00:00')


class UnifiDataSource(DataSource):
    """DataSource for network presence via UniFi controller"""

    def get_name(self) -> str:
        return 'presence'

    def get_data(self) -> list[dict]:
        return unifi.get_connected_macs()

    def format_data(self, data: list[dict]) -> list[dict]:
        return [
            {
                'name': client['name'],
                'connected': client['connected'],
                'last_seen': 0 if client['connected'] else client['last_seen']
            }
            for client in data
        ]

    def get_schedule(self) -> schedule.Job:
        return schedule.every(1).minutes


DATA_SOURCES = [
    BusDataSource(),
    WeatherDataSource(),
    RecyclingDataSource(),
    UnifiDataSource()
]
DATA: dict[str, dict | list[dict]] = {}


def _publish_data_source(data_source: DataSource) -> None:
    name = data_source.get_name()
    if name not in DATA:
        return
    try:
        payload = data_source.format_data(DATA[name])
        mqtt_publisher.publish(name, payload)
    except Exception:  # pylint: disable=broad-exception-caught
        logging.exception('Failed to publish %s to MQTT', name)


def _update_data_source(data_source: DataSource) -> None:
    try:
        logging.info('Updating %s', data_source.get_name())
        DATA[data_source.get_name()] = data_source.get_data()
        _publish_data_source(data_source)
    except Exception:  # pylint: disable=broad-exception-caught
        logging.exception('Failed to update %s', data_source.get_name())


def run_scheduler():
    """Function to run the scheduler"""
    while True:
        schedule.run_pending()
        time.sleep(1)


def initialise_data_fetching() -> None:
    """Initialise the dashboard"""

    for data_source in DATA_SOURCES:
        update = functools.partial(_update_data_source, data_source)
        update()
        data_source.get_schedule().do(update)

    # Run the scheduler in a background thread
    scheduler_thread = threading.Thread(target=run_scheduler, daemon=True)
    scheduler_thread.start()
