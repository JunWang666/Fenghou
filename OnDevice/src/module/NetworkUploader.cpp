#include "NetworkUploader.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <math.h>

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
      if (!onlineRefreshSent_) {
        DeviceConfig cfg;
        configStore_->getCopy(&cfg);
        if (ensureWifi(cfg)) {
          onlineRefreshSent_ = postPayload(cfg, "{}", "online-refresh");
        }
      }
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

    if (!onlineRefreshSent_) {
      onlineRefreshSent_ = postPayload(cfg, "{}", "online-refresh");
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
    DEBUG_LOG("[wifi] connected ip=%s rssi=%d\n",
              WiFi.localIP().toString().c_str(),
              WiFi.RSSI());
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE,
              INADDR_NONE,
              INADDR_NONE,
              IPAddress(1, 1, 1, 1),
              IPAddress(8, 8, 8, 8));
  DEBUG_LOG("[wifi] connecting ssid=%s password_len=%u\n",
            cfg.ssid,
            static_cast<unsigned int>(strlen(cfg.password)));
  WiFi.begin(cfg.ssid, cfg.password);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    DEBUG_LOG("[wifi] wait status=%d elapsed_ms=%lu\n",
              static_cast<int>(WiFi.status()),
              static_cast<unsigned long>(millis() - start));
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  bool connected = WiFi.status() == WL_CONNECTED;
  DEBUG_LOG("[wifi] connect result=%d status=%d elapsed_ms=%lu ip=%s rssi=%d\n",
            connected ? 1 : 0,
            static_cast<int>(WiFi.status()),
            static_cast<unsigned long>(millis() - start),
            connected ? WiFi.localIP().toString().c_str() : "0.0.0.0",
            connected ? WiFi.RSSI() : 0);
  if (connected) {
    DEBUG_LOG("[wifi] gateway=%s subnet=%s dns=%s\n",
              WiFi.gatewayIP().toString().c_str(),
              WiFi.subnetMask().toString().c_str(),
              WiFi.dnsIP().toString().c_str());
  }
  return connected;
}

String NetworkUploader::jsonPayload(const LatestData &d, const DeviceConfig &cfg) {
  String s;
  s.reserve(760);
  s += "{\"device_id\":\"";
  s += cfg.deviceId;
  s += "\",\"uptime_s\":";
  s += d.uptimeS;
  s += ",\"rssi\":";
  s += String(static_cast<int>(d.wifiRssi));
  s += ",\"env\":{\"temperature_c\":";
  s += isfinite(d.temperatureC) ? String(d.temperatureC, 2) : "null";
  s += ",\"humidity_rh\":";
  s += isfinite(d.humidityRh) ? String(d.humidityRh, 2) : "null";
  s += ",\"pressure_hpa\":";
  s += isfinite(d.pressureHpa) ? String(d.pressureHpa, 2) : "null";
  s += ",\"altitude_m\":";
  s += isfinite(d.altitudeM) ? String(d.altitudeM, 1) : "null";
  s += "},\"air\":{\"eco2_ppm\":";
  s += d.eco2Ppm;
  s += ",\"tvoc_ppb\":";
  s += d.tvocPpb;
  s += ",\"warmup\":";
  s += d.sgpWarmup ? "true" : "false";
  s += "},\"sound\":{\"rms\":";
  s += String(d.soundRms, 5);
  s += ",\"peak\":";
  s += String(d.soundPeak, 5);
  s += ",\"level\":";
  s += String(static_cast<unsigned int>(d.soundLevel));
  s += "},\"light\":{\"f\":[";
  for (uint8_t i = 0; i < 8; i++) {
    if (i) {
      s += ",";
    }
    s += d.f[i];
  }
  s += "],\"clear\":";
  s += d.clear;
  s += ",\"nir\":";
  s += d.nir;
  s += "},\"status\":{\"aht\":";
  s += d.ahtOk ? "true" : "false";
  s += ",\"bmp\":";
  s += d.bmpOk ? "true" : "false";
  s += ",\"sgp30\":";
  s += d.sgpOk ? "true" : "false";
  s += ",\"as7341\":";
  s += d.as7341Ok ? "true" : "false";
  s += ",\"audio\":";
  s += d.audioOk ? "true" : "false";
  s += "}}";
  return s;
}
