#include "NetworkUploader.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <math.h>
#include <time.h>

#include "../DebugLog.h"

void NetworkUploader::begin(QueueHandle_t uploadQueue, ConfigStore *configStore) {
  uploadQueue_ = uploadQueue;
  configStore_ = configStore;
}

void NetworkUploader::run() {
  WiFi.mode(WIFI_STA);
  LatestData data;
  for (;;) {
    if (xQueueReceive(uploadQueue_, &data, pdMS_TO_TICKS(1000)) != pdTRUE) {
      continue;
    }

    DeviceConfig cfg;
    configStore_->getCopy(&cfg);
    DEBUG_LOG("[post] queued sample uptime=%lu server=%s\n",
              static_cast<unsigned long>(data.uptimeS),
              cfg.server);
    if (!ensureWifi(cfg)) {
      Serial.println("WiFi not connected; upload skipped.");
      continue;
    }

    String payload = jsonPayload(data, cfg);
    DEBUG_LOG("[post] payload bytes=%u body=%s\n",
              static_cast<unsigned int>(payload.length()),
              payload.c_str());
    postPayload(cfg, payload, "sensor-data");
  }
}

bool NetworkUploader::postPayload(const DeviceConfig &cfg, const String &payload, const char *tag) {
  HTTPClient http;
  bool began = false;
  if (String(cfg.server).startsWith("https://")) {
    secureClient_.setInsecure();
    began = http.begin(secureClient_, cfg.server);
  } else {
    began = http.begin(cfg.server);
  }
  if (!began) {
    Serial.println("HTTP begin failed.");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  if (cfg.token[0]) {
    String auth = "Bearer ";
    auth += cfg.token;
    http.addHeader("Authorization", auth);
  }
  int code = http.POST(payload);
  String response = http.getString();
  Serial.printf("Upload %s HTTP %d, heap=%lu\n",
                tag,
                code,
                static_cast<unsigned long>(ESP.getFreeHeap()));
  DEBUG_LOG("[post:%s] response bytes=%u body=%s\n",
            tag,
            static_cast<unsigned int>(response.length()),
            response.c_str());
  http.end();
  return code > 0 && code < 400;
}

bool NetworkUploader::ensureWifi(const DeviceConfig &cfg) {
  if (!cfg.valid) {
    DEBUG_LOGLN("[wifi] invalid config; skip connect");
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
  DEBUG_LOG("[wifi] connecting ssid=%s password_len=%u\n", cfg.ssid, static_cast<unsigned int>(strlen(cfg.password)));
  WiFi.begin(cfg.ssid, cfg.password);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
    DEBUG_LOG("[wifi] gateway=%s subnet=%s dns=%s\n",
              WiFi.gatewayIP().toString().c_str(),
              WiFi.subnetMask().toString().c_str(),
              WiFi.dnsIP().toString().c_str());
  }
  return connected;
}

String NetworkUploader::jsonPayload(const LatestData &d, const DeviceConfig &cfg) {
  JsonDocument doc;

  doc["device_id"] = cfg.deviceId;
  doc["time"] = time(nullptr);

  JsonObject sensor = doc["sensor_data"].to<JsonObject>();
  if (isfinite(d.humidityRh)) {
    sensor["humidity"] = serialized(String(d.humidityRh, 2));
  } else {
    sensor["humidity"] = nullptr;
  }
  if (isfinite(d.temperatureC)) {
    sensor["temperature"] = serialized(String(d.temperatureC, 2));
  } else {
    sensor["temperature"] = nullptr;
  }
  if (isfinite(d.pressureHpa)) {
    sensor["pressure"] = serialized(String(d.pressureHpa, 2));
  } else {
    sensor["pressure"] = nullptr;
  }
  if (isfinite(d.altitudeM)) {
    sensor["altitude"] = serialized(String(d.altitudeM, 1));
  } else {
    sensor["altitude"] = nullptr;
  }
  if (isfinite(d.soundPeakMax)) {
    sensor["sound_peak_max"] = serialized(String(d.soundPeakMax, 4));
  }
  if (isfinite(d.lightClearSum)) {
    sensor["light_clear_sum"] = serialized(String(d.lightClearSum, 0));
  }
  if (d.aggregateSampleCount > 0) {
    sensor["sample_count"] = d.aggregateSampleCount;
  }

  String output;
  serializeJson(doc, output);
  return output;
}
