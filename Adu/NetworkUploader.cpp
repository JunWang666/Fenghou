#include "NetworkUploader.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <math.h>

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
    if (!ensureWifi(cfg)) {
      Serial.println("WiFi not connected; upload skipped.");
      continue;
    }

    HTTPClient http;
    String payload = jsonPayload(data, cfg);
    http.begin(cfg.server);
    http.addHeader("Content-Type", "application/json");
    if (cfg.token[0]) {
      String auth = "Bearer ";
      auth += cfg.token;
      http.addHeader("Authorization", auth);
    }
    int code = http.POST(payload);
    Serial.printf("Upload HTTP %d, heap=%lu\n", code, static_cast<unsigned long>(ESP.getFreeHeap()));
    http.end();
  }
}

bool NetworkUploader::ensureWifi(const DeviceConfig &cfg) {
  if (!cfg.valid) {
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid, cfg.password);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  return WiFi.status() == WL_CONNECTED;
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
