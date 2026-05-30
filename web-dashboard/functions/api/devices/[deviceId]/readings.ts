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

  const limit = normalizeLimit(url.searchParams.get("limit"), 500, 2000);
  if (!limit) {
    return jsonError("invalid_limit", "limit must be an integer from 1 to 2000.", 400);
  }

  const clauses = ["device_id = ?"];
  const bindings: Array<string | number> = [deviceId];

  if (sensors.length > 0) {
    clauses.push(`sensor_name IN (${sensors.map(() => "?").join(", ")})`);
    bindings.push(...sensors);
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
    const result = await env.DB.prepare(
      `SELECT data_id, device_id, sensor_name, data, time, upload_time
       FROM sensor_data
       WHERE ${clauses.join(" AND ")}
       ORDER BY time DESC
       LIMIT ?`
    )
      .bind(...bindings)
      .all<ReadingRow>();

    const readings = (result.results ?? []).map((row) => ({
      ...row,
      value_number: valueNumber(row.data)
    }));

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
