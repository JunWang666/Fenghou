PRAGMA foreign_keys = OFF;

CREATE TABLE device_uploads_new (
  id TEXT PRIMARY KEY,
  device_id TEXT NOT NULL,
  recorded_at TEXT NOT NULL,
  received_at TEXT NOT NULL,
  remote_addr TEXT,
  user_agent TEXT,
  content_length INTEGER,
  sensor_count INTEGER NOT NULL DEFAULT 0,
  FOREIGN KEY (device_id) REFERENCES devices(id)
);

INSERT INTO device_uploads_new
  (id, device_id, recorded_at, received_at, remote_addr, user_agent, content_length, sensor_count)
SELECT
  id,
  device_id,
  received_at,
  received_at,
  remote_addr,
  user_agent,
  content_length,
  0
FROM device_uploads;

DROP TABLE device_uploads;

ALTER TABLE device_uploads_new RENAME TO device_uploads;

CREATE INDEX IF NOT EXISTS idx_device_uploads_device_recorded
  ON device_uploads(device_id, recorded_at DESC);

CREATE INDEX IF NOT EXISTS idx_device_uploads_received
  ON device_uploads(received_at DESC);

CREATE TABLE IF NOT EXISTS device_sensor_readings (
  id TEXT PRIMARY KEY,
  upload_id TEXT NOT NULL,
  device_id TEXT NOT NULL,
  recorded_at TEXT NOT NULL,
  received_at TEXT NOT NULL,
  sensor_name TEXT NOT NULL,
  value_type TEXT NOT NULL,
  value_number REAL,
  value_text TEXT,
  value_boolean INTEGER,
  value_json TEXT NOT NULL,
  FOREIGN KEY (upload_id) REFERENCES device_uploads(id),
  FOREIGN KEY (device_id) REFERENCES devices(id)
);

CREATE INDEX IF NOT EXISTS idx_device_sensor_readings_device_recorded
  ON device_sensor_readings(device_id, recorded_at DESC);

CREATE INDEX IF NOT EXISTS idx_device_sensor_readings_sensor_recorded
  ON device_sensor_readings(sensor_name, recorded_at DESC);

CREATE INDEX IF NOT EXISTS idx_device_sensor_readings_device_sensor_recorded
  ON device_sensor_readings(device_id, sensor_name, recorded_at DESC);

PRAGMA foreign_keys = ON;
