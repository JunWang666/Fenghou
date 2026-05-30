export interface Device {
  device_id: string;
  first_seen_time: string;
  last_upload_time: string;
  upload_count: number;
}

export interface Sensor {
  sensor_name: string;
}

export interface Reading {
  data_id: string;
  device_id: string;
  sensor_name: string;
  data: string;
  value_number: number | null;
  time: string;
  upload_time: string;
}

export interface DevicesResponse {
  ok: true;
  devices: Device[];
}

export interface SensorsResponse {
  ok: true;
  device_id: string;
  sensors: Sensor[];
}

export interface ReadingsResponse {
  ok: true;
  device_id: string;
  readings: Reading[];
}

export interface ApiErrorResponse {
  ok: false;
  error: {
    code: string;
    message: string;
  };
}
