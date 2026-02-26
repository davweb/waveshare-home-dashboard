"""Fetch Weather data from Pirate Weather API"""

from datetime import datetime
import json
import requests
from .config import CONFIG


def get_weather_data() -> dict:
    """Fetch temperature and chance of rain from Pirate Weather API"""

    url = f'https://api.pirateweather.net/forecast/{CONFIG.pirate_api_key}/{
        CONFIG.lat_long}?units=uk,exclude=hourly,minutely,alerts'
    response = requests.get(url, timeout=5)
    data = json.loads(response.content)

    return {
        'temperature': data['currently']['temperature'],
        'rain': data['currently']['precipProbability'],
        'sunrise': datetime.fromtimestamp(data['daily']['data'][0]['sunriseTime']),
        'sunset': datetime.fromtimestamp(data['daily']['data'][0]['sunsetTime']),
        'icon': data['currently']['icon'],
        'feels_like': data['currently']['apparentTemperature'],
    }
