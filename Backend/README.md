# Fenghou Backend

Cloudflare Workers backend for Fenghou device data upload.

## Endpoint

`POST /api/device/upload`

Canonical URL:

`https://fenghou.goudaijun.top/api/device/upload`

The payload must be JSON with this structure:

Example:

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

`time` accepts an ISO timestamp string, Unix seconds, or Unix milliseconds.

The backend does not store the raw payload. It parses `sensor_data` and stores one row per sensor.

Tables:

- `device`: device-level first seen time, last upload time, and upload count.
- `sensor_data`: parsed sensor rows with only `data_id`, `device_id`, `sensor_name`, `data`, `time`, and `upload_time`.

For each payload entry, `sensor_data.data` stores that sensor's JSON value only, not the full request payload.

Successful response:

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

## Setup

Install dependencies:

```sh
npm install
```

Create the D1 database:

```sh
npx wrangler d1 create fenghou-device-data
```

Copy the returned `database_id` into `wrangler.jsonc`, then apply migrations:

```sh
npm run d1:migrate:local
npm run dev
```

For production:

```sh
npm run d1:migrate:remote
npm run deploy
```
