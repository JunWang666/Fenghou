#include "I2cSensorManager.h"

#include <Wire.h>

#include "../DebugLog.h"

bool I2cSensorManager::aht20Read(float *temperatureC, float *humidityRh) {
  uint8_t status = 0;
  if (!i2cReadReg(AHT20_ADDR, 0x71, &status, 1)) {
    return false;
  }
  if ((status & 0x18) != 0x18) {
    uint8_t initCmd[] = {0xBE, 0x08, 0x00};
    i2cWrite(AHT20_ADDR, initCmd, sizeof(initCmd));
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  uint8_t trig[] = {0xAC, 0x33, 0x00};
  if (!i2cWrite(AHT20_ADDR, trig, sizeof(trig))) {
    return false;
  }
  vTaskDelay(pdMS_TO_TICKS(80));

  uint8_t data[7];
  size_t got = Wire.requestFrom(AHT20_ADDR, static_cast<uint8_t>(7));
  if (got != 7) {
    DEBUG_LOG("[i2c:request-fail] sensor=aht20 addr=0x%02X len=7 got=%u\n",
              AHT20_ADDR,
              static_cast<unsigned int>(got));
    return false;
  }
  for (uint8_t i = 0; i < 7; i++) {
    data[i] = Wire.read();
  }
  if (data[0] & 0x80) {
    return false;
  }

  uint32_t rawHumidity = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
  uint32_t rawTemperature = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
  DEBUG_LOG("[sensor:aht20:raw] status=0x%02X bytes=%02X %02X %02X %02X %02X %02X %02X raw_h=%lu raw_t=%lu\n",
            data[0], data[0], data[1], data[2], data[3], data[4], data[5], data[6],
            static_cast<unsigned long>(rawHumidity),
            static_cast<unsigned long>(rawTemperature));
  *humidityRh = rawHumidity * 100.0f / 1048576.0f;
  *temperatureC = rawTemperature * 200.0f / 1048576.0f - 50.0f;
  return true;
}
