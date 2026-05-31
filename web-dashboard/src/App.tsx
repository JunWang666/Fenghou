import { useEffect, useMemo, useState } from "react";
import { AlertCircle, Clock, Droplets, Gauge, MapPinned, RefreshCw, Sun, Thermometer, Volume2, Zap } from "lucide-react";
import {
  CartesianGrid,
  Line,
  LineChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis
} from "recharts";
import { fetchReadings } from "./api";
import type { Reading } from "./types";

type PageMode = "dash" | "view";
type TimeWindowKey = "1h" | "6h" | "24h" | "7d" | "30d";

interface MetricConfig {
  key: string;
  label: string;
  unit: string;
  color: string;
  icon: React.ReactNode;
  precision: number;
  aggregate?: "hourly-sum";
}

const METRICS: MetricConfig[] = [
  { key: "temperature", label: "温度", unit: "°C", color: "#e11d48", icon: <Thermometer size={22} />, precision: 1 },
  { key: "humidity", label: "湿度", unit: "%", color: "#2563eb", icon: <Droplets size={22} />, precision: 1 },
  { key: "pressure", label: "气压", unit: "hPa", color: "#0f766e", icon: <Gauge size={22} />, precision: 1 },
  { key: "altitude", label: "海拔", unit: "m", color: "#7c3aed", icon: <MapPinned size={22} />, precision: 1 },
  { key: "noise_max_db", label: "每分钟噪声最大值", unit: "dB", color: "#0891b2", icon: <Volume2 size={22} />, precision: 1 },
  {
    key: "high_volume_exposure_minutes",
    label: "高音量暴露时长",
    unit: "小时",
    color: "#ea580c",
    icon: <Clock size={22} />,
    precision: 2,
    aggregate: "hourly-sum"
  },
  {
    key: "flicker_hazard_count",
    label: "频闪危害次数",
    unit: "次",
    color: "#dc2626",
    icon: <Zap size={22} />,
    precision: 0,
    aggregate: "hourly-sum"
  },
  {
    key: "sunlight_duration_minutes",
    label: "日照时长",
    unit: "小时",
    color: "#ca8a04",
    icon: <Sun size={22} />,
    precision: 2,
    aggregate: "hourly-sum"
  }
];

const TIME_WINDOWS: Array<{ key: TimeWindowKey; label: string; hours: number; limit: number }> = [
  { key: "1h", label: "1小时", hours: 1, limit: 120 },
  { key: "6h", label: "6小时", hours: 6, limit: 240 },
  { key: "24h", label: "1天", hours: 24, limit: 360 },
  { key: "7d", label: "7天", hours: 24 * 7, limit: 480 },
  { key: "30d", label: "30天", hours: 24 * 30, limit: 500 }
];

const VIEW_WINDOW_MINUTES = 5;

function routeFromLocation(): { mode: PageMode | "unknown"; deviceId: string } {
  const [, mode, deviceId] = window.location.pathname.split("/");
  if ((mode === "dash" || mode === "view") && deviceId) {
    return { mode, deviceId: decodeURIComponent(deviceId) };
  }

  return { mode: "unknown", deviceId: "" };
}

function formatTime(value: string): string {
  return new Intl.DateTimeFormat("zh-CN", {
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit"
  }).format(new Date(value));
}

function shortTime(value: string): string {
  return new Intl.DateTimeFormat("zh-CN", {
    hour: "2-digit",
    minute: "2-digit"
  }).format(new Date(value));
}

function formatValue(value: number | null | undefined, metric: MetricConfig): string {
  if (value === null || value === undefined) {
    return "--";
  }

  return `${value.toFixed(metric.precision)}${metric.unit}`;
}

function displayReading(metric: MetricConfig, readings: Reading[], series: Array<{ time: string; timestamp: number; value: number }>): Reading | undefined {
  if (metric.aggregate !== "hourly-sum") {
    return latestByMetric(readings).get(metric.key);
  }

  if (series.length === 0) {
    return undefined;
  }

  const total = series.reduce((sum, point) => sum + point.value, 0);
  const latestPoint = series[series.length - 1];
  return {
    data_id: `${metric.key}:aggregate`,
    device_id: "",
    sensor_name: metric.key,
    data: JSON.stringify(total),
    value_number: total,
    time: latestPoint.time,
    upload_time: latestPoint.time
  };
}

function latestByMetric(readings: Reading[]): Map<string, Reading> {
  const latest = new Map<string, Reading>();

  for (const reading of readings) {
    const current = latest.get(reading.sensor_name);
    if (!current || reading.time > current.time) {
      latest.set(reading.sensor_name, reading);
    }
  }

  return latest;
}

export function App() {
  const route = useMemo(routeFromLocation, []);
  const [readings, setReadings] = useState<Reading[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [loadedAt, setLoadedAt] = useState<string | null>(null);
  const [timeWindow, setTimeWindow] = useState<TimeWindowKey>("24h");

  async function loadData() {
    if (route.mode === "unknown") {
      setLoading(false);
      return;
    }

    setLoading(true);
    setError(null);

    const now = Date.now();
    const activeWindow = TIME_WINDOWS.find((option) => option.key === timeWindow) ?? TIME_WINDOWS[2];
    const from =
      route.mode === "view"
        ? new Date(now - VIEW_WINDOW_MINUTES * 60 * 1000).toISOString()
        : new Date(now - activeWindow.hours * 60 * 60 * 1000).toISOString();

    try {
      const response = await fetchReadings({
        deviceId: route.deviceId,
        sensors: METRICS.map((metric) => metric.key),
        bucket: "hour",
        from,
        limit: route.mode === "view" ? 80 : activeWindow.limit
      });

      setReadings(response.readings);
      setLoadedAt(new Date().toISOString());
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : "数据加载失败。");
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    void loadData();
  }, [timeWindow]);

  const metricSeries = useMemo(() => {
    const rawSeries = new Map<string, Array<{ time: string; timestamp: number; value: number }>>();
    const series = new Map<string, Array<{ time: string; timestamp: number; value: number }>>();

    for (const metric of METRICS) {
      rawSeries.set(metric.key, []);
    }

    for (const reading of readings) {
      if (reading.value_number === null) {
        continue;
      }

      rawSeries.get(reading.sensor_name)?.push({
        time: reading.time,
        timestamp: Date.parse(reading.time),
        value: reading.value_number
      });
    }

    for (const [key, values] of rawSeries) {
      values.sort((left, right) => left.time.localeCompare(right.time));
      series.set(key, values);
    }

    return series;
  }, [readings]);

  if (route.mode === "unknown") {
    return (
      <main className="screen centered">
        <section className="empty">
          <AlertCircle size={30} />
          <h1>缺少设备路径</h1>
          <p>请使用 /dash/设备ID 或 /view/设备ID。</p>
        </section>
      </main>
    );
  }

  const isView = route.mode === "view";
  const activeWindow = TIME_WINDOWS.find((option) => option.key === timeWindow) ?? TIME_WINDOWS[2];

  return (
    <main className={isView ? "screen view-screen" : "screen"}>
      <header className="hero">
        <div>
          <p className="eyebrow">{isView ? "实时查看" : "历史趋势"}</p>
          <h1>{route.deviceId}</h1>
          <p className="subtitle">
            {isView ? `最近 ${VIEW_WINDOW_MINUTES} 分钟内的最新读数` : `最近 ${activeWindow.label}环境、噪声与光照数据`}
          </p>
        </div>
        <div className="hero-actions">
          {!isView ? (
            <div className="time-window" aria-label="时间窗口">
              {TIME_WINDOWS.map((option) => (
                <button
                  key={option.key}
                  className={timeWindow === option.key ? "active" : ""}
                  type="button"
                  onClick={() => setTimeWindow(option.key)}
                >
                  {option.label}
                </button>
              ))}
            </div>
          ) : null}
          <button className="refresh-button" type="button" onClick={() => void loadData()} disabled={loading}>
            <RefreshCw size={18} className={loading ? "spin" : ""} />
            刷新
          </button>
        </div>
      </header>

      {error ? (
        <section className="notice">
          <AlertCircle size={18} />
          {error}
        </section>
      ) : null}

      <section className={isView ? "metric-grid view-grid" : "metric-grid dashboard-grid"}>
        {METRICS.map((metric) => {
          const series = metricSeries.get(metric.key) ?? [];
          const reading = displayReading(metric, readings, series);
          return (
            <article className={isView ? "metric-card" : "metric-card chart-card"} key={metric.key}>
              <div className="metric-card-head">
                <div className="metric-title">
                  <span className="metric-icon" style={{ color: metric.color }}>
                    {metric.icon}
                  </span>
                  <span>{metric.label}</span>
                </div>
                <small>{reading ? formatTime(reading.time) : "暂无最近数据"}</small>
              </div>
              <strong>{formatValue(reading?.value_number, metric)}</strong>
              {!isView ? <MetricChart metric={metric} series={series} loading={loading} /> : null}
            </article>
          );
        })}
      </section>

      {isView ? (
        <section className="view-status">
          <Clock size={20} />
          <span>{loadedAt ? `更新于 ${formatTime(loadedAt)}` : loading ? "正在读取最近数据" : "等待数据"}</span>
        </section>
      ) : null}
    </main>
  );
}

function MetricChart(props: {
  metric: MetricConfig;
  series: Array<{ time: string; timestamp: number; value: number }>;
  loading: boolean;
}) {
  if (props.series.length === 0) {
    return (
      <div className="metric-chart-empty">
        {props.loading ? "正在加载曲线" : "当前时间范围内没有数据"}
      </div>
    );
  }

  return (
    <div className="metric-chart">
      <ResponsiveContainer width="100%" height="100%">
        <LineChart data={props.series} margin={{ top: 10, right: 12, bottom: 4, left: 0 }}>
          <CartesianGrid strokeDasharray="3 3" stroke="#d7dee9" />
          <XAxis
            dataKey="timestamp"
            type="number"
            scale="time"
            domain={["dataMin", "dataMax"]}
            tick={{ fontSize: 11 }}
            minTickGap={24}
            stroke="#64748b"
            tickFormatter={(value) => shortTime(new Date(Number(value)).toISOString())}
          />
          <YAxis tick={{ fontSize: 11 }} stroke="#64748b" width={48} />
          <Tooltip
            cursor={{ stroke: props.metric.color, strokeWidth: 1 }}
            labelFormatter={(value) => formatTime(new Date(Number(value)).toISOString())}
            formatter={(value) => [formatValue(Number(value), props.metric), props.metric.label]}
          />
          <Line
            type="monotone"
            dataKey="value"
            name={props.metric.label}
            stroke={props.metric.color}
            strokeWidth={2.5}
            dot={{ r: 2.5, strokeWidth: 1 }}
            activeDot={{ r: 6, strokeWidth: 2, onClick: () => undefined }}
            connectNulls
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}
