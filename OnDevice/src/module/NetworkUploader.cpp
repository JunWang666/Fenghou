#include "NetworkUploader.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <math.h>
#include <time.h> // 确保引入时间库
#include <ArduinoJson.h>

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
      String refreshBody = onlineRefreshPayload(cfg);
      onlineRefreshSent_ = postPayload(cfg, refreshBody, "online-refresh");
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
    // 连上 WiFi 后自动向阿里云同步时间
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
    DEBUG_LOG("[wifi] gateway=%s subnet=%s dns=%s\n",
              WiFi.gatewayIP().toString().c_str(),
              WiFi.subnetMask().toString().c_str(),
              WiFi.dnsIP().toString().c_str());
  }
  return connected;
}

String NetworkUploader::onlineRefreshPayload(const DeviceConfig &cfg) {
  JsonDocument doc;
  doc["device_id"] = cfg.deviceId;
  doc["time"] = time(nullptr);

  JsonObject sensor = doc["sensor_data"].to<JsonObject>();
  JsonObject status = sensor["status"].to<JsonObject>();
  status["online"] = true;
  status["refresh"] = true;

  String output;
  serializeJson(doc, output);
  return output;
}

String NetworkUploader::jsonPayload(const LatestData &d, const DeviceConfig &cfg) {
  // 创建 JSON 文档对象（ArduinoJson 7 会自动管理内存大小）
  JsonDocument doc;

  // 1. 外层基础信息
  doc["device_id"] = cfg.deviceId;
  doc["time"] = time(nullptr);

  // 2. 创建 sensor_data 嵌套对象
  JsonObject sensor = doc["sensor_data"].to<JsonObject>();
  sensor["uptime_s"] = d.uptimeS;
  sensor["rssi"] = d.wifiRssi;

  // --- env 环境数据 ---
  JsonObject env = sensor["env"].to<JsonObject>();
  if (isfinite(d.temperatureC)) {
    // 使用 serialized 保持你原本 String(val, 2) 的小数位数控制
    env["temperature_c"] = serialized(String(d.temperatureC, 2));
    env["humidity_rh"] = serialized(String(d.humidityRh, 2));
    env["pressure_hpa"] = serialized(String(d.pressureHpa, 2));
    env["altitude_m"] = serialized(String(d.altitudeM, 1));
  } else {
    env["temperature_c"] = nullptr;
    env["humidity_rh"] = nullptr;
    env["pressure_hpa"] = nullptr;
    env["altitude_m"] = nullptr;
  }

  // --- air 气体数据 ---
  JsonObject air = sensor["air"].to<JsonObject>();
  air["eco2_ppm"] = d.eco2Ppm;
  air["tvoc_ppb"] = d.tvocPpb;
  air["warmup"] = d.sgpWarmup;

  // --- sound 声音数据 ---
  JsonObject sound = sensor["sound"].to<JsonObject>();
  sound["rms"] = serialized(String(d.soundRms, 5));
  sound["peak"] = serialized(String(d.soundPeak, 5));
  sound["level"] = d.soundLevel;

  // --- light 光谱数据 ---
  JsonObject light = sensor["light"].to<JsonObject>();
  JsonArray f_array = light["f"].to<JsonArray>();
  for (uint8_t i = 0; i < 8; i++) {
    f_array.add(d.f[i]);
  }
  light["clear"] = d.clear;
  light["nir"] = d.nir;

  // --- status 传感器状态 ---
  JsonObject status = sensor["status"].to<JsonObject>();
  status["aht"] = d.ahtOk;
  status["bmp"] = d.bmpOk;
  status["sgp30"] = d.sgpOk;
  status["as7341"] = d.as7341Ok;
  status["audio"] = d.audioOk;

  // 3. 序列化为标准 JSON 字符串输出
  String output;
  serializeJson(doc, output);

  return output;
}
