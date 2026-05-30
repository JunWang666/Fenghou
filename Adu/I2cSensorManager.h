#pragma once

#include <Arduino.h>

#include "ConfigStore.h"
#include "Types.h"

class I2cSensorManager {
public:
  void begin(QueueHandle_t sampleQueue,
             ConfigStore *configStore,
             SemaphoreHandle_t dataMutex,
             LatestData *latestData);
  void run();

private:
  QueueHandle_t sampleQueue_ = nullptr;
  ConfigStore *configStore_ = nullptr;
  SemaphoreHandle_t dataMutex_ = nullptr;
  LatestData *latestData_ = nullptr;

  uint8_t bmp280Addr_ = BMP280_ADDR_PRIMARY;
  bool bmp280Present_ = false;
  uint16_t bmpDigT1_ = 0;
  int16_t bmpDigT2_ = 0;
  int16_t bmpDigT3_ = 0;
  uint16_t bmpDigP1_ = 0;
  int16_t bmpDigP2_ = 0;
  int16_t bmpDigP3_ = 0;
  int16_t bmpDigP4_ = 0;
  int16_t bmpDigP5_ = 0;
  int16_t bmpDigP6_ = 0;
  int16_t bmpDigP7_ = 0;
  int16_t bmpDigP8_ = 0;
  int16_t bmpDigP9_ = 0;
  int32_t bmpTfine_ = 0;

  uint32_t sgp30StartMs_ = 0;
  bool sgp30Present_ = false;
  bool as7341Present_ = false;

  void sendSample(const SensorSample &sample);
  void pollNfc(uint32_t now);
  void pollAht(uint32_t now);
  void pollBmp(uint32_t now);
  void pollSgp(uint32_t now);
  void pollAs7341(uint32_t now);

  bool i2cWrite(uint8_t addr, const uint8_t *data, size_t len);
  bool i2cWriteByte(uint8_t addr, uint8_t reg, uint8_t value);
  bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);
  bool i2cDevicePresent(uint8_t addr);

  static uint16_t le16(const uint8_t *p);
  static int16_t sle16(const uint8_t *p);
  static uint8_t crc8Sgp30(const uint8_t *data, uint8_t len);

  bool sgp30Command(const uint8_t *cmd,
                    uint8_t cmdLen,
                    uint16_t delayMs,
                    uint16_t *words = nullptr,
                    uint8_t wordCount = 0);
  bool sgp30Begin();
  uint32_t absoluteHumidityMgM3(float temperatureC, float humidityRh);
  bool sgp30SetHumidity(uint32_t absoluteHumidity);
  bool sgp30Read(uint16_t *eco2, uint16_t *tvoc);

  bool aht20Read(float *temperatureC, float *humidityRh);

  bool bmp280ReadCoefficients(uint8_t addr);
  bool bmp280Begin();
  bool bmp280Read(float *temperatureC, float *pressureHpa, float *altitudeM);

  bool as7341WaitBitClear(uint8_t reg, uint8_t bitMask, uint16_t timeoutMs);
  bool as7341WaitDataReady(uint16_t timeoutMs);
  bool as7341WriteSmux(const uint8_t *smux);
  bool as7341Setup();
  bool as7341ReadSix(uint16_t *out);
  bool as7341Read(uint16_t f[8], uint16_t *clear, uint16_t *nir);

  bool st25ReadUserMemory(char *buf, size_t len);
};
