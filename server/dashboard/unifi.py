"""Query a UniFi controller for currently connected clients"""

import logging
import urllib3
import requests
from .config import CONFIG

logger = logging.getLogger(__name__)

# UniFi controllers use self-signed certs; suppress the resulting warnings globally.
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

_API_BASE = f'{CONFIG.unifi_url}/proxy/network/v2/api/site/default/clients'
_HEADERS = {'X-API-KEY': CONFIG.unifi_api_key}


def get_connected_macs() -> list[dict]:
    """Return presence info for watched clients using the UniFi Network API v1."""
    active_clients = requests.get(f'{_API_BASE}/active', headers=_HEADERS, verify=False, timeout=10)
    active_clients.raise_for_status()
    connected = {client['mac'].lower() for client in active_clients.json()}

    client_history = requests.get(f'{_API_BASE}/history?withinHours=0', headers=_HEADERS, verify=False, timeout=10)
    client_history.raise_for_status()
    last_seen = {entry['mac'].lower(): entry.get('last_seen') for entry in client_history.json()   }

    return [
        {
            'name': name,
            'connected': mac in connected,
            'last_seen': last_seen.get(mac),
        }
        for mac, name in CONFIG.unifi_client_names.items()
    ]
