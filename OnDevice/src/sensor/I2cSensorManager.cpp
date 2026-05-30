#include "I2cSensorManager.h"

#include <Wire.h>
#include <math.h>

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

    if (now - lastNfc >= 5000) {
      lastNfc = now;
      pollNfc(now);
    }
    if (now - lastAht >= 2000) {
      lastAht = now;
      pollAht(now);
    }
    if (now - lastBmp >= 5000) {
      lastBmp = now;
      pollBmp(now);
    }
    if (now - lastSgp >= 1000) {
      lastSgp = now;
      pollSgp(now);
    }
    if (now - lastAs >= 5000) {
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
  if (!sample.ok && i2cDevicePresent(AS7341_ADDR)) {
    as7341Present_ = as7341Setup();
  }
  DEBUG_LOG("[sensor:as7341] ok=%d f=[%u,%u,%u,%u,%u,%u,%u,%u] clear=%u nir=%u\n",
            sample.ok ? 1 : 0,
            sample.f[0], sample.f[1], sample.f[2], sample.f[3],
            sample.f[4], sample.f[5], sample.f[6], sample.f[7],
            sample.clear,
            sample.nir);
  sendSample(sample);
}

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

bool I2cSensorManager::st25ReadUserMemory(char *buf, size_t len) {
  if (len == 0) {
    return false;
  }
  memset(buf, 0, len);
  Wire.beginTransmission(ST25_USER_ADDR);
  Wire.write(0x00);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  size_t toRead = min(len - 1, static_cast<size_t>(220));
  size_t got = Wire.requestFrom(ST25_USER_ADDR, static_cast<uint8_t>(toRead));
  if (got != toRead) {
    DEBUG_LOG("[i2c:request-fail] sensor=st25 addr=0x%02X len=%u got=%u\n",
              ST25_USER_ADDR,
              static_cast<unsigned int>(toRead),
              static_cast<unsigned int>(got));
  }
  for (size_t i = 0; i < got && i < len - 1; i++) {
    char c = static_cast<char>(Wire.read());
    buf[i] = isPrintable(c) ? c : ' ';
  }
  buf[min(got, len - 1)] = 0;
  return got > 0;
}
