export interface Reading {
  data_id: string;
  device_id: string;
  sensor_name: string;
  data: string;
  value_number: number | null;
  time: string;
  upload_time: string;
}

export interface ReadingsResponse {
  ok: boolean;
  device_id: string;
  readings: Reading[];
}

export interface ApiErrorDetail {
  code: string;
  message: string;
}

export interface MetricConfig {
  key: string;
  label: string;
  unit: string;
  color: string;
  icon: string;
  precision: number;
  aggregate: string;
}

export const ALL_METRICS: MetricConfig[] = [
  { key: 'temperature', label: '温度', unit: '°C', color: '#e11d48', icon: '🌡', precision: 1, aggregate: '' },
  { key: 'humidity', label: '湿度', unit: '%', color: '#2563eb', icon: '💧', precision: 1, aggregate: '' },
  { key: 'pressure', label: '气压', unit: 'hPa', color: '#0f766e', icon: '🔵', precision: 1, aggregate: '' },
  { key: 'altitude', label: '海拔', unit: 'm', color: '#7c3aed', icon: '📍', precision: 1, aggregate: '' },
  { key: 'noise_max_db', label: '每分钟噪声最大值', unit: 'dB', color: '#0891b2', icon: '🔊', precision: 1, aggregate: '' },
  { key: 'high_volume_exposure_minutes', label: '高音量暴露时长', unit: '小时', color: '#ea580c', icon: '⏱', precision: 2, aggregate: 'hourly-sum' },
  { key: 'flicker_hazard_count', label: '频闪危害次数', unit: '次', color: '#dc2626', icon: '⚡', precision: 0, aggregate: 'hourly-sum' },
  { key: 'sunlight_duration_minutes', label: '日照时长', unit: '分钟', color: '#ca8a04', icon: '☀', precision: 0, aggregate: 'hourly-sum' }
];

export interface TimeWindow {
  key: string;
  label: string;
  hours: number;
  limit: number;
}

export const TIME_WINDOWS: TimeWindow[] = [
  { key: '1h', label: '1小时', hours: 1, limit: 120 },
  { key: '6h', label: '6小时', hours: 6, limit: 240 },
  { key: '24h', label: '1天', hours: 24, limit: 360 },
  { key: '7d', label: '7天', hours: 168, limit: 480 },
  { key: '30d', label: '30天', hours: 720, limit: 500 }
];

export const VIEW_WINDOW_MINUTES: number = 5;

export function formatMetricTime(isoTime: string): string {
  const date: Date = new Date(isoTime);
  const month: string = (date.getMonth() + 1).toString().padStart(2, '0');
  const day: string = date.getDate().toString().padStart(2, '0');
  const hour: string = date.getHours().toString().padStart(2, '0');
  const minute: string = date.getMinutes().toString().padStart(2, '0');
  const second: string = date.getSeconds().toString().padStart(2, '0');
  return `${month}-${day} ${hour}:${minute}:${second}`;
}

export function formatShortTime(isoTime: string): string {
  const date: Date = new Date(isoTime);
  const hour: string = date.getHours().toString().padStart(2, '0');
  const minute: string = date.getMinutes().toString().padStart(2, '0');
  return `${hour}:${minute}`;
}

export function formatValue(value: number | null | undefined, precision: number, unit: string): string {
  if (value === null || value === undefined) {
    return '--';
  }
  return `${value.toFixed(precision)}${unit}`;
}

export interface ChartPoint {
  time: string;
  timestamp: number;
  value: number;
}

export function buildSeries(readings: Reading[], metricKey: string): ChartPoint[] {
  const points: ChartPoint[] = [];
  for (const r of readings) {
    if (r.sensor_name === metricKey && r.value_number !== null) {
      points.push({
        time: r.time,
        timestamp: new Date(r.time).getTime(),
        value: r.value_number
      });
    }
  }
  points.sort((a: ChartPoint, b: ChartPoint) => a.timestamp - b.timestamp);
  return points;
}

export function getLatestReading(readings: Reading[], metricKey: string): Reading | null {
  let latest: Reading | null = null;
  for (const r of readings) {
    if (r.sensor_name === metricKey) {
      if (!latest || r.time > latest.time) {
        latest = r;
      }
    }
  }
  return latest;
}

export function getDisplayValue(metric: MetricConfig, readings: Reading[], series: ChartPoint[]): number | null {
  if (metric.aggregate !== 'hourly-sum') {
    const latest: Reading | null = getLatestReading(readings, metric.key);
    return latest ? latest.value_number : null;
  }
  if (series.length === 0) {
    return null;
  }
  let total: number = 0;
  for (const point of series) {
    total += point.value;
  }
  return total;
}

export function getDisplayTime(metric: MetricConfig, readings: Reading[], series: ChartPoint[]): string {
  if (metric.aggregate !== 'hourly-sum') {
    const latest: Reading | null = getLatestReading(readings, metric.key);
    return latest ? latest.time : '';
  }
  if (series.length === 0) {
    return '';
  }
  return series[series.length - 1].time;
}
