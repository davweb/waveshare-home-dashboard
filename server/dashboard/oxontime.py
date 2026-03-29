"""Fetch bus time by scraping https://oxontime.com/"""

from datetime import datetime, timezone
from functools import cache
import json
import requests
from dateutil import parser, tz
from .config import CONFIG

_LONDON_TZ = tz.gettz('Europe/London')
_LOCATIONS_URL = 'https://oxontime.com/pwi/getShareLocations'
_TIMES_URL = 'https://oxontime.com/pwi/departureBoard/{}'


@cache
def _bus_stop_name(bus_stop_id) -> str:
    overrides = CONFIG.bus_stop_names

    if bus_stop_id in overrides:
        return overrides[bus_stop_id]

    page = requests.get(_LOCATIONS_URL, timeout=60)
    locations = json.loads(page.content)

    for location in locations:
        if location['location_code'] == bus_stop_id:
            return location['location_name']

    return f'Bus Stop {bus_stop_id}'


def _to_utc(time_str: str) -> datetime:
    dt = parser.parse(time_str)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=_LONDON_TZ)
    return dt.astimezone(timezone.utc)


def _bus_details(bus) -> tuple[str, str, datetime]:
    route = bus['route_code']
    destination = bus['destination']
    departure_time = _to_utc(bus['expected_arrival_time'] or bus['aimed_arrival_time'])

    return (route, destination, departure_time)


def extract_bus_information(bus_stop_id) -> tuple[str, datetime, list[tuple[str, str, datetime]]]:
    """Download bus time information page and return the data"""

    url = _TIMES_URL.format(bus_stop_id)
    page = requests.get(url, timeout=60)
    data = json.loads(page.content)[bus_stop_id]

    stop_name = _bus_stop_name(bus_stop_id)
    refresh_time = _to_utc(data['time'])
    buses = [_bus_details(bus) for bus in data['calls']]

    return (stop_name, refresh_time, buses)
