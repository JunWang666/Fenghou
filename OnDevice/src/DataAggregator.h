#pragma once

#include <Arduino.h>

#include "ConfigStore.h"
#include "sensor/Types.h"

class DataAggregator {
public:
  void begin(QueueHandle_t sampleQueue,
             QueueHandle_t uploadQueue,
             SemaphoreHandle_t dataMutex,
             LatestData *latestData,
             ConfigStore *configStore);
  void run();

private:
  QueueHandle_t sampleQueue_ = nullptr;
  QueueHandle_t uploadQueue_ = nullptr;
  SemaphoreHandle_t dataMutex_ = nullptr;
  LatestData *latestData_ = nullptr;
  ConfigStore *configStore_ = nullptr;

  void applySample(const SensorSample &sample);
};
