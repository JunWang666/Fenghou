export interface Env {
  DB: D1Database;
}

export const DEVICE_ID_PATTERN = /^[A-Za-z0-9._:-]+$/;
export const SENSOR_NAME_PATTERN = /^[A-Za-z0-9._:-]+$/;

export function jsonResponse(body: Record<string, unknown>, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": "no-store"
    }
  });
}

export function jsonError(code: string, message: string, status: number): Response {
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

export function normalizeDeviceId(value: string | undefined): string | null {
  if (!value) {
    return null;
  }

  const deviceId = value.trim();
  if (deviceId.length === 0 || deviceId.length > 128 || !DEVICE_ID_PATTERN.test(deviceId)) {
    return null;
  }

  return deviceId;
}

export function normalizeSensorNames(values: string[]): string[] | null {
  const names = Array.from(new Set(values.map((value) => value.trim()).filter(Boolean)));
  if (names.length === 0) {
    return [];
  }

  for (const name of names) {
    if (name.length > 128 || !SENSOR_NAME_PATTERN.test(name)) {
      return null;
    }
  }

  return names;
}

export function normalizeIsoDate(value: string | null): string | null {
  if (!value) {
    return null;
  }

  const date = new Date(value);
  if (Number.isNaN(date.getTime())) {
    return null;
  }

  return date.toISOString();
}

export function normalizeLimit(value: string | null, fallback: number, max: number): number | null {
  if (!value) {
    return fallback;
  }

  const limit = Number(value);
  if (!Number.isInteger(limit) || limit < 1 || limit > max) {
    return null;
  }

  return limit;
}

export function valueNumber(rawData: string): number | null {
  try {
    const value: unknown = JSON.parse(rawData);
    if (typeof value === "number" && Number.isFinite(value)) {
      return value;
    }

    if (typeof value === "string" && value.trim() !== "") {
      const parsed = Number(value);
      return Number.isFinite(parsed) ? parsed : null;
    }
  } catch {
    return null;
  }

  return null;
}
