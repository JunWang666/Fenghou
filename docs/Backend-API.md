# Fenghou Backend API

Base URL:

`https://fenghou.goudaijun.top/api`

Workers fallback URL:

`https://fenghou-backend.82-rayon-legible.workers.dev`

## Health Check

`GET /health`

Full URL:

`GET https://fenghou.goudaijun.top/api/health`

Response:

```json
{
  "ok": true
}
```

## Upload Device Sensor Data

`POST /device/upload`

Full URL:

`POST https://fenghou.goudaijun.top/api/device/upload`

Request headers:

```http
Content-Type: application/json
```

Request body:

```json
{
  "device_id": "esp32-c3-001",
  "sensor_data": {
    "temperature_c": 25.3,
    "humidity_rh": 61.2,
    "eco2_ppm": 415,
    "tvoc_ppb": 9
  },
  "time": "2026-05-30T15:30:00+08:00"
}
```

PowerShell example:

```powershell
$body = @{
  device_id = "esp32-c3-001"
  sensor_data = @{
    temperature_c = 25.3
    humidity_rh = 61.2
    eco2_ppm = 415
    tvoc_ppb = 9
  }
  time = "2026-05-30T15:30:00+08:00"
} | ConvertTo-Json -Depth 5 -Compress

$body | curl.exe -X POST "https://fenghou.goudaijun.top/api/device/upload" `
  -H "Content-Type: application/json" `
  --data-binary "@-"
```

PowerShell native HTTP example:

```powershell
Invoke-RestMethod `
  -Method POST `
  -Uri "https://fenghou.goudaijun.top/api/device/upload" `
  -ContentType "application/json" `
  -Body $body
```

Fields:

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `device_id` | string | yes | Device identifier. Allowed characters: letters, numbers, `.`, `_`, `:`, `-`. |
| `sensor_data` | object | yes | Flat sensor map. Each key is stored as `sensor_name`; values must be scalar JSON values, not nested objects or arrays. |
| `time` | string or number | yes | Device-side measurement time. Accepts ISO timestamp, Unix seconds, or Unix milliseconds. |

`sensor_data` may contain at most 8 sensor fields per upload. The device firmware should aggregate high-frequency samples before upload, for example Avg for temperature/humidity/pressure/altitude, Max for sound peak, and Sum for light clear counts.

Response:

```json
{
  "ok": true,
  "device_id": "esp32-c3-001",
  "time": "2026-05-30T07:30:00.000Z",
  "upload_time": "2026-05-30T07:30:01.000Z",
  "sensor_count": 4,
  "data_ids": [
    "uuid-1",
    "uuid-2",
    "uuid-3",
    "uuid-4"
  ]
}
```

Storage behavior:

- The raw request payload is not stored.
- `device` is upserted by `device_id`.
- Each `sensor_data` entry creates one row in `sensor_data`.
- `sensor_data.data` stores only that sensor value serialized as JSON.

Database tables:

```sql
device(
  device_id,
  first_seen_time,
  last_upload_time,
  upload_count
)

sensor_data(
  data_id,
  device_id,
  sensor_name,
  data,
  time,
  upload_time
)
```

Error response shape:

```json
{
  "ok": false,
  "error": {
    "code": "invalid_sensor_data",
    "message": "sensor_data must contain at least one sensor."
  }
}
```

All failed requests return `ok: false`. `ok: true` is returned only after the backend accepts the payload and the database write completes.
