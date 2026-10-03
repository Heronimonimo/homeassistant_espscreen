import { getJson } from '../api';

export type ForecastPreview = {
  start: number;
  end: number;
  points: [number, number, number | null][];
  unit: string;
  dom: [number, number];
  yt: [number, string][];
  xt: [number, string][];
};

const cache = new Map<string, { time: number; result: Promise<ForecastPreview | null> }>();

export function loadForecast(entity: string) {
  const previous = cache.get(entity);
  if (previous && Date.now() - previous.time < 60000) return previous.result;
  const result = getJson<{ forecast: ForecastPreview | null }>(`forecast-preview?entity=${encodeURIComponent(entity)}`)
    .then((data) => data.forecast);
  cache.set(entity, { time: Date.now(), result });
  while (cache.size > 64) cache.delete(cache.keys().next().value!);
  result.catch(() => { if (cache.get(entity)?.result === result) cache.delete(entity); });
  return result;
}

export function forecastGeometry(data: ForecastPreview) {
  const known = data.points.filter(([, end, value]) => end > 0 && value !== null && Number.isFinite(value));
  if (!known.length || data.end <= data.start) return null;
  const [low, high] = data.dom;
  if (!Number.isFinite(low) || !Number.isFinite(high) || high <= low) return null;
  const x = (time: number) => 31 + (time - data.start) / (data.end - data.start) * 166;
  const y = (value: number) => 43 - (value - low) / (high - low) * 36;
  const paths: string[] = [];
  let segment = '';
  let previousEnd = -1;
  let previousValue: number | null = null;
  const flush = () => { if (segment) paths.push(segment); segment = ''; };
  for (const [start, end, value] of data.points) {
    if (value === null || !Number.isFinite(value) || end <= start) {
      flush(); previousEnd = -1; previousValue = null; continue;
    }
    if (segment && previousEnd === start && previousValue !== null) {
      segment += ` V${y(value)} H${x(end)}`;
    } else {
      flush();
      segment = `M${x(start)},${y(value)} H${x(end)}`;
    }
    previousEnd = end;
    previousValue = value;
  }
  flush();
  return {
    paths,
    yTicks: data.yt.map(([value, label]) => ({ y: y(value), label })),
    xTicks: data.xt.map(([time, label]) => ({ x: x(time), label })),
  };
}
