"""Telegram alerting for the dashboard server.

Sends a message to a Telegram bot/chat configured via environment variables.
Alerting is optional: if the bot token or chat ID is not configured, every
function here is a silent no-op and the server behaves exactly as before.

``report`` adds uptime-monitor-style deduplication on top of ``notify`` so that
a sustained outage produces a single alert (and a single recovery message),
rather than one message per failed attempt.
"""

import logging
import threading

import requests

from .config import CONFIG

logger = logging.getLogger(__name__)

# Keys currently in a failed state, guarded by a lock because the scheduler and
# the MQTT loop call report() from background threads.
_failing: set[str] = set()
_failing_lock = threading.Lock()


def notify(message: str) -> None:
    """Send *message* to the configured Telegram chat.

    No-op if Telegram is not configured. Never raises — it is called from inside
    ``except`` handlers, so a Telegram outage must not break data fetching.
    """
    token = CONFIG.telegram_bot_token
    chat_id = CONFIG.telegram_chat_id

    if not token or not chat_id:
        return

    try:
        response = requests.post(
            f'https://api.telegram.org/bot{token}/sendMessage',
            json={'chat_id': chat_id, 'text': message},
            timeout=5,
        )
        response.raise_for_status()
    except Exception:  # pylint: disable=broad-exception-caught
        logger.exception('Failed to send Telegram notification')


def report(key: str, ok: bool, detail: str = '') -> None:
    """Report the health of *key*, alerting only on state changes.

    The first failure for a key sends a ``🔴`` alert; the first success after a
    failure sends a ``✅`` recovery message. Repeated failures or repeated
    successes are silent.
    """
    with _failing_lock:
        if ok:
            if key not in _failing:
                return
            _failing.discard(key)
        else:
            if key in _failing:
                return
            _failing.add(key)

    if ok:
        notify(f'✅ {key} recovered')
    else:
        notify(f'🔴 {detail or key}')
