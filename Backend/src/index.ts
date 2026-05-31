interface Env {
  DB: D1Database;
}

type JsonObject = Record<string, unknown>;

const DEVICE_ID_MAX_LENGTH = 128;
const DEVICE_ID_PATTERN = /^[A-Za-z0-9._:-]+$/;
const SENSOR_NAME_MAX_LENGTH = 128;
const SENSOR_NAME_PATTERN = /^[A-Za-z0-9._:-]+$/;

interface DeviceUploadPayload {
  deviceId: string;
  sensorData: Array<{
    name: string;
    data: string;
  }>;
  recordedAt: string;
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    try {
      const url = new URL(request.url);

      if (request.method === "OPTIONS") {
        return new Response(null, { status: 204, headers: corsHeaders() });
      }

      if ((url.pathname === "/health" || url.pathname === "/api/health") && request.method === "GET") {
        return jsonResponse({ ok: true });
      }

      if (url.pathname === "/device/upload" || url.pathname === "/api/device/upload") {
        if (request.method !== "POST") {
          return jsonError("method_not_allowed", "Use POST /api/device/upload.", 405);
        }

        return handleDeviceUpload(request, env);
      }

      return jsonError("not_found", "Route not found.", 404);
    } catch (error) {
      console.error("Unhandled request error", error);
      return jsonError("internal_error", "Internal server error.", 500);
    }
  }
} satisfies ExportedHandler<Env>;

async function handleDeviceUpload(request: Request, env: Env): Promise<Response> {
  const contentType = request.headers.get("content-type") ?? "";
  if (!contentType.toLowerCase().includes("application/json")) {
    return jsonError("unsupported_media_type", "Request body must be JSON.", 415);
  }

  const payload = await parseJsonObject(request);
  if (!payload.ok) {
    return jsonError("invalid_json", payload.error, 400);
  }

  const upload = normalizeDeviceUploadPayload(payload.value);
  if (!upload.ok) {
    return jsonError(upload.code, upload.error, 400);
  }

  console.log(
    JSON.stringify({
      event: "device_upload_received",
      device_id: upload.value.deviceId,
      time: upload.value.recordedAt,
      sensor_count: upload.value.sensorData.length,
      sensor_names: upload.value.sensorData.map((sensor) => sensor.name)
    })
  );

  const receivedAt = new Date().toISOString();

  const statements = [
    env.DB.prepare(
      `INSERT INTO device (device_id, first_seen_time, last_upload_time, upload_count)
       VALUES (?1, ?2, ?2, 1)
       ON CONFLICT(device_id) DO UPDATE SET
         last_upload_time = excluded.last_upload_time,
         upload_count = device.upload_count + 1`
    ).bind(upload.value.deviceId, receivedAt),
  ];
  const dataIds: string[] = [];

  for (const sensor of upload.value.sensorData) {
    const dataId = crypto.randomUUID();
    dataIds.push(dataId);

    statements.push(
      env.DB.prepare(
        `INSERT INTO sensor_data
           (data_id, device_id, sensor_name, data, time, upload_time)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6)`
      ).bind(
        dataId,
        upload.value.deviceId,
        sensor.name,
        sensor.data,
        upload.value.recordedAt,
        receivedAt
      )
    );
  }

  await env.DB.batch(statements);

  console.log(
    JSON.stringify({
      event: "device_upload_stored",
      device_id: upload.value.deviceId,
      time: upload.value.recordedAt,
      upload_time: receivedAt,
      sensor_count: upload.value.sensorData.length,
      data_ids: dataIds
    })
  );

  return jsonResponse(
    {
      ok: true,
      device_id: upload.value.deviceId,
      time: upload.value.recordedAt,
      upload_time: receivedAt,
      sensor_count: upload.value.sensorData.length,
      data_ids: dataIds
    },
    201
  );
}

async function parseJsonObject(
  request: Request
): Promise<{ ok: true; value: JsonObject } | { ok: false; error: string }> {
  try {
    const value: unknown = await request.json();
    if (!isJsonObject(value)) {
      return { ok: false, error: "JSON body must be an object." };
    }

    return { ok: true, value };
  } catch {
    return { ok: false, error: "Malformed JSON body." };
  }
}

function normalizeDeviceUploadPayload(
  payload: JsonObject
): { ok: true; value: DeviceUploadPayload } | { ok: false; code: string; error: string } {
  const deviceId = normalizeDeviceId(payload.device_id);
  if (!deviceId.ok) {
    return { ok: false, code: "invalid_device_id", error: deviceId.error };
  }

  const recordedAt = normalizePayloadTime(payload.time);
  if (!recordedAt.ok) {
    return { ok: false, code: "invalid_time", error: recordedAt.error };
  }

  if (!isJsonObject(payload.sensor_data)) {
    return { ok: false, code: "invalid_sensor_data", error: "sensor_data must be a non-empty JSON object." };
  }

  const sensors = Object.entries(payload.sensor_data);
  if (sensors.length === 0) {
    return { ok: false, code: "invalid_sensor_data", error: "sensor_data must contain at least one sensor." };
  }

  const sensorData = [];
  for (const [name, value] of sensors) {
    const sensorName = normalizeSensorName(name);
    if (!sensorName.ok) {
      return { ok: false, code: "invalid_sensor_name", error: sensorName.error };
    }

    if (!isFlatSensorValue(value)) {
      return {
        ok: false,
        code: "invalid_sensor_value",
        error: `Sensor ${name} must be a scalar value; nested objects and arrays are not supported.`
      };
    }

    sensorData.push(normalizeSensorReading(sensorName.value, value));
  }

  return {
    ok: true,
    value: {
      deviceId: deviceId.value,
      sensorData,
      recordedAt: recordedAt.value
    }
  };
}

function normalizeDeviceId(rawValue: unknown): { ok: true; value: string } | { ok: false; error: string } {
  if (typeof rawValue !== "string" || rawValue.trim() === "") {
    return { ok: false, error: "device_id is required and must be a non-empty string." };
  }

  const value = rawValue.trim();
  if (value.length > DEVICE_ID_MAX_LENGTH) {
    return { ok: false, error: `Device id must be ${DEVICE_ID_MAX_LENGTH} characters or fewer.` };
  }

  if (!DEVICE_ID_PATTERN.test(value)) {
    return { ok: false, error: "Device id may contain only letters, numbers, dot, underscore, colon, and dash." };
  }

  return { ok: true, value };
}

function normalizeSensorName(value: string): { ok: true; value: string } | { ok: false; error: string } {
  const normalized = value.trim();
  if (normalized === "") {
    return { ok: false, error: "Sensor name must not be empty." };
  }

  if (normalized.length > SENSOR_NAME_MAX_LENGTH) {
    return { ok: false, error: `Sensor name must be ${SENSOR_NAME_MAX_LENGTH} characters or fewer.` };
  }

  if (!SENSOR_NAME_PATTERN.test(normalized)) {
    return { ok: false, error: "Sensor name may contain only letters, numbers, dot, underscore, colon, and dash." };
  }

  return { ok: true, value: normalized };
}

function normalizePayloadTime(value: unknown): { ok: true; value: string } | { ok: false; error: string } {
  if (typeof value === "string") {
    const date = new Date(value);
    if (!Number.isNaN(date.getTime())) {
      return { ok: true, value: date.toISOString() };
    }
  }

  if (typeof value === "number" && Number.isFinite(value)) {
    const milliseconds = value < 10_000_000_000 ? value * 1000 : value;
    const date = new Date(milliseconds);
    if (!Number.isNaN(date.getTime())) {
      return { ok: true, value: date.toISOString() };
    }
  }

  return { ok: false, error: "time must be an ISO timestamp string, Unix seconds, or Unix milliseconds." };
}

function normalizeSensorReading(name: string, value: unknown): DeviceUploadPayload["sensorData"][number] {
  return {
    name,
    data: JSON.stringify(value)
  };
}

function isFlatSensorValue(value: unknown): boolean {
  return value === null || typeof value === "string" || typeof value === "number" || typeof value === "boolean";
}

function isJsonObject(value: unknown): value is JsonObject {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function jsonResponse(body: JsonObject, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      ...corsHeaders()
    }
  });
}

function jsonError(code: string, message: string, status: number): Response {
  return jsonResponse(
    {
      ok: false,
      error: {
        code,
        message
      }
    },
    status
  );
}

function corsHeaders(): HeadersInit {
  return {
    "access-control-allow-origin": "*",
    "access-control-allow-methods": "GET,POST,OPTIONS",
    "access-control-allow-headers": "content-type,x-device-id"
  };
}
