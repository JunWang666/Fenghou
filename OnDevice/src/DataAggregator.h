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
  struct MetricAggregate {
    double sum = 0.0;
    float max = -INFINITY;
    uint32_t count = 0;

    void add(float value);
    bool hasValue() const;
    float avg() const;
    float sumValue() const;
    float maxValue() const;
    void reset();
  };

  QueueHandle_t sampleQueue_ = nullptr;
  QueueHandle_t uploadQueue_ = nullptr;
  SemaphoreHandle_t dataMutex_ = nullptr;
  LatestData *latestData_ = nullptr;
  ConfigStore *configStore_ = nullptr;
  MetricAggregate temperature_;
  MetricAggregate humidity_;
  MetricAggregate pressure_;
  MetricAggregate altitude_;
  MetricAggregate soundPeak_;
  uint32_t lastSoundSampleMs_ = 0;
  uint32_t noiseMinuteStartMs_ = 0;
  float noiseMinuteMaxDb_ = NAN;
  uint32_t highVolumeExposureMs_ = 0;
  uint32_t lastLightSampleMs_ = 0;
  uint32_t sunlightDurationMs_ = 0;
  uint32_t flickerHazardCount_ = 0;
  uint32_t windowSampleCount_ = 0;

  void applySample(const SensorSample &sample);
  void buildUploadSnapshot(LatestData *snapshot) const;
  void resetUploadWindow();
};
