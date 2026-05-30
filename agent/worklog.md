# Worklog

## 2026-05-30

- Initialized repository-level ignore rules for ESP32, Cloudflare Workers/Pages, iOS, common tooling, and local secrets.
- Created `agent/` as the project work record folder.
- Initialized `Backend/` as a Cloudflare Workers service.
- Added `POST /device/upload` to accept JSON payloads, resolve `device_id`/`deviceId`/`x-device-id`, and store raw payloads by device in D1.
- Added D1 migration for `devices` and `device_uploads`.
- Bound `Backend/wrangler.jsonc` to remote D1 database `code2real-device-data` (`4a5a1d4b-8d7f-4d37-aab6-04b0e8a0e993`).
- Applied remote D1 migration `0001_device_uploads.sql`.
- Updated upload contract to require `{ device_id, sensor_data, time }`.
- Added parsed sensor storage so raw payloads are not stored: `device_uploads` contains upload metadata and `device_sensor_readings` contains one row per sensor.
- Simplified database schema to only retain `device` and `sensor_data`; `sensor_data` contains `data_id`, `device_id`, `sensor_name`, `data`, `time`, and `upload_time`.

### Verification

- Installed `Backend/` npm dependencies with `npm.cmd install`.
- Passed `npm.cmd run check` from `Backend/`.
- Verified remote D1 tables include `devices` and `device_uploads`.
- Applied remote D1 migration `0003_simplify_device_sensor_schema.sql`.
- Verified remote D1 business tables are `device` and `sensor_data`; `sensor_data` columns are `data_id`, `device_id`, `sensor_name`, `data`, `time`, and `upload_time`.
- Renamed project to Fenghou: Worker/package `fenghou-backend`, D1 database `fenghou-device-data`.
- Added public API documentation at `docs/Backend-API.md`.
- Added `/api/health` and `/api/device/upload` routes.
- Configured Worker route `fenghou.goudaijun.top/api/*` for zone `goudaijun.top`.
- Deployed Worker route successfully; Wrangler output confirmed `fenghou.goudaijun.top/api/*`.
- Local verification of custom domain is blocked by the current proxy/DNS environment: `nslookup fenghou.goudaijun.top 1.1.1.1` returns `198.18.0.61`.
- Added top-level Worker error handling so backend failures return `ok:false`; successful upload returns `ok:true` only after database writes complete.
- Synced Dashboard routing settings into `Backend/wrangler.jsonc`: keep `workers_dev` and preview URLs enabled.
- Documented PowerShell-safe upload examples using `curl.exe --data-binary "@-"` and `Invoke-RestMethod`.
- Updated `OnDevice` mock uploader to POST Fenghou sensor data to `https://fenghou.goudaijun.top/api/device/upload` with `device_id=test_1`.
- Enabled ESP32-C3 USB CDC serial output with `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1`.
- Made OnDevice upload device id configurable through the `DEVICE_ID` build flag; current value is `test_1`.
- Added `WIFI_HOSTNAME` build flag and set ESP32 Wi-Fi hostname before connecting to the router.
- Changed ESP32 Wi-Fi hostname generation to `fenghou-{DEVICE_ID}` and removed the separate hostname build flag.
- Added backend structured logs for received upload metadata and stored sensor row IDs.
- Improved OnDevice Wi-Fi connection diagnostics: disabled Wi-Fi sleep, added connection timing, and added a 15 second connection timeout.
- Reverted aggressive Wi-Fi disconnect behavior; use normal `WiFi.begin()` flow and non-persistent disconnect only on timeout.
- Added Wi-Fi scan diagnostics after connection timeout to identify whether the configured SSID is visible to ESP32.
- Temporarily changed OnDevice firmware flow to scan Wi-Fi networks every 5 seconds without connecting or uploading.
- Restored OnDevice connection/upload flow and added timing logs for Wi-Fi setup, scan, begin, wait, payload build, TLS setup, HTTP begin, and POST.
- Changed Wi-Fi connect flow to lock onto the scanned target AP channel and BSSID before `WiFi.begin()` to avoid repeated AP selection delays.
- Added ESP32 Wi-Fi event logging with disconnect reason codes and normalized router hostname to `fenghou-{device-id}` with only lowercase letters, digits, and dashes.
- Changed Wi-Fi connection flow back to direct `WiFi.begin(ssid, password)` and moved scanning to post-failure diagnostics after `reason=2` auth timeout.
- Added OnDevice Wi-Fi configuration diagnostics for SSID and password length without printing the password value.
- Added a 5 second boot diagnostics delay so PlatformIO serial monitor can attach before initial logs.
