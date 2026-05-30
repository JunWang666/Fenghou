import type { DevicesResponse, ReadingsResponse, SensorsResponse } from "./types";

const API_BASE = "/dash-api";

function isApiError(value: unknown): value is { ok: false; error?: { message?: string } } {
  return typeof value === "object" && value !== null && "ok" in value;
}

async function getJson<T>(url: string): Promise<T> {
  const response = await fetch(url);
  const body: unknown = await response.json().catch(() => null);

  if (!response.ok || !isApiError(body) || !body.ok) {
    const message = isApiError(body) ? body.error?.message ?? `Request failed with ${response.status}` : `Request failed with ${response.status}`;
    throw new Error(message);
  }

  return body as T;
}

export function fetchDevices(): Promise<DevicesResponse> {
  return getJson<DevicesResponse>(`${API_BASE}/devices`);
}

export function fetchSensors(deviceId: string): Promise<SensorsResponse> {
  return getJson<SensorsResponse>(`${API_BASE}/devices/${encodeURIComponent(deviceId)}/sensors`);
}

export function fetchReadings(params: {
  deviceId: string;
  sensors: string[];
  from?: string;
  to?: string;
  limit?: number;
}): Promise<ReadingsResponse> {
  const query = new URLSearchParams();

  for (const sensor of params.sensors) {
    query.append("sensor", sensor);
  }

  if (params.from) {
    query.set("from", params.from);
  }

  if (params.to) {
    query.set("to", params.to);
  }

  if (params.limit) {
    query.set("limit", String(params.limit));
  }

  return getJson<ReadingsResponse>(
    `${API_BASE}/devices/${encodeURIComponent(params.deviceId)}/readings?${query.toString()}`
  );
}
