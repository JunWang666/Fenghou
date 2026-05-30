#include "DataAggregator.h"

#include <WiFi.h>

void DataAggregator::begin(QueueHandle_t sampleQueue,
                           QueueHandle_t uploadQueue,
                           SemaphoreHandle_t dataMutex,
                           LatestData *latestData,
                           ConfigStore *configStore) {
  sampleQueue_ = sampleQueue;
  uploadQueue_ = uploadQueue;
  dataMutex_ = dataMutex;
  latestData_ = latestData;
  configStore_ = configStore;
}

void DataAggregator::run() {
  SensorSample sample;
  uint32_t lastUpload = 0;
  for (;;) {
    if (xQueueReceive(sampleQueue_, &sample, pdMS_TO_TICKS(250)) != pdTRUE) {
      continue;
    }

    xSemaphoreTake(dataMutex_, portMAX_DELAY);
    applySample(sample);
    LatestData snapshot = *latestData_;
    xSemaphoreGive(dataMutex_);

    DeviceConfig cfg;
    configStore_->getCopy(&cfg);
    if (cfg.valid && millis() - lastUpload >= cfg.uploadIntervalMs) {
      if (xQueueSend(uploadQueue_, &snapshot, 0) == pdTRUE) {
        lastUpload = millis();
      }
    }
  }
}

void DataAggregator::applySample(const SensorSample &sample) {
  latestData_->ms = sample.ms;
  latestData_->uptimeS = millis() / 1000;
  latestData_->wifiRssi = WiFi.isConnected() ? WiFi.RSSI() : 0;

  switch (sample.kind) {
  case KIND_AHT:
    latestData_->ahtOk = sample.ok;
    if (sample.ok) {
      latestData_->temperatureC = sample.temperatureC;
      latestData_->humidityRh = sample.humidityRh;
    }
    break;
  case KIND_BMP:
    latestData_->bmpOk = sample.ok;
    if (sample.ok) {
      latestData_->pressureHpa = sample.pressureHpa;
      latestData_->altitudeM = sample.altitudeM;
    }
    break;
  case KIND_AIR:
    latestData_->sgpOk = sample.ok;
    latestData_->sgpWarmup = sample.sgpWarmup;
    if (sample.ok) {
      latestData_->eco2Ppm = sample.eco2Ppm;
      latestData_->tvocPpb = sample.tvocPpb;
    }
    break;
  case KIND_LIGHT:
    latestData_->as7341Ok = sample.ok;
    if (sample.ok) {
      memcpy(latestData_->f, sample.f, sizeof(latestData_->f));
      latestData_->clear = sample.clear;
      latestData_->nir = sample.nir;
    }
    break;
  case KIND_SOUND:
    latestData_->audioOk = sample.ok;
    if (sample.ok) {
      latestData_->soundRms = sample.soundRms;
      latestData_->soundPeak = sample.soundPeak;
      latestData_->soundLevel = sample.soundLevel;
    }
    break;
  case KIND_CONFIG:
    break;
  }
}
