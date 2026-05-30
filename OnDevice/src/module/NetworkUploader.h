#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>

#include "../ConfigStore.h"
#include "../sensor/Types.h"

class NetworkUploader {
public:
  void begin(QueueHandle_t uploadQueue, ConfigStore *configStore);
  void run();

private:
  QueueHandle_t uploadQueue_ = nullptr;
  ConfigStore *configStore_ = nullptr;
  WiFiClientSecure secureClient_;

  bool ensureWifi(const DeviceConfig &cfg);
  bool postPayload(const DeviceConfig &cfg, const String &payload, const char *tag);
  String jsonPayload(const LatestData &data, const DeviceConfig &cfg);
};
