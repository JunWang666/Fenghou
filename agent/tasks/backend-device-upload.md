# Fenghou Backend Device Upload

## Scope

Cloudflare Workers backend receives ESP32 sensor uploads at:

`POST /api/device/upload`

Canonical route:

`https://fenghou.goudaijun.top/api/device/upload`

Storage uses Cloudflare D1 database:

- Database name: `fenghou-device-data`
- Database id: `be1e6798-3a65-412a-a91d-ae24b6035eb4`

## Request Payload

```json
{
  "device_id": "esp32-c3-001",
  "sensor_data": {
    "temperature": 25.3,
    "humidity": 61.2,
    "sgp30": {
      "eco2": 415,
      "tvoc": 9
    }
  },
  "time": "2026-05-30T15:30:00+08:00"
}
```

Rules:

- `device_id` is required.
- `sensor_data` is required and must be a non-empty object.
- Each `sensor_data` key becomes one `sensor_data` table row.
- `time` is the device-side measurement time. It accepts ISO timestamp, Unix seconds, or Unix milliseconds.
- The backend adds `upload_time` when the Worker receives the request.
- The full raw payload is not stored.

## Database Tables

Only two business tables should remain.

### `device`

| Column | Purpose |
| --- | --- |
| `device_id` | Device primary key. |
| `first_seen_time` | First upload time observed by backend. |
| `last_upload_time` | Last upload time observed by backend. |
| `upload_count` | Number of upload requests accepted for this device. |

### `sensor_data`

| Column | Purpose |
| --- | --- |
| `data_id` | Sensor row primary key. |
| `device_id` | Device id from request. |
| `sensor_name` | Key from `sensor_data`. |
| `data` | JSON string for that sensor value only. |
| `time` | Parsed device-side measurement time. |
| `upload_time` | Backend receive time. |

Indexes:

- `idx_sensor_data_device_time`
- `idx_sensor_data_sensor_time`
- `idx_sensor_data_device_sensor_time`

## Response

```json
{
  "ok": true,
  "device_id": "esp32-c3-001",
  "time": "2026-05-30T07:30:00.000Z",
  "upload_time": "2026-05-30T07:30:01.000Z",
  "sensor_count": 3,
  "data_ids": ["uuid-1", "uuid-2", "uuid-3"]
}
```

## Implementation Files

- Worker handler: `Backend/src/index.ts`
- Wrangler config: `Backend/wrangler.jsonc`
- Public API doc: `docs/Backend-API.md`
- Migrations:
  - `Backend/migrations/0001_device_uploads.sql`
  - `Backend/migrations/0002_parse_sensor_readings.sql`
  - `Backend/migrations/0003_simplify_device_sensor_schema.sql`

The first two migrations are historical setup steps. The current final schema is enforced by `0003_simplify_device_sensor_schema.sql`.

## Verification

Completed:

- `npm.cmd run check` passes in `Backend/`.
- Remote D1 migration `0003_simplify_device_sensor_schema.sql` applied successfully.
- Remote D1 table list contains business tables `device` and `sensor_data`.
- Remote `sensor_data` columns verified as:
  - `data_id`
  - `device_id`
  - `sensor_name`
  - `data`
  - `time`
  - `upload_time`
