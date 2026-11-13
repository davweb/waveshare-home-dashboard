"""Telegram alerting for the dashboard server.

Sends a message to a Telegram bot/chat configured via environment variables.
Alerting is optional: if the bot token or chat ID is not configured, every
function here is a silent no-op and the server behaves exactly as before.

``report`` adds uptime-monitor-style deduplication on top of ``notify`` so that
a sustained outage produces a single alert (and a single recovery message),
rather than one message per failed attempt. A per-key threshold delays the
alert until that many consecutive failures, so brief blips stay silent.
"""

import logging
import threading

import requests

from .config import CONFIG

logger = logging.getLogger(__name__)

# Consecutive failure counts and keys that have been alerted on, guarded by a
# lock because the scheduler and the MQTT loop call report() from background
# threads.
_consecutive_failures: dict[str, int] = {}
_alerted: set[str] = set()
_state_lock = threading.Lock()


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


def report(key: str, ok: bool, detail: str = '', threshold: int = 1) -> None:
    """Report the health of *key*, alerting only on state changes.

    A ``🔴`` alert is sent when *key* reaches *threshold* consecutive failures;
    the first success after that alert sends a ``✅`` recovery message. Failures
    below the threshold, repeated failures after the alert, and successes with
    no outstanding alert are silent.
    """
    with _state_lock:
        if ok:
            _consecutive_failures.pop(key, None)
            if key not in _alerted:
                return
            _alerted.discard(key)
        else:
            failures = _consecutive_failures.get(key, 0) + 1
            _consecutive_failures[key] = failures
            if failures < threshold or key in _alerted:
                return
            _alerted.add(key)

    if ok:
        notify(f'✅ {key} recovered')
    elif threshold > 1:
        notify(f'🔴 {detail or key} ({threshold} in a row)')
    else:
        notify(f'🔴 {detail or key}')
