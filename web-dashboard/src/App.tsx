import { useCallback, useEffect, useMemo, useState } from "react";
import {
  Activity,
  AlertCircle,
  BarChart3,
  Check,
  Clock3,
  Database,
  RefreshCw,
  Router,
  Search,
  Wifi
} from "lucide-react";
import {
  CartesianGrid,
  Legend,
  Line,
  LineChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis
} from "recharts";
import { fetchDevices, fetchReadings, fetchSensors } from "./api";
import type { Device, Reading, Sensor } from "./types";

type RangeKey = "1h" | "6h" | "24h" | "7d" | "custom";

const RANGE_OPTIONS: Array<{ key: RangeKey; label: string; hours?: number }> = [
  { key: "1h", label: "1 小时", hours: 1 },
  { key: "6h", label: "6 小时", hours: 6 },
  { key: "24h", label: "24 小时", hours: 24 },
  { key: "7d", label: "7 天", hours: 24 * 7 },
  { key: "custom", label: "自定义" }
];

const SENSOR_COLORS = ["#2563eb", "#059669", "#dc2626", "#7c3aed", "#d97706", "#0891b2", "#be123c"];

function toDateTimeLocal(date: Date): string {
  const offsetMs = date.getTimezoneOffset() * 60_000;
  return new Date(date.getTime() - offsetMs).toISOString().slice(0, 16);
}

function fromDateTimeLocal(value: string): string | undefined {
  if (!value) {
    return undefined;
  }

  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? undefined : date.toISOString();
}

function displayTime(value: string): string {
  return new Intl.DateTimeFormat("zh-CN", {
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit"
  }).format(new Date(value));
}

function compactTime(value: string): string {
  return new Intl.DateTimeFormat("zh-CN", {
    hour: "2-digit",
    minute: "2-digit"
  }).format(new Date(value));
}

function displayData(raw: string): string {
  try {
    const value: unknown = JSON.parse(raw);
    return typeof value === "string" ? value : JSON.stringify(value);
  } catch {
    return raw;
  }
}

function selectedRange(range: RangeKey, customFrom: string, customTo: string): { from?: string; to?: string } {
  if (range === "custom") {
    return {
      from: fromDateTimeLocal(customFrom),
      to: fromDateTimeLocal(customTo)
    };
  }

  const option = RANGE_OPTIONS.find((item) => item.key === range);
  if (!option?.hours) {
    return {};
  }

  return {
    from: new Date(Date.now() - option.hours * 60 * 60 * 1000).toISOString()
  };
}

export function App() {
  const [devices, setDevices] = useState<Device[]>([]);
  const [selectedDeviceId, setSelectedDeviceId] = useState("");
  const [sensors, setSensors] = useState<Sensor[]>([]);
  const [selectedSensors, setSelectedSensors] = useState<string[]>([]);
  const [readings, setReadings] = useState<Reading[]>([]);
  const [range, setRange] = useState<RangeKey>("24h");
  const [customFrom, setCustomFrom] = useState(() => toDateTimeLocal(new Date(Date.now() - 24 * 60 * 60 * 1000)));
  const [customTo, setCustomTo] = useState(() => toDateTimeLocal(new Date()));
  const [loadingDevices, setLoadingDevices] = useState(true);
  const [loadingData, setLoadingData] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const selectedDevice = useMemo(
    () => devices.find((device) => device.device_id === selectedDeviceId) ?? null,
    [devices, selectedDeviceId]
  );

  const loadDevices = useCallback(async () => {
    setLoadingDevices(true);
    setError(null);

    try {
      const response = await fetchDevices();
      setDevices(response.devices);
      setSelectedDeviceId((current) => current || response.devices[0]?.device_id || "");
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : "设备列表加载失败。");
    } finally {
      setLoadingDevices(false);
    }
  }, []);

  const loadDeviceData = useCallback(async () => {
    if (!selectedDeviceId) {
      setSensors([]);
      setSelectedSensors([]);
      setReadings([]);
      return;
    }

    setLoadingData(true);
    setError(null);

    try {
      const sensorResponse = await fetchSensors(selectedDeviceId);
      const nextSensors = sensorResponse.sensors;
      setSensors(nextSensors);

      const nextSelected = selectedSensors.length
        ? selectedSensors.filter((sensor) => nextSensors.some((item) => item.sensor_name === sensor))
        : nextSensors.slice(0, 3).map((sensor) => sensor.sensor_name);

      setSelectedSensors(nextSelected);

      if (nextSelected.length === 0) {
        setReadings([]);
        return;
      }

      const window = selectedRange(range, customFrom, customTo);
      const readingResponse = await fetchReadings({
        deviceId: selectedDeviceId,
        sensors: nextSelected,
        from: window.from,
        to: window.to,
        limit: 1000
      });
      setReadings(readingResponse.readings);
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : "设备数据加载失败。");
    } finally {
      setLoadingData(false);
    }
  }, [customFrom, customTo, range, selectedDeviceId, selectedSensors]);

  useEffect(() => {
    void loadDevices();
  }, [loadDevices]);

  useEffect(() => {
    void loadDeviceData();
  }, [selectedDeviceId, range, customFrom, customTo]);

  useEffect(() => {
    if (!selectedDeviceId) {
      return;
    }

    const window = selectedRange(range, customFrom, customTo);
    setLoadingData(true);
    setError(null);

    fetchReadings({
      deviceId: selectedDeviceId,
      sensors: selectedSensors,
      from: window.from,
      to: window.to,
      limit: 1000
    })
      .then((response) => setReadings(response.readings))
      .catch((caught) => setError(caught instanceof Error ? caught.message : "设备数据加载失败。"))
      .finally(() => setLoadingData(false));
  }, [customFrom, customTo, range, selectedDeviceId, selectedSensors]);

  const chartData = useMemo(() => {
    const byTime = new Map<string, Record<string, number | string>>();

    for (const reading of readings) {
      if (reading.value_number === null) {
        continue;
      }

      const point = byTime.get(reading.time) ?? {
        time: reading.time,
        label: compactTime(reading.time)
      };

      point[reading.sensor_name] = reading.value_number;
      byTime.set(reading.time, point);
    }

    return Array.from(byTime.values()).sort((left, right) => String(left.time).localeCompare(String(right.time)));
  }, [readings]);

  const recentReadings = useMemo(
    () => [...readings].sort((left, right) => right.time.localeCompare(left.time)).slice(0, 80),
    [readings]
  );

  const numericReadingCount = readings.filter((reading) => reading.value_number !== null).length;

  function toggleSensor(sensorName: string) {
    setSelectedSensors((current) => {
      if (current.includes(sensorName)) {
        return current.filter((sensor) => sensor !== sensorName);
      }

      return [...current, sensorName];
    });
  }

  function refreshAll() {
    void loadDevices();
    void loadDeviceData();
  }

  return (
    <main className="dashboard-shell">
      <header className="topbar">
        <div className="brand-block">
          <div className="brand-icon" aria-hidden="true">
            <BarChart3 size={24} />
          </div>
          <div>
            <h1>Fenghou 设备数据</h1>
            <p>Cloudflare Pages dashboard</p>
          </div>
        </div>

        <button className="icon-button primary" type="button" onClick={refreshAll} disabled={loadingDevices || loadingData}>
          <RefreshCw size={18} className={loadingDevices || loadingData ? "spin" : ""} />
          <span>刷新</span>
        </button>
      </header>

      <section className="controls-band">
        <label className="field">
          <span>
            <Router size={15} />
            设备
          </span>
          <select value={selectedDeviceId} onChange={(event) => setSelectedDeviceId(event.target.value)}>
            {devices.length === 0 ? <option value="">暂无设备</option> : null}
            {devices.map((device) => (
              <option key={device.device_id} value={device.device_id}>
                {device.device_id}
              </option>
            ))}
          </select>
        </label>

        <div className="segmented" aria-label="时间范围">
          {RANGE_OPTIONS.map((option) => (
            <button
              key={option.key}
              className={range === option.key ? "active" : ""}
              type="button"
              onClick={() => setRange(option.key)}
            >
              {option.label}
            </button>
          ))}
        </div>

        {range === "custom" ? (
          <div className="custom-range">
            <label className="field compact">
              <span>开始</span>
              <input type="datetime-local" value={customFrom} onChange={(event) => setCustomFrom(event.target.value)} />
            </label>
            <label className="field compact">
              <span>结束</span>
              <input type="datetime-local" value={customTo} onChange={(event) => setCustomTo(event.target.value)} />
            </label>
          </div>
        ) : null}
      </section>

      {error ? (
        <section className="notice error">
          <AlertCircle size={18} />
          <span>{error}</span>
        </section>
      ) : null}

      {loadingDevices ? (
        <section className="empty-state">
          <RefreshCw size={28} className="spin" />
          <strong>正在加载设备</strong>
        </section>
      ) : devices.length === 0 ? (
        <section className="empty-state">
          <Database size={30} />
          <strong>没有设备数据</strong>
          <span>设备上传数据后会显示在这里。</span>
        </section>
      ) : (
        <>
          <section className="metrics-grid">
            <Metric icon={<Wifi size={18} />} label="当前设备" value={selectedDevice?.device_id ?? "-"} />
            <Metric
              icon={<Clock3 size={18} />}
              label="最后上传"
              value={selectedDevice ? displayTime(selectedDevice.last_upload_time) : "-"}
            />
            <Metric icon={<Activity size={18} />} label="上传次数" value={String(selectedDevice?.upload_count ?? 0)} />
            <Metric icon={<Search size={18} />} label="传感器" value={String(sensors.length)} />
          </section>

          <section className="workbench">
            <aside className="sensor-panel">
              <div className="section-heading">
                <h2>传感器</h2>
                <span>{selectedSensors.length} / {sensors.length}</span>
              </div>

              {sensors.length === 0 ? (
                <div className="panel-empty">该设备还没有传感器记录。</div>
              ) : (
                <div className="sensor-list">
                  {sensors.map((sensor) => {
                    const checked = selectedSensors.includes(sensor.sensor_name);
                    return (
                      <button
                        key={sensor.sensor_name}
                        className={`sensor-toggle ${checked ? "selected" : ""}`}
                        type="button"
                        onClick={() => toggleSensor(sensor.sensor_name)}
                      >
                        <span className="check-box">{checked ? <Check size={14} /> : null}</span>
                        <span>{sensor.sensor_name}</span>
                      </button>
                    );
                  })}
                </div>
              )}
            </aside>

            <section className="chart-area">
              <div className="section-heading">
                <div>
                  <h2>趋势图</h2>
                  <span>{numericReadingCount} 个数值点</span>
                </div>
                {loadingData ? <RefreshCw size={18} className="spin muted" /> : null}
              </div>

              {selectedSensors.length === 0 ? (
                <div className="chart-empty">选择至少一个传感器。</div>
              ) : chartData.length === 0 ? (
                <div className="chart-empty">当前范围内没有可绘制的数值数据。</div>
              ) : (
                <div className="chart-frame">
                  <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={chartData} margin={{ top: 8, right: 16, bottom: 8, left: 0 }}>
                      <CartesianGrid strokeDasharray="3 3" stroke="#d7dee9" />
                      <XAxis dataKey="label" tick={{ fontSize: 12 }} minTickGap={28} stroke="#64748b" />
                      <YAxis tick={{ fontSize: 12 }} stroke="#64748b" width={52} />
                      <Tooltip
                        labelFormatter={(_, payload) => {
                          const time = payload?.[0]?.payload?.time;
                          return time ? displayTime(String(time)) : "";
                        }}
                      />
                      <Legend />
                      {selectedSensors.map((sensor, index) => (
                        <Line
                          key={sensor}
                          type="monotone"
                          dataKey={sensor}
                          stroke={SENSOR_COLORS[index % SENSOR_COLORS.length]}
                          strokeWidth={2}
                          dot={false}
                          connectNulls
                        />
                      ))}
                    </LineChart>
                  </ResponsiveContainer>
                </div>
              )}
            </section>
          </section>

          <section className="table-section">
            <div className="section-heading">
              <h2>最近数据</h2>
              <span>{recentReadings.length} 条</span>
            </div>

            {recentReadings.length === 0 ? (
              <div className="panel-empty">当前范围内没有数据。</div>
            ) : (
              <div className="table-wrap">
                <table>
                  <thead>
                    <tr>
                      <th>时间</th>
                      <th>传感器</th>
                      <th>数据</th>
                      <th>上传时间</th>
                    </tr>
                  </thead>
                  <tbody>
                    {recentReadings.map((reading) => (
                      <tr key={reading.data_id}>
                        <td>{displayTime(reading.time)}</td>
                        <td>{reading.sensor_name}</td>
                        <td className="data-cell">{displayData(reading.data)}</td>
                        <td>{displayTime(reading.upload_time)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </section>
        </>
      )}
    </main>
  );
}

function Metric(props: { icon: React.ReactNode; label: string; value: string }) {
  return (
    <article className="metric">
      <div className="metric-icon">{props.icon}</div>
      <div>
        <span>{props.label}</span>
        <strong>{props.value}</strong>
      </div>
    </article>
  );
}
