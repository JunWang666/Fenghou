#include "I2cSensorManager.h"

#include <Wire.h>

#include "../DebugLog.h"

void I2cSensorManager::begin(QueueHandle_t sampleQueue,
                             ConfigStore *configStore,
                             SemaphoreHandle_t dataMutex,
                             LatestData *latestData) {
  sampleQueue_ = sampleQueue;
  configStore_ = configStore;
  dataMutex_ = dataMutex;
  latestData_ = latestData;
}

void I2cSensorManager::run() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setTimeOut(50);
  vTaskDelay(pdMS_TO_TICKS(100));

  scanI2cBus();

  aht20Present_ = i2cDevicePresent(AHT20_ADDR);
  bmp280Present_ = bmp280Begin();
  sgp30Present_ = i2cDevicePresent(SGP30_ADDR) && sgp30Begin();
  as7341Present_ = as7341Setup();
  st25Present_ = i2cDevicePresent(ST25_USER_ADDR);
  if (st25Present_) {
    initializeNfcViewLink();
  }
  DEBUG_LOG("[sensor:init] sda=%d scl=%d aht20=%d bmp280=%d sgp30=%d as7341=%d st25=%d\n",
            PIN_I2C_SDA,
            PIN_I2C_SCL,
            aht20Present_ ? 1 : 0,
            bmp280Present_ ? 1 : 0,
            sgp30Present_ ? 1 : 0,
            as7341Present_ ? 1 : 0,
            st25Present_ ? 1 : 0);

  uint32_t lastAht = 0;
  uint32_t lastBmp = 0;
  uint32_t lastSgp = 0;
  uint32_t lastAs = 0;
  uint32_t lastNfc = 0;

  for (;;) {
    uint32_t now = millis();

    if (now - lastNfc >= NFC_POLL_INTERVAL_MS) {
      lastNfc = now;
      pollNfc(now);
    }
    if (now - lastAht >= AHT_SAMPLE_INTERVAL_MS) {
      lastAht = now;
      pollAht(now);
    }
    if (now - lastBmp >= BMP_SAMPLE_INTERVAL_MS) {
      lastBmp = now;
      pollBmp(now);
    }
    if (now - lastSgp >= SGP_SAMPLE_INTERVAL_MS) {
      lastSgp = now;
      pollSgp(now);
    }
    if (now - lastAs >= AS7341_SAMPLE_INTERVAL_MS) {
      lastAs = now;
      pollAs7341(now);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void I2cSensorManager::sendSample(const SensorSample &sample) {
  xQueueSend(sampleQueue_, &sample, QUEUE_WAIT);
}

void I2cSensorManager::pollNfc(uint32_t now) {
  if (!st25Present_) {
    return;
  }
  char raw[256];
  if (!st25ReadUserMemory(raw, sizeof(raw))) {
    return;
  }
  DEBUG_LOG("[sensor:nfc] raw=%s\n", raw);
  if (!configStore_ || !configStore_->applyPayload(raw)) {
    return;
  }

  SensorSample sample = {};
  sample.kind = KIND_CONFIG;
  sample.ms = now;
  sample.ok = true;
  sendSample(sample);
  Serial.println("NFC config updated.");
}

void I2cSensorManager::pollAht(uint32_t now) {
  SensorSample sample = {};
  sample.kind = KIND_AHT;
  sample.ms = now;
  sample.ok = aht20Present_ && aht20Read(&sample.temperatureC, &sample.humidityRh);
  DEBUG_LOG("[sensor:aht20] ok=%d temperature_c=%.2f humidity_rh=%.2f\n",
            sample.ok ? 1 : 0,
            sample.temperatureC,
            sample.humidityRh);
  sendSample(sample);
}

void I2cSensorManager::pollBmp(uint32_t now) {
  SensorSample sample = {};
  sample.kind = KIND_BMP;
  sample.ms = now;
  sample.ok = bmp280Read(&sample.temperatureC, &sample.pressureHpa, &sample.altitudeM);
  DEBUG_LOG("[sensor:bmp280] ok=%d temperature_c=%.2f pressure_hpa=%.2f altitude_m=%.1f\n",
            sample.ok ? 1 : 0,
            sample.temperatureC,
            sample.pressureHpa,
            sample.altitudeM);
  sendSample(sample);
}

void I2cSensorManager::pollSgp(uint32_t now) {
  SensorSample sample = {};
  sample.kind = KIND_AIR;
  sample.ms = now;
  sample.sgpWarmup = sgp30StartMs_ == 0 || now - sgp30StartMs_ < 15000;

  LatestData copy;
  xSemaphoreTake(dataMutex_, portMAX_DELAY);
  copy = *latestData_;
  xSemaphoreGive(dataMutex_);

  if (sgp30Present_) {
    uint32_t ah = absoluteHumidityMgM3(copy.temperatureC, copy.humidityRh);
    if (ah) {
      sgp30SetHumidity(ah);
    }
    sample.ok = sgp30Read(&sample.eco2Ppm, &sample.tvocPpb);
  }
  DEBUG_LOG("[sensor:sgp30] present=%d ok=%d eco2_ppm=%u tvoc_ppb=%u warmup=%d\n",
            sgp30Present_ ? 1 : 0,
            sample.ok ? 1 : 0,
            sample.eco2Ppm,
            sample.tvocPpb,
            sample.sgpWarmup ? 1 : 0);
  sendSample(sample);
}

void I2cSensorManager::pollAs7341(uint32_t now) {
  SensorSample sample = {};
  sample.kind = KIND_LIGHT;
  sample.ms = now;
  sample.ok = as7341Present_ && as7341Read(sample.f, &sample.clear, &sample.nir);
  if (sample.ok) {
    sample.sunlightPresent = as7341IsSunlightLike(sample.f, sample.clear, sample.nir);
    sample.flickerHazard = as7341ReadFlickerStatus(&sample.flickerStatus) &&
                           as7341IsFlickerHazard(sample.flickerStatus);
  }
  if (!sample.ok && i2cDevicePresent(AS7341_ADDR)) {
    as7341Present_ = as7341Setup();
  }
  DEBUG_LOG("[sensor:as7341] ok=%d f=[%u,%u,%u,%u,%u,%u,%u,%u] clear=%u nir=%u sunlight=%d flicker_status=0x%02X hazard=%d\n",
            sample.ok ? 1 : 0,
            sample.f[0], sample.f[1], sample.f[2], sample.f[3],
            sample.f[4], sample.f[5], sample.f[6], sample.f[7],
            sample.clear,
            sample.nir,
            sample.sunlightPresent ? 1 : 0,
            sample.flickerStatus,
            sample.flickerHazard ? 1 : 0);
  sendSample(sample);
}
