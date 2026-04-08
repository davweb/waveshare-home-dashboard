"""Get next recycling collection from Council ical"""

from datetime import date, datetime, timedelta
from typing import TypedDict
from icalendar import Calendar
import recurring_ical_events
import requests
from .config import CONFIG

RecyclingCollection = TypedDict('RecyclingCollection', {'date': date, 'type': str})


def get_next_recycling_collections(count: int = 4) -> list[RecyclingCollection]:
    """Return the next recycling collections"""

    response = requests.get(CONFIG.recycling_calendar_url, timeout=5)
    calendar = Calendar.from_ical(response.text)
    start_date = date.today()
    end_date = start_date + timedelta(days=90)
    events = recurring_ical_events.of(calendar).between(start_date, end_date)
    collections = []

    for event in events:
        dtstart = event.get('DTSTART').dt  # type: ignore[no-untyped-call]

        # Check if it's a date object (all-day events store only dates, not datetimes)
        if isinstance(dtstart, date) and not isinstance(dtstart, datetime):
            summary = event.get('SUMMARY').to_ical().decode('utf-8').strip()  # type: ignore[no-untyped-call]
            collection_type = 'Recycling' if 'RECYCLING' in summary else 'Rubbish'
            collection: RecyclingCollection = {
                'date': dtstart,
                'type': collection_type
            }
            collections.append(collection)

    collections.sort(key=lambda event: event['date'])
    return collections[:count]
