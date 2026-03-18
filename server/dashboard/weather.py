"""Fetch Weather data from Pirate Weather API"""

from datetime import datetime, timezone
import json
import requests
from .config import CONFIG


def get_weather_data() -> dict:
    """Fetch temperature and chance of rain from Pirate Weather API"""

    url = f'https://api.pirateweather.net/forecast/{CONFIG.pirate_api_key}/{CONFIG.lat_long}?units=uk&exclude=minutely,alerts'
    response = requests.get(url, timeout=5)
    data = json.loads(response.content)

    now = datetime.now(timezone.utc)
    current_hour = now.replace(minute=0, second=0, microsecond=0)
    hourly = []

    for entry in data.get('hourly', {}).get('data', []):
        entry_time = datetime.fromtimestamp(entry['time'], tz=timezone.utc)
        if entry_time >= current_hour:
            hour = {
                'time': entry_time,
                'icon': entry['icon'],
                'temperature': entry['temperature'],
                'feels_like': entry['apparentTemperature'],
                'rain_chance_percent': entry['precipProbability'] * 100,
                'wind_speed': entry['windSpeed'],
                'uv_index': entry['uvIndex']
            }
            hourly.append(hour)
        if len(hourly) >= 24:
            break

    return {
        'sunrise': datetime.fromtimestamp(data['daily']['data'][0]['sunriseTime']),
        'sunset': datetime.fromtimestamp(data['daily']['data'][0]['sunsetTime']),
        'day': {
            'temperature': data['currently']['temperature'],
            'rain_chance_percent': data['currently']['precipProbability'] * 100,
            'icon': data['currently']['icon'],
            'feels_like': data['currently']['apparentTemperature'],
        },
        'hourly': hourly,
    }
