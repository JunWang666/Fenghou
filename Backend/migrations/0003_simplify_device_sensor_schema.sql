PRAGMA foreign_keys = OFF;

CREATE TABLE IF NOT EXISTS device (
  device_id TEXT PRIMARY KEY,
  first_seen_time TEXT NOT NULL,
  last_upload_time TEXT NOT NULL,
  upload_count INTEGER NOT NULL DEFAULT 0
);

INSERT OR REPLACE INTO device (device_id, first_seen_time, last_upload_time, upload_count)
SELECT
  id,
  first_seen_at,
  last_seen_at,
  upload_count
FROM devices;

CREATE TABLE sensor_data_new (
  data_id TEXT PRIMARY KEY,
  device_id TEXT NOT NULL,
  sensor_name TEXT NOT NULL,
  data TEXT NOT NULL,
  time TEXT NOT NULL,
  upload_time TEXT NOT NULL,
  FOREIGN KEY (device_id) REFERENCES device(device_id)
);

INSERT INTO sensor_data_new (data_id, device_id, sensor_name, data, time, upload_time)
SELECT
  id,
  device_id,
  sensor_name,
  value_json,
  recorded_at,
  received_at
FROM device_sensor_readings;

DROP TABLE IF EXISTS device_sensor_readings;
DROP TABLE IF EXISTS device_uploads;
DROP TABLE IF EXISTS devices;

ALTER TABLE sensor_data_new RENAME TO sensor_data;

CREATE INDEX IF NOT EXISTS idx_sensor_data_device_time
  ON sensor_data(device_id, time DESC);

CREATE INDEX IF NOT EXISTS idx_sensor_data_sensor_time
  ON sensor_data(sensor_name, time DESC);

CREATE INDEX IF NOT EXISTS idx_sensor_data_device_sensor_time
  ON sensor_data(device_id, sensor_name, time DESC);

PRAGMA foreign_keys = ON;
