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
    buildUploadSnapshot(&snapshot);
    xSemaphoreGive(dataMutex_);

    DeviceConfig cfg;
    configStore_->getCopy(&cfg);
    if (cfg.valid && windowSampleCount_ > 0 && millis() - lastUpload >= cfg.uploadIntervalMs) {
      if (xQueueSend(uploadQueue_, &snapshot, 0) == pdTRUE) {
        lastUpload = millis();
        xSemaphoreTake(dataMutex_, portMAX_DELAY);
        resetUploadWindow();
        xSemaphoreGive(dataMutex_);
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
      temperature_.add(sample.temperatureC);
      humidity_.add(sample.humidityRh);
      windowSampleCount_++;
    }
    break;
  case KIND_BMP:
    latestData_->bmpOk = sample.ok;
    if (sample.ok) {
      latestData_->pressureHpa = sample.pressureHpa;
      latestData_->altitudeM = sample.altitudeM;
      pressure_.add(sample.pressureHpa);
      altitude_.add(sample.altitudeM);
      windowSampleCount_++;
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
      latestData_->sunlightPresent = sample.sunlightPresent;
      latestData_->flickerHazard = sample.flickerHazard;
      latestData_->flickerStatus = sample.flickerStatus;
      uint32_t elapsedMs = lastLightSampleMs_ == 0 ? AS7341_SAMPLE_INTERVAL_MS : sample.ms - lastLightSampleMs_;
      lastLightSampleMs_ = sample.ms;
      elapsedMs = min(elapsedMs, AS7341_SAMPLE_INTERVAL_MS * 2);
      if (sample.sunlightPresent) {
        sunlightDurationMs_ += elapsedMs;
      }
      if (sample.flickerHazard) {
        flickerHazardCount_++;
      }
      latestData_->sunlightDurationMinutes = static_cast<float>(sunlightDurationMs_ / 60000UL);
      latestData_->flickerHazardCount = static_cast<float>(flickerHazardCount_);
      windowSampleCount_++;
    }
    break;
  case KIND_SOUND:
    latestData_->audioOk = sample.ok;
    if (sample.ok) {
      latestData_->soundRms = sample.soundRms;
      latestData_->soundPeak = sample.soundPeak;
      latestData_->soundDb = sample.soundDb;
      latestData_->soundLevel = sample.soundLevel;
      soundPeak_.add(sample.soundPeak);
      if (noiseMinuteStartMs_ == 0 || sample.ms - noiseMinuteStartMs_ >= AUDIO_NOISE_WINDOW_MS) {
        noiseMinuteStartMs_ = sample.ms;
        noiseMinuteMaxDb_ = sample.soundDb;
      } else if (!isfinite(noiseMinuteMaxDb_) || sample.soundDb > noiseMinuteMaxDb_) {
        noiseMinuteMaxDb_ = sample.soundDb;
      }
      uint32_t elapsedMs = lastSoundSampleMs_ == 0 ? AUDIO_SAMPLE_INTERVAL_MS : sample.ms - lastSoundSampleMs_;
      lastSoundSampleMs_ = sample.ms;
      elapsedMs = min(elapsedMs, AUDIO_SAMPLE_INTERVAL_MS * 2);
      if (sample.soundDb >= AUDIO_HIGH_VOLUME_DB) {
        highVolumeExposureMs_ += elapsedMs;
      }
      latestData_->noiseMaxDb = noiseMinuteMaxDb_;
      latestData_->highVolumeExposureMinutes = static_cast<float>(highVolumeExposureMs_ / 60000UL);
      windowSampleCount_++;
    }
    break;
  case KIND_CONFIG:
    break;
  }
}

void DataAggregator::buildUploadSnapshot(LatestData *snapshot) const {
  if (temperature_.hasValue()) {
    snapshot->temperatureC = temperature_.avg();
  }
  if (humidity_.hasValue()) {
    snapshot->humidityRh = humidity_.avg();
  }
  if (pressure_.hasValue()) {
    snapshot->pressureHpa = pressure_.avg();
  }
  if (altitude_.hasValue()) {
    snapshot->altitudeM = altitude_.avg();
  }
  if (soundPeak_.hasValue()) {
    snapshot->soundPeakMax = soundPeak_.maxValue();
  }
  if (isfinite(noiseMinuteMaxDb_)) {
    snapshot->noiseMaxDb = noiseMinuteMaxDb_;
  }
  snapshot->highVolumeExposureMinutes = static_cast<float>(highVolumeExposureMs_ / 60000UL);
  snapshot->sunlightDurationMinutes = static_cast<float>(sunlightDurationMs_ / 60000UL);
  snapshot->flickerHazardCount = static_cast<float>(flickerHazardCount_);
  snapshot->aggregateSampleCount = windowSampleCount_;
}

void DataAggregator::resetUploadWindow() {
  temperature_.reset();
  humidity_.reset();
  pressure_.reset();
  altitude_.reset();
  soundPeak_.reset();
  windowSampleCount_ = 0;
}

void DataAggregator::MetricAggregate::add(float value) {
  if (!isfinite(value)) {
    return;
  }

  sum += value;
  if (count == 0 || value > max) {
    max = value;
  }
  count++;
}

bool DataAggregator::MetricAggregate::hasValue() const {
  return count > 0;
}

float DataAggregator::MetricAggregate::avg() const {
  return count > 0 ? static_cast<float>(sum / count) : NAN;
}

float DataAggregator::MetricAggregate::sumValue() const {
  return count > 0 ? static_cast<float>(sum) : NAN;
}

float DataAggregator::MetricAggregate::maxValue() const {
  return count > 0 ? max : NAN;
}

void DataAggregator::MetricAggregate::reset() {
  sum = 0.0;
  max = -INFINITY;
  count = 0;
}
