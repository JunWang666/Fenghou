#include "I2cSensorManager.h"

#include <Wire.h>

#include "../DebugLog.h"

void I2cSensorManager::initializeNfcViewLink() {
  DeviceConfig cfg = {};
  if (configStore_) {
    configStore_->getCopy(&cfg);
  }
  const char *deviceId = cfg.deviceId[0] ? cfg.deviceId : DEFAULT_DEVICE_ID;

  char url[96];
  snprintf(url, sizeof(url), "%s%s", DEFAULT_VIEW_URL_PREFIX, deviceId);

  static const char httpsPrefix[] = "https://";
  const char *uriBody = url;
  uint8_t uriPrefixCode = 0x00;
  if (strncmp(url, httpsPrefix, strlen(httpsPrefix)) == 0) {
    uriPrefixCode = 0x04;
    uriBody = url + strlen(httpsPrefix);
  }

  const size_t uriBodyLen = strlen(uriBody);
  const size_t payloadLen = 1 + uriBodyLen;
  const size_t recordLen = 4 + payloadLen;
  const size_t ndefLen = 4 + 2 + recordLen + 1;
  if (payloadLen > 255 || ndefLen > 128) {
    Serial.println("NFC view link too long.");
    return;
  }

  uint8_t ndef[128] = {};
  size_t i = 0;
  ndef[i++] = 0xE1;
  ndef[i++] = 0x40;
  ndef[i++] = 0x40;
  ndef[i++] = 0x05;
  ndef[i++] = 0x03;
  ndef[i++] = static_cast<uint8_t>(recordLen);
  ndef[i++] = 0xD1;
  ndef[i++] = 0x01;
  ndef[i++] = static_cast<uint8_t>(payloadLen);
  ndef[i++] = 0x55;
  ndef[i++] = uriPrefixCode;
  memcpy(ndef + i, uriBody, uriBodyLen);
  i += uriBodyLen;
  ndef[i++] = 0xFE;

  if (st25WriteUserMemory(0, ndef, i)) {
    Serial.print("NFC view link initialized: ");
    Serial.println(url);
  } else {
    Serial.println("NFC view link init failed.");
  }
}

bool I2cSensorManager::st25WriteUserMemory(uint16_t offset, const uint8_t *data, size_t len) {
  const size_t pageSize = 4;
  size_t written = 0;
  while (written < len) {
    size_t chunk = min(pageSize, len - written);
    Wire.beginTransmission(ST25_USER_ADDR);
    Wire.write(static_cast<uint8_t>((offset + written) >> 8));
    Wire.write(static_cast<uint8_t>((offset + written) & 0xFF));
    Wire.write(data + written, chunk);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
      DEBUG_LOG("[sensor:nfc:write-fail] offset=%u len=%u err=%u\n",
                static_cast<unsigned int>(offset + written),
                static_cast<unsigned int>(chunk),
                err);
      return false;
    }
    written += chunk;
    vTaskDelay(pdMS_TO_TICKS(6));
  }
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
