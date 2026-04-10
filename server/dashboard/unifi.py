"""Query a UniFi controller for currently connected clients"""

import logging
import urllib3
import requests
from .config import CONFIG

logger = logging.getLogger(__name__)

# UniFi controllers use self-signed certs; suppress the resulting warnings globally.
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

def get_connected_macs() -> list[dict]:
    """Return the set of MAC addresses currently connected to the network.

    Tries the legacy standalone controller endpoint first, then falls back to
    UniFi OS (UDM / UDM-Pro / Cloud Key Gen2).
    SSL verification is disabled because UniFi controllers use self-signed certs.
    """

    session = requests.Session()
    credentials = {'username': CONFIG.unifi_username, 'password': CONFIG.unifi_password}

    # Legacy standalone UniFi controller
    login_resp = session.post(f'{CONFIG.unifi_url}/api/login', json=credentials, verify=False, timeout=10)

    if login_resp.ok:
        api_base = f'{CONFIG.unifi_url}/api/s/{CONFIG.unifi_site}'
    else:
        # UniFi OS (UDM, UDM-Pro, UCK-G2-Plus running UniFi OS)
        login_resp = session.post(f'{CONFIG.unifi_url}/api/auth/login', json=credentials, verify=False, timeout=10)
        login_resp.raise_for_status()
        api_base = f'{CONFIG.unifi_url}/proxy/network/api/s/{CONFIG.unifi_site}'

    sta_resp = session.get(f'{api_base}/stat/sta', verify=False, timeout=10)
    sta_resp.raise_for_status()
    connected_macs = {client['mac'].lower() for client in sta_resp.json().get('data', [])}

    all_resp = session.get(f'{api_base}/stat/alluser', verify=False, timeout=10)
    all_resp.raise_for_status()
    last_seen = {client['mac'].lower(): client.get('last_seen') for client in all_resp.json().get('data', [])}

    return [
        {
            'name': name,
            'connected': mac in connected_macs,
            'last_seen': last_seen.get(mac)
        }
        for mac, name in CONFIG.unifi_client_names.items()
    ]
