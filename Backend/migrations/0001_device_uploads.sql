CREATE TABLE IF NOT EXISTS devices (
  id TEXT PRIMARY KEY,
  first_seen_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  last_seen_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  upload_count INTEGER NOT NULL DEFAULT 0,
  metadata_json TEXT
);

CREATE TABLE IF NOT EXISTS device_uploads (
  id TEXT PRIMARY KEY,
  device_id TEXT NOT NULL,
  received_at TEXT NOT NULL,
  payload_json TEXT NOT NULL,
  remote_addr TEXT,
  user_agent TEXT,
  content_length INTEGER,
  schema_version TEXT,
  FOREIGN KEY (device_id) REFERENCES devices(id)
);

CREATE INDEX IF NOT EXISTS idx_device_uploads_device_received
  ON device_uploads(device_id, received_at DESC);

CREATE INDEX IF NOT EXISTS idx_device_uploads_received
  ON device_uploads(received_at DESC);
