#pragma once

#include <Arduino.h>

#include "ConfigStore.h"
#include "Types.h"

class NetworkUploader {
public:
  void begin(QueueHandle_t uploadQueue, ConfigStore *configStore);
  void run();

private:
  QueueHandle_t uploadQueue_ = nullptr;
  ConfigStore *configStore_ = nullptr;

  bool ensureWifi(const DeviceConfig &cfg);
  String jsonPayload(const LatestData &data, const DeviceConfig &cfg);
};
