#include "I2cSensorManager.h"

#include "../DebugLog.h"

bool I2cSensorManager::as7341WaitBitClear(uint8_t reg, uint8_t bitMask, uint16_t timeoutMs) {
  uint32_t start = millis();
  uint8_t v = 0;
  do {
    if (!i2cReadReg(AS7341_ADDR, reg, &v, 1)) {
      return false;
    }
    if ((v & bitMask) == 0) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  } while (millis() - start < timeoutMs);
  return false;
}

bool I2cSensorManager::as7341WaitDataReady(uint16_t timeoutMs) {
  uint32_t start = millis();
  uint8_t v = 0;
  do {
    if (!i2cReadReg(AS7341_ADDR, 0xA3, &v, 1)) {
      return false;
    }
    if (v & 0x40) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  } while (millis() - start < timeoutMs);
  return false;
}

bool I2cSensorManager::as7341WriteSmux(const uint8_t *smux) {
  if (!i2cWriteByte(AS7341_ADDR, 0x80, 0x01)) {
    return false;
  }
  if (!i2cWriteByte(AS7341_ADDR, 0xAF, 0x10)) {
    return false;
  }
  for (uint8_t i = 0; i < 20; i++) {
    if (!i2cWriteByte(AS7341_ADDR, i, smux[i])) {
      return false;
    }
  }
  if (!i2cWriteByte(AS7341_ADDR, 0x80, 0x11)) {
    return false;
  }
  return as7341WaitBitClear(0x80, 0x10, 100);
}

bool I2cSensorManager::as7341Setup() {
  if (!i2cDevicePresent(AS7341_ADDR)) {
    return false;
  }
  return i2cWriteByte(AS7341_ADDR, 0x80, 0x01) &&
         i2cWriteByte(AS7341_ADDR, 0x81, 0x64) &&
         i2cWriteByte(AS7341_ADDR, 0xCA, 0xE7) &&
         i2cWriteByte(AS7341_ADDR, 0xCB, 0x03) &&
         i2cWriteByte(AS7341_ADDR, 0xAA, 0x09);
}

bool I2cSensorManager::as7341ReadSix(uint16_t *out) {
  if (!i2cWriteByte(AS7341_ADDR, 0x80, 0x03)) {
    return false;
  }
  if (!as7341WaitDataReady(400)) {
    i2cWriteByte(AS7341_ADDR, 0x80, 0x01);
    return false;
  }
  uint8_t d[12];
  if (!i2cReadReg(AS7341_ADDR, 0x95, d, sizeof(d))) {
    return false;
  }
  for (uint8_t i = 0; i < 6; i++) {
    out[i] = le16(d + i * 2);
  }
  i2cWriteByte(AS7341_ADDR, 0x80, 0x01);
  return true;
}

bool I2cSensorManager::as7341Read(uint16_t f[8], uint16_t *clear, uint16_t *nir) {
  static const uint8_t smuxLo[20] = {
      0x30, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x50, 0x00,
      0x00, 0x00, 0x20, 0x04, 0x00, 0x30, 0x01, 0x50, 0x00, 0x06};
  static const uint8_t smuxHi[20] = {
      0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x10, 0x03, 0x50, 0x10,
      0x03, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x50, 0x00, 0x06};
  uint16_t lo[6];
  uint16_t hi[6];
  if (!as7341WriteSmux(smuxLo) || !as7341ReadSix(lo)) {
    return false;
  }
  if (!as7341WriteSmux(smuxHi) || !as7341ReadSix(hi)) {
    return false;
  }
  DEBUG_LOG("[sensor:as7341:raw] lo=[%u,%u,%u,%u,%u,%u] hi=[%u,%u,%u,%u,%u,%u]\n",
            lo[0], lo[1], lo[2], lo[3], lo[4], lo[5],
            hi[0], hi[1], hi[2], hi[3], hi[4], hi[5]);
  f[0] = lo[0];
  f[1] = lo[1];
  f[2] = lo[2];
  f[3] = lo[3];
  f[4] = hi[0];
  f[5] = hi[1];
  f[6] = hi[2];
  f[7] = hi[3];
  *clear = (lo[4] + hi[4]) / 2;
  *nir = (lo[5] + hi[5]) / 2;
  return true;
}

bool I2cSensorManager::as7341ReadFlickerStatus(uint8_t *status) {
  static const uint8_t smuxFlicker[20] = {
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x60};

  *status = 0;
  if (!i2cWriteByte(AS7341_ADDR, 0x80, 0x00)) {
    return false;
  }
  vTaskDelay(pdMS_TO_TICKS(2));
  if (!i2cWriteByte(AS7341_ADDR, 0x80, 0x01) ||
      !as7341WriteSmux(smuxFlicker) ||
      !i2cWriteByte(AS7341_ADDR, 0x80, 0x41)) {
    return false;
  }

  vTaskDelay(pdMS_TO_TICKS(500));
  bool ok = i2cReadReg(AS7341_ADDR, 0xDB, status, 1);
  i2cWriteByte(AS7341_ADDR, 0x80, 0x01);
  return ok;
}

bool I2cSensorManager::as7341IsSunlightLike(const uint16_t f[8], uint16_t clear, uint16_t nir) {
  uint32_t visible = 0;
  for (uint8_t i = 0; i < 8; i++) {
    visible += f[i];
  }
  if (clear < 50 || visible < 120) {
    return false;
  }

  float blueRatio = static_cast<float>(f[0] + f[1] + f[2]) / static_cast<float>(visible);
  float redRatio = static_cast<float>(f[6] + f[7]) / static_cast<float>(visible);
  float nirRatio = static_cast<float>(nir) / static_cast<float>(visible);
  uint16_t minVisible = f[0];
  uint16_t maxVisible = f[0];
  for (uint8_t i = 1; i < 8; i++) {
    minVisible = min(minVisible, f[i]);
    maxVisible = max(maxVisible, f[i]);
  }
  float spread = minVisible > 0 ? static_cast<float>(maxVisible) / static_cast<float>(minVisible) : 99.0f;

  return nirRatio >= 0.015f &&
         redRatio >= 0.18f &&
         blueRatio >= 0.08f &&
         spread <= 18.0f;
}

bool I2cSensorManager::as7341IsFlickerHazard(uint8_t status) {
  bool saturated = (status & 0x10) != 0;
  bool valid100Hz = (status & 0x04) != 0;
  bool valid120Hz = (status & 0x08) != 0;
  bool detected100Hz = (status & 0x01) != 0;
  bool detected120Hz = (status & 0x02) != 0;
  return !saturated && ((valid100Hz && detected100Hz) || (valid120Hz && detected120Hz));
}
