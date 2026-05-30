import { type Env, jsonError, jsonResponse, normalizeDeviceId } from "../../_shared";

interface SensorRow {
  sensor_name: string;
}

export const onRequestGet: PagesFunction<Env> = async ({ env, params }) => {
  const deviceId = normalizeDeviceId(String(params.deviceId ?? ""));
  if (!deviceId) {
    return jsonError("invalid_device_id", "Invalid device id.", 400);
  }

  try {
    const result = await env.DB.prepare(
      `SELECT DISTINCT sensor_name
       FROM sensor_data
       WHERE device_id = ?1
       ORDER BY sensor_name ASC`
    )
      .bind(deviceId)
      .all<SensorRow>();

    return jsonResponse({
      ok: true,
      device_id: deviceId,
      sensors: result.results ?? []
    });
  } catch (error) {
    console.error("Failed to list sensors", error);
    return jsonError("query_failed", "Failed to list sensors.", 500);
  }
};
