#include "I2cSensorManager.h"

#include <Wire.h>
#include <math.h>

#include "../DebugLog.h"

uint8_t I2cSensorManager::crc8Sgp30(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
  }
  return crc;
}

bool I2cSensorManager::sgp30Command(const uint8_t *cmd,
                                    uint8_t cmdLen,
                                    uint16_t delayMs,
                                    uint16_t *words,
                                    uint8_t wordCount) {
  if (!i2cWrite(SGP30_ADDR, cmd, cmdLen)) {
    return false;
  }
  vTaskDelay(pdMS_TO_TICKS(delayMs));
  if (wordCount == 0) {
    return true;
  }

  const uint8_t replyLen = wordCount * 3;
  uint8_t reply[18];
  if (replyLen > sizeof(reply)) {
    return false;
  }
  size_t got = Wire.requestFrom(SGP30_ADDR, replyLen);
  if (got != replyLen) {
    DEBUG_LOG("[i2c:request-fail] sensor=sgp30 addr=0x%02X len=%u got=%u\n",
              SGP30_ADDR,
              replyLen,
              static_cast<unsigned int>(got));
    return false;
  }
  for (uint8_t i = 0; i < replyLen; i++) {
    reply[i] = Wire.read();
  }
  for (uint8_t i = 0; i < wordCount; i++) {
    uint8_t *p = &reply[i * 3];
    if (crc8Sgp30(p, 2) != p[2]) {
      return false;
    }
    words[i] = (static_cast<uint16_t>(p[0]) << 8) | p[1];
  }
  return true;
}

bool I2cSensorManager::sgp30Begin() {
  uint8_t iaqInit[] = {0x20, 0x03};
  if (!sgp30Command(iaqInit, sizeof(iaqInit), 10)) {
    return false;
  }
  sgp30StartMs_ = millis();
  return true;
}

uint32_t I2cSensorManager::absoluteHumidityMgM3(float temperatureC, float humidityRh) {
  if (!isfinite(temperatureC) || !isfinite(humidityRh)) {
    return 0;
  }
  const float ah = 216.7f *
                   ((humidityRh / 100.0f) * 6.112f *
                    expf((17.62f * temperatureC) / (243.12f + temperatureC)) /
                    (273.15f + temperatureC));
  if (ah <= 0.0f) {
    return 0;
  }
  return static_cast<uint32_t>(ah * 1000.0f);
}

bool I2cSensorManager::sgp30SetHumidity(uint32_t absoluteHumidity) {
  if (absoluteHumidity > 256000) {
    return false;
  }
  uint16_t scaled = static_cast<uint16_t>(((uint64_t)absoluteHumidity * 256 * 16777) >> 24);
  uint8_t cmd[5] = {0x20, 0x61, static_cast<uint8_t>(scaled >> 8), static_cast<uint8_t>(scaled & 0xFF), 0};
  cmd[4] = crc8Sgp30(cmd + 2, 2);
  return sgp30Command(cmd, sizeof(cmd), 10);
}

bool I2cSensorManager::sgp30Read(uint16_t *eco2, uint16_t *tvoc) {
  uint8_t cmd[] = {0x20, 0x08};
  uint16_t words[2];
  if (!sgp30Command(cmd, sizeof(cmd), 12, words, 2)) {
    return false;
  }
  *eco2 = words[0];
  *tvoc = words[1];
  DEBUG_LOG("[sensor:sgp30:raw] words=%u,%u\n", words[0], words[1]);
  return true;
}
