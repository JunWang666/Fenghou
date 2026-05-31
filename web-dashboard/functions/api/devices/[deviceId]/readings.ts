import {
  type Env,
  jsonError,
  jsonResponse,
  normalizeDeviceId,
  normalizeIsoDate,
  normalizeLimit,
  normalizeSensorNames,
  valueNumber
} from "../../_shared";

interface ReadingRow {
  data_id: string;
  device_id: string;
  sensor_name: string;
  data: string;
  time: string;
  upload_time: string;
}

interface HourlyReadingRow {
  sensor_name: string;
  hour_time: string;
  value: number;
}

const DEFAULT_READING_LIMIT = 240;
const MAX_READING_LIMIT = 500;
const HOURLY_AGGREGATE_SENSORS = new Set([
  "high_volume_exposure_minutes",
  "flicker_hazard_count",
  "sunlight_duration_minutes"
]);

export const onRequestGet: PagesFunction<Env> = async ({ request, env, params }) => {
  const deviceId = normalizeDeviceId(String(params.deviceId ?? ""));
  if (!deviceId) {
    return jsonError("invalid_device_id", "Invalid device id.", 400);
  }

  const url = new URL(request.url);
  const sensors = normalizeSensorNames(url.searchParams.getAll("sensor"));
  if (!sensors) {
    return jsonError("invalid_sensor", "Invalid sensor name.", 400);
  }

  const from = normalizeIsoDate(url.searchParams.get("from"));
  if (url.searchParams.has("from") && !from) {
    return jsonError("invalid_from", "from must be a valid timestamp.", 400);
  }

  const to = normalizeIsoDate(url.searchParams.get("to"));
  if (url.searchParams.has("to") && !to) {
    return jsonError("invalid_to", "to must be a valid timestamp.", 400);
  }

  const limit = normalizeLimit(url.searchParams.get("limit"), DEFAULT_READING_LIMIT, MAX_READING_LIMIT);
  if (!limit) {
    return jsonError("invalid_limit", `limit must be an integer from 1 to ${MAX_READING_LIMIT}.`, 400);
  }

  const bucket = url.searchParams.get("bucket");
  if (bucket && bucket !== "hour") {
    return jsonError("invalid_bucket", "bucket must be hour when provided.", 400);
  }

  const aggregateSensors = bucket === "hour" ? sensors.filter((sensor) => HOURLY_AGGREGATE_SENSORS.has(sensor)) : [];
  const rawSensors = aggregateSensors.length > 0 ? sensors.filter((sensor) => !HOURLY_AGGREGATE_SENSORS.has(sensor)) : sensors;

  const clauses = ["device_id = ?"];
  const bindings: Array<string | number> = [deviceId];

  if (rawSensors.length > 0) {
    clauses.push(`sensor_name IN (${rawSensors.map(() => "?").join(", ")})`);
    bindings.push(...rawSensors);
  } else if (sensors.length > 0) {
    clauses.push("1 = 0");
  }

  if (from) {
    clauses.push("time >= ?");
    bindings.push(from);
  }

  if (to) {
    clauses.push("time <= ?");
    bindings.push(to);
  }

  bindings.push(limit);

  try {
    const rows: ReadingRow[] = [];
    if (rawSensors.length > 0 || sensors.length === 0) {
      const result = await env.DB.prepare(
        `SELECT data_id, device_id, sensor_name, data, time, upload_time
         FROM sensor_data
         WHERE ${clauses.join(" AND ")}
         ORDER BY time DESC
         LIMIT ?`
      )
        .bind(...bindings)
        .all<ReadingRow>();
      rows.push(...(result.results ?? []));
    }

    const readings = rows.map((row) => ({
      ...row,
      value_number: valueNumber(row.data)
    }));

    if (aggregateSensors.length > 0 && from) {
      readings.push(...(await hourlyAggregateReadings(env, deviceId, aggregateSensors, from, to)));
    }

    return jsonResponse({
      ok: true,
      device_id: deviceId,
      readings
    });
  } catch (error) {
    console.error("Failed to list readings", error);
    return jsonError("query_failed", "Failed to list readings.", 500);
  }
};

async function hourlyAggregateReadings(
  env: Env,
  deviceId: string,
  sensors: string[],
  from: string,
  to: string | null
): Promise<Array<ReadingRow & { value_number: number }>> {
  const bindings: Array<string | number> = [deviceId, ...sensors, from];
  const timeClause = to ? "AND time <= ?" : "";
  if (to) {
    bindings.push(to);
  }

  const result = await env.DB.prepare(
    `WITH ordered AS (
       SELECT
         sensor_name,
         time,
         CAST(data AS REAL) AS value,
         LAG(CAST(data AS REAL)) OVER (PARTITION BY sensor_name ORDER BY time ASC) AS previous_value
       FROM sensor_data
       WHERE device_id = ?
         AND sensor_name IN (${sensors.map(() => "?").join(", ")})
         AND time >= ?
         ${timeClause}
     ),
     deltas AS (
       SELECT
         sensor_name,
         strftime('%Y-%m-%dT%H:00:00.000Z', time) AS hour_time,
         CASE
           WHEN previous_value IS NULL THEN 0
           WHEN value >= previous_value THEN value - previous_value
           ELSE 0
         END AS delta_value
       FROM ordered
     )
     SELECT
       sensor_name,
       hour_time,
       SUM(
         CASE
           WHEN sensor_name IN ('high_volume_exposure_minutes', 'sunlight_duration_minutes') THEN delta_value / 60.0
           ELSE delta_value
         END
       ) AS value
     FROM deltas
     GROUP BY sensor_name, hour_time
     ORDER BY hour_time DESC`
  )
    .bind(...bindings)
    .all<HourlyReadingRow>();

  return (result.results ?? []).map((row) => ({
    data_id: `${row.sensor_name}:${row.hour_time}`,
    device_id: deviceId,
    sensor_name: row.sensor_name,
    data: JSON.stringify(row.value),
    value_number: row.value,
    time: row.hour_time,
    upload_time: row.hour_time
  }));
}
