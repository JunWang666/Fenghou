import { type Env, jsonError, jsonResponse } from "./_shared";

interface DeviceRow {
  device_id: string;
  first_seen_time: string;
  last_upload_time: string;
  upload_count: number;
}

export const onRequestGet: PagesFunction<Env> = async ({ env }) => {
  try {
    const result = await env.DB.prepare(
      `SELECT device_id, first_seen_time, last_upload_time, upload_count
       FROM device
       ORDER BY last_upload_time DESC`
    ).all<DeviceRow>();

    return jsonResponse({
      ok: true,
      devices: result.results ?? []
    });
  } catch (error) {
    console.error("Failed to list devices", error);
    return jsonError("query_failed", "Failed to list devices.", 500);
  }
};
