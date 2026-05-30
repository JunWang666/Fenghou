#include "I2cSensorManager.h"

#include <math.h>

#include "../DebugLog.h"

bool I2cSensorManager::bmp280ReadCoefficients(uint8_t addr) {
  uint8_t c[24];
  if (!i2cReadReg(addr, 0x88, c, sizeof(c))) {
    return false;
  }
  bmpDigT1_ = le16(c + 0);
  bmpDigT2_ = sle16(c + 2);
  bmpDigT3_ = sle16(c + 4);
  bmpDigP1_ = le16(c + 6);
  bmpDigP2_ = sle16(c + 8);
  bmpDigP3_ = sle16(c + 10);
  bmpDigP4_ = sle16(c + 12);
  bmpDigP5_ = sle16(c + 14);
  bmpDigP6_ = sle16(c + 16);
  bmpDigP7_ = sle16(c + 18);
  bmpDigP8_ = sle16(c + 20);
  bmpDigP9_ = sle16(c + 22);
  return bmpDigP1_ != 0;
}

bool I2cSensorManager::bmp280Begin() {
  uint8_t id = 0;
  if (i2cReadReg(BMP280_ADDR_PRIMARY, 0xD0, &id, 1) && id == 0x58) {
    bmp280Addr_ = BMP280_ADDR_PRIMARY;
  } else if (i2cReadReg(BMP280_ADDR_SECONDARY, 0xD0, &id, 1) && id == 0x58) {
    bmp280Addr_ = BMP280_ADDR_SECONDARY;
  } else {
    return false;
  }
  if (!bmp280ReadCoefficients(bmp280Addr_)) {
    return false;
  }
  if (!i2cWriteByte(bmp280Addr_, 0xF4, 0x2F)) {
    return false;
  }
  return i2cWriteByte(bmp280Addr_, 0xF5, 0x90);
}

bool I2cSensorManager::bmp280Read(float *temperatureC, float *pressureHpa, float *altitudeM) {
  uint8_t d[6];
  if (!bmp280Present_ || !i2cReadReg(bmp280Addr_, 0xF7, d, sizeof(d))) {
    return false;
  }
  int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
  DEBUG_LOG("[sensor:bmp280:raw] addr=0x%02X bytes=%02X %02X %02X %02X %02X %02X adc_p=%ld adc_t=%ld\n",
            bmp280Addr_,
            d[0], d[1], d[2], d[3], d[4], d[5],
            static_cast<long>(adcP),
            static_cast<long>(adcT));

  int32_t var1 = ((((adcT >> 3) - ((int32_t)bmpDigT1_ << 1))) * ((int32_t)bmpDigT2_)) >> 11;
  int32_t var2 = (((((adcT >> 4) - ((int32_t)bmpDigT1_)) * ((adcT >> 4) - ((int32_t)bmpDigT1_))) >> 12) *
                  ((int32_t)bmpDigT3_)) >> 14;
  bmpTfine_ = var1 + var2;
  *temperatureC = ((bmpTfine_ * 5 + 128) >> 8) / 100.0f;

  int64_t pVar1 = ((int64_t)bmpTfine_) - 128000;
  int64_t pVar2 = pVar1 * pVar1 * (int64_t)bmpDigP6_;
  pVar2 = pVar2 + ((pVar1 * (int64_t)bmpDigP5_) << 17);
  pVar2 = pVar2 + (((int64_t)bmpDigP4_) << 35);
  pVar1 = ((pVar1 * pVar1 * (int64_t)bmpDigP3_) >> 8) + ((pVar1 * (int64_t)bmpDigP2_) << 12);
  pVar1 = (((((int64_t)1) << 47) + pVar1)) * ((int64_t)bmpDigP1_) >> 33;
  if (pVar1 == 0) {
    return false;
  }
  int64_t p = 1048576 - adcP;
  p = (((p << 31) - pVar2) * 3125) / pVar1;
  pVar1 = (((int64_t)bmpDigP9_) * (p >> 13) * (p >> 13)) >> 25;
  pVar2 = (((int64_t)bmpDigP8_) * p) >> 19;
  p = ((p + pVar1 + pVar2) >> 8) + (((int64_t)bmpDigP7_) << 4);
  *pressureHpa = (p / 256.0f) / 100.0f;
  *altitudeM = 44330.0f * (1.0f - powf(*pressureHpa / 1013.25f, 0.1903f));
  return true;
}
