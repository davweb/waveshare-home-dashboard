from datetime import date
from icalendar import Event, Component

class UnfoldableCalendar:
    def between(self, start: tuple[int, int, int] | date, stop: tuple[int, int, int] | date) -> list[Event]: ...

def of(calendar: Component) -> UnfoldableCalendar: ...
