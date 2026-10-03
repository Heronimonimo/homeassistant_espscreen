import sys
import unittest
from datetime import date, datetime, timedelta, timezone
from pathlib import Path
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'screen_manager/app'))
import forecast_card
import core


AMSTERDAM = ZoneInfo('Europe/Amsterdam')


class ForecastCard(unittest.TestCase):
    def test_nordpool_raw_intervals_keep_price_and_unit(self):
        tomorrow = [
            {'start': '2026-10-04T00:00:00+02:00', 'end': '2026-10-04T01:00:00+02:00', 'value': 0.12},
            {'start': '2026-10-04T01:00:00+02:00', 'end': '2026-10-04T02:00:00+02:00', 'value': 0.31},
        ]
        result = forecast_card.message(
            'sensor.power_price', {'raw_tomorrow': tomorrow, 'unit_of_measurement': 'EUR/kWh'},
            AMSTERDAM, date(2026, 10, 3))
        self.assertEqual(result['kind'], 'step')
        self.assertEqual(result['unit'], 'EUR/kWh')
        self.assertEqual([point[2] for point in result['points']], [0.12, 0.31])
        self.assertEqual(result['points'][1][0] - result['points'][0][0], 3600)
        self.assertEqual([label for _, label in result['xt']], ['06', '12', '18'])
        self.assertEqual(len(result['yt']), 2)

    def test_entsoe_quarter_hour_data_preserves_missing_interval(self):
        rows = [
            {'time': '2026-10-04T00:00:00+02:00', 'price': 12.5},
            {'time': '2026-10-04T00:15:00+02:00', 'price': 14.0},
            {'time': '2026-10-04T00:45:00+02:00', 'price': 9.0},
        ]
        result = forecast_card.message(
            'sensor.day_ahead_price', {'prices_tomorrow': rows, 'unit_of_measurement': 'EUR/MWh'},
            AMSTERDAM, date(2026, 10, 3))
        points = result['points']
        self.assertEqual([point[2] for point in points], [12.5, 14.0, 9.0])
        self.assertEqual(points[1][1], points[1][0] + 900)
        self.assertEqual(points[2][0] - points[1][1], 900)

    def test_daylight_saving_day_uses_local_midnights(self):
        rows = [
            {'time': '2026-10-25T00:00:00+02:00', 'price': 1},
            {'time': '2026-10-25T01:00:00+02:00', 'price': 2},
            {'time': '2026-10-25T02:00:00+02:00', 'price': 3},
            {'time': '2026-10-25T02:00:00+01:00', 'price': 4},
            {'time': '2026-10-25T03:00:00+01:00', 'price': 5},
        ]
        result = forecast_card.message(
            'sensor.price', {'prices_tomorrow': rows}, AMSTERDAM, date(2026, 10, 24))
        self.assertEqual(result['end'] - result['start'], 25 * 3600)
        self.assertEqual(len(result['points']), 5)
        self.assertEqual(result['points'][3][0] - result['points'][2][0], 3600)
        self.assertEqual([datetime.fromtimestamp(at, AMSTERDAM).hour for at, _ in result['xt']], [6, 12, 18])

    def test_spring_forward_day_has_23_local_hours(self):
        rows = [
            {'time': '2026-03-29T00:00:00+01:00', 'price': 1},
            {'time': '2026-03-29T01:00:00+01:00', 'price': 2},
            {'time': '2026-03-29T03:00:00+02:00', 'price': 3},
        ]
        result = forecast_card.message(
            'sensor.price', {'prices_tomorrow': rows}, AMSTERDAM, date(2026, 3, 28))
        self.assertEqual(result['end'] - result['start'], 23 * 3600)
        self.assertEqual([datetime.fromtimestamp(at, AMSTERDAM).hour for at, _ in result['xt']], [6, 12, 18])

    def test_missing_malformed_and_unavailable_data_is_empty(self):
        self.assertFalse(forecast_card.supports({'raw_tomorrow': None}))
        self.assertEqual(forecast_card.message('sensor.price', {}, AMSTERDAM, date(2026, 10, 3))['points'], [])
        result = forecast_card.message('sensor.price', {'prices_tomorrow': [
            None, {'time': 'not a timestamp', 'price': 2},
            {'time': '2026-10-04T01:00:00+02:00', 'price': float('nan')},
        ]}, AMSTERDAM, date(2026, 10, 3))
        self.assertEqual(result['points'], [[int(datetime(2026, 10, 4, 1, tzinfo=AMSTERDAM).timestamp()),
                                             int(datetime(2026, 10, 4, 2, tzinfo=AMSTERDAM).timestamp()), None]])
        self.assertEqual(result['start'], int(datetime(2026, 10, 4, tzinfo=AMSTERDAM).timestamp()))
        self.assertEqual(result['yt'], [])

    def test_points_and_encoded_message_stay_bounded(self):
        midnight = datetime(2026, 10, 4, tzinfo=AMSTERDAM)
        rows = [{'time': (midnight + timedelta(minutes=12 * index)).isoformat(), 'price': 0.12345}
                for index in range(120)]
        result = forecast_card.message(
            'sensor.price', {'prices_tomorrow': rows}, AMSTERDAM, date(2026, 10, 3))
        self.assertEqual(len(result['points']), forecast_card.MAX_POINTS)
        self.assertLessEqual(len(core.encode(result).encode()), 4096)


if __name__ == '__main__':
    unittest.main()
