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
}

export const ALL_METRICS: MetricConfig[] = [
  { key: 'temperature', label: '温度', unit: '°C', color: '#e11d48', icon: '🌡', precision: 1 },
  { key: 'humidity', label: '湿度', unit: '%', color: '#2563eb', icon: '💧', precision: 1 },
  { key: 'pressure', label: '气压', unit: 'hPa', color: '#0f766e', icon: '🔵', precision: 1 },
  { key: 'altitude', label: '海拔', unit: 'm', color: '#7c3aed', icon: '📍', precision: 1 },
  { key: 'flicker_hazard_count', label: '频闪危害次数', unit: '次', color: '#dc2626', icon: '⚡', precision: 0 },
  { key: 'sunlight_duration_minutes', label: '日照时长', unit: '分钟', color: '#ca8a04', icon: '☀', precision: 0 }
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
