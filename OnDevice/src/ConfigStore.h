#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "sensor/Types.h"

class ConfigStore {
public:
  bool begin(SemaphoreHandle_t mutex);
  void load();
  void save(const DeviceConfig &cfg);
  void getCopy(DeviceConfig *out);
  bool applyPayload(const char *raw);

private:
  DeviceConfig config_;
  Preferences prefs_;
  SemaphoreHandle_t mutex_ = nullptr;
  uint32_t lastPayloadHash_ = 0;

  void setConfig(const DeviceConfig &cfg);
  static uint32_t fnv1a(const char *s);
  static bool extractValue(const char *src, const char *key, char *out, size_t outLen);
  static bool extractUint32(const char *src, const char *key, uint32_t *out);
  static bool parseConfigPayload(const char *raw, DeviceConfig *cfg);
};
