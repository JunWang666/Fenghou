#include "I2cSensorManager.h"

#include <Wire.h>

#include "../DebugLog.h"

void I2cSensorManager::scanI2cBus() {
  DEBUG_LOG("[i2c:scan] start sda=%d scl=%d clock=%lu\n",
            PIN_I2C_SDA,
            PIN_I2C_SCL,
            static_cast<unsigned long>(I2C_CLOCK_HZ));

  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 0x7F; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      found++;
      DEBUG_LOG("[i2c:scan] found addr=0x%02X%s%s%s%s%s\n",
                addr,
                addr == AHT20_ADDR ? " AHT20" : "",
                addr == AS7341_ADDR ? " AS7341" : "",
                addr == SGP30_ADDR ? " SGP30" : "",
                (addr == BMP280_ADDR_PRIMARY || addr == BMP280_ADDR_SECONDARY) ? " BMP280" : "",
                addr == ST25_USER_ADDR ? " ST25" : "");
    } else if (err == 4) {
      DEBUG_LOG("[i2c:scan] unknown error addr=0x%02X err=%u\n", addr, err);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  DEBUG_LOG("[i2c:scan] done found=%u\n", found);
}

bool I2cSensorManager::i2cWrite(uint8_t addr, const uint8_t *data, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(data, len);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    DEBUG_LOG("[i2c:write-fail] addr=0x%02X len=%u err=%u\n",
              addr,
              static_cast<unsigned int>(len),
              err);
    return false;
  }
  return true;
}

bool I2cSensorManager::i2cWriteByte(uint8_t addr, uint8_t reg, uint8_t value) {
  uint8_t data[2] = {reg, value};
  return i2cWrite(addr, data, sizeof(data));
}

bool I2cSensorManager::i2cReadReg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) {
    DEBUG_LOG("[i2c:read-reg-fail] phase=write-addr addr=0x%02X reg=0x%02X len=%u err=%u\n",
              addr,
              reg,
              static_cast<unsigned int>(len),
              err);
    return false;
  }
  size_t got = Wire.requestFrom(addr, static_cast<uint8_t>(len));
  if (got != len) {
    DEBUG_LOG("[i2c:read-reg-fail] phase=request addr=0x%02X reg=0x%02X len=%u got=%u\n",
              addr,
              reg,
              static_cast<unsigned int>(len),
              static_cast<unsigned int>(got));
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

bool I2cSensorManager::i2cDevicePresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission();
  DEBUG_LOG("[i2c:probe] addr=0x%02X present=%d err=%u\n",
            addr,
            err == 0 ? 1 : 0,
            err);
  return err == 0;
}

uint16_t I2cSensorManager::le16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

int16_t I2cSensorManager::sle16(const uint8_t *p) {
  return static_cast<int16_t>(le16(p));
}
