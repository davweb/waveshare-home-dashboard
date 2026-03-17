"""Home Dashboard Server"""

from abc import ABC, abstractmethod
from datetime import datetime
import functools
import logging
import threading
import time
from typing import Any
import schedule
from . import oxontime
from . import recycling
from . import weather
from .config import CONFIG


class DataSource(ABC):
    """Abstract class for data sources"""

    @abstractmethod
    def get_name(self) -> str:
        """The name of the data source, used in the JSON sent to the client"""

    @abstractmethod
    def get_data(self) -> Any:
        """Load data from the source"""

    @abstractmethod
    def format_data(self, data: Any) -> dict:
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
            name, _, buses = oxontime.extract_bus_information(bus_stop_id)
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
        now = datetime.now()

        if sunrise < now < sunset:
            event_name = 'Sunset'
            event_time = sunset
        else:
            event_name = 'Sunrise'
            event_time = sunrise

        hours = [
            {
                'hour': datetime.fromtimestamp(entry['time']).hour,
                'icon': entry.get('icon', ''),
                'temperature': round(entry.get('temperature', 0)),
                'feels_like': round(entry.get('apparentTemperature', 0)),
                'rain_chance': round(entry.get('precipProbability', 0) * 100),
                'wind_speed': round(entry.get('windSpeed', 0)),
                'uv_index': round(entry.get('uvIndex', 0)),
            }
            for entry in data.get('hourly', [])
        ]

        return {
            'day': {
                'temperature': round(data["temperature"]),
                'feels_like': round(data["feels_like"]),
                'icon': data['icon'],
                'rain_chance': round(data["rain"] * 100),
            },
            'sun': {
                'event': event_name,
                'time': event_time.strftime('%-H:%M')
            },
            'hours': hours,
        }

    def get_schedule(self) -> schedule.Job:
        return schedule.every(15).minutes


class RecyclingDataSource(DataSource):
    """DataSource to Return day and date as String"""

    def get_name(self) -> str:
        return 'recycling'

    def get_data(self) -> recycling.RecyclingCollection:
        return recycling.get_next_recycling_collection()

    def format_data(self, data: dict) -> dict:
        epoch = int(datetime(data['date'].year, data['date'].month, data['date'].day).timestamp())
        return {
            'date_epoch': epoch,
            'type': data['type']
        }

    def get_schedule(self):
        return schedule.every().day.at('00:00')


DATA_SOURCES = [
    BusDataSource(),
    WeatherDataSource(),
    RecyclingDataSource()
]
DATA = {}


def _update_data_source(data_source):
    try:
        logging.info('Updating %s', data_source.get_name())
        DATA[data_source.get_name()] = data_source.get_data()
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
        source = data_source
        update = functools.partial(_update_data_source, source)
        update()
        data_source.get_schedule().do(update)

    # Run the scheduler in a background thread
    scheduler_thread = threading.Thread(target=run_scheduler, daemon=True)
    scheduler_thread.start()


def get_data() -> dict:
    """Return the data to be displayed on the dashboard"""
    result = {}

    for data_source in DATA_SOURCES:
        name = data_source.get_name()
        result[name] = data_source.format_data(DATA[name])
        if name in DATA:
            result[name] = data_source.format_data(DATA[name])

    return result
