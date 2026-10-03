import { describe, expect, it } from 'vitest';
import { forecastGeometry, type ForecastPreview } from '../src/model/forecast-preview';

const sample: ForecastPreview = {
  start: 0, end: 240, unit: 'EUR/kWh', dom: [0, 4],
  points: [[0, 60, 1], [60, 120, 3], [180, 240, 2]],
  yt: [[0, '0'], [2, '2'], [4, '4']], xt: [[60, '06'], [120, '12'], [180, '18']],
};

describe('day-ahead forecast geometry', () => {
  it('draws constant-price steps and leaves unavailable intervals open', () => {
    const result = forecastGeometry(sample)!;
    expect(result.paths).toHaveLength(2);
    expect(result.paths[0]).toContain('H72.5 V');
    expect(result.paths[1]).toContain('M155.5');
    expect(result.xTicks).toEqual([
      { x: 72.5, label: '06' }, { x: 114, label: '12' }, { x: 155.5, label: '18' },
    ]);
  });

  it('treats zero as a price and returns no graph for a forecast with no values', () => {
    expect(forecastGeometry({ ...sample, points: [[0, 240, 0]], dom: [-1, 1] })?.paths).toHaveLength(1);
    expect(forecastGeometry({ ...sample, points: [[0, 240, null]] })).toBeNull();
  });
});
