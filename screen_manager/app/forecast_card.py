"""Day-ahead electricity prices from Nord Pool and ENTSO-e sensor attributes."""

import math
from datetime import date, datetime, time, timedelta, timezone

import history_card
import header_bar


MAX_POINTS = 100
SOURCE_ATTRIBUTES = ('raw_tomorrow', 'prices_tomorrow')


def _moment(value, tz):
    if isinstance(value, datetime):
        parsed = value
    elif isinstance(value, str):
        try:
            parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
        except ValueError:
            return None
    else:
        return None
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=tz or timezone.utc)
    return parsed.astimezone(timezone.utc)


def supports(attributes):
    """Whether a sensor exposes one of the supported timestamped day-ahead lists."""
    return isinstance(attributes, dict) and any(
        isinstance(attributes.get(key), list) for key in SOURCE_ATTRIBUTES
    )


def _tomorrow_window(tz, today):
    tz = tz or timezone.utc
    local_today = today or datetime.now(tz).date()
    if isinstance(local_today, datetime):
        local_today = local_today.astimezone(tz).date() if local_today.tzinfo else local_today.date()
    day = local_today + timedelta(days=1)
    start = datetime.combine(day, time.min, tzinfo=tz).astimezone(timezone.utc)
    end = datetime.combine(day + timedelta(days=1), time.min, tzinfo=tz).astimezone(timezone.utc)
    return start, end


def tomorrow(attributes, tz, today=None):
    """Normalize a Nord Pool or ENTSO-e tomorrow list to bounded UTC intervals."""
    start, end = _tomorrow_window(tz, today)
    if not isinstance(attributes, dict):
        return {'start': int(start.timestamp()), 'end': int(end.timestamp()), 'points': []}
    rows = next((attributes.get(key) for key in SOURCE_ATTRIBUTES
                 if isinstance(attributes.get(key), list)), None)
    if rows is None:
        return {'start': int(start.timestamp()), 'end': int(end.timestamp()), 'points': []}
    entries = []
    for row in rows[:MAX_POINTS + 1]:
        if not isinstance(row, dict):
            continue
        at = _moment(row.get('start', row.get('time')), tz)
        if at is None:
            continue
        price = row.get('value', row.get('price'))
        if isinstance(price, bool):
            price = None
        try:
            price = float(price)
        except (TypeError, ValueError):
            price = None
        if price is not None and not math.isfinite(price):
            price = None
        finish = _moment(row.get('end'), tz)
        entries.append([at, finish, price])
    entries.sort(key=lambda item: item[0])
    if not entries:
        return {'start': int(start.timestamp()), 'end': int(end.timestamp()), 'points': []}
    durations = [(following[0] - current[0]).total_seconds()
                 for current, following in zip(entries, entries[1:]) if following[0] > current[0]]
    default_duration = min(durations) if durations else 3600
    intervals = []
    for index, (at, finish, price) in enumerate(entries):
        if finish is None:
            finish = at + timedelta(seconds=default_duration)
        left, right = max(at, start), min(finish, end)
        if right <= left:
            continue
        if intervals and left < intervals[-1][1]:
            left = intervals[-1][1]
        if right <= left:
            continue
        intervals.append((left, right, price))
        if len(intervals) >= MAX_POINTS:
            break
    if not intervals:
        return {'start': int(start.timestamp()), 'end': int(end.timestamp()), 'points': []}
    return {
        'start': int(start.timestamp()), 'end': int(end.timestamp()),
        'points': [[int(left.timestamp()), int(right.timestamp()),
                    None if price is None else round(price, 5)]
                   for left, right, price in intervals],
    }


def message(entity, attributes, tz, today=None, clock_24h=True):
    """A bounded wire payload; empty lists remain explicit, valid forecast responses."""
    result = tomorrow(attributes, tz, today)
    values = [point[2] for point in result['points'] if point[2] is not None]
    unit = str((attributes or {}).get('unit_of_measurement') or '')[:16]
    decimals = (attributes or {}).get('suggested_display_precision')
    if not isinstance(decimals, int) or isinstance(decimals, bool):
        decimals = 3 if any(value != round(value, 2) for value in values) else 2
    decimals = min(4, max(0, decimals))
    ticks = []
    domain = []
    if values:
        domain, y_ticks = history_card.axis(min(values), max(values))
        step = y_ticks[1] - y_ticks[0] if len(y_ticks) > 1 else 1
        ticks = [[round(value, 6), header_bar.number_text(value, history_card.step_decimals(step))]
                 for value in y_ticks]
    local_tz = tz or timezone.utc
    local_day = datetime.fromtimestamp(result['start'], local_tz)
    x_ticks = []
    for hour in (6, 12, 18):
        at = local_day.replace(hour=hour)
        label = at.strftime('%H') if clock_24h else at.strftime('%I %p').lstrip('0')
        x_ticks.append([int(at.timestamp()), label])
    return {
        'v': 1, 'op': 'history', 'kind': 'step', 'entity': entity,
        'hours': 24, 'start': result['start'], 'end': result['end'],
        'points': [[at, end, value] for at, end, value in result['points']],
        'unit': unit, 'dom': [round(value, 6) for value in domain],
        'yt': ticks, 'xt': x_ticks, 'dec': decimals,
    }
