#pragma once

#include <Arduino.h>
#include <math.h>

#include "Constants.h"
#include "DeviceIdentity.h"

enum SensorKind : uint8_t {
  KIND_AHT,
  KIND_BMP,
  KIND_AIR,
  KIND_LIGHT,
  KIND_SOUND,
  KIND_CONFIG
};

struct DeviceConfig {
  DeviceConfig() {
    setChipDeviceId(deviceId, sizeof(deviceId));
  }

  char ssid[33] = "";
  char password[65] = "";
  char server[129] = "";
  char deviceId[33] = "";
  char token[97] = "";
  uint32_t uploadIntervalMs = DEFAULT_UPLOAD_INTERVAL_MS;
  bool valid = false;
};

struct SensorSample {
  SensorKind kind;
  uint32_t ms;
  bool ok;
  float temperatureC;
  float humidityRh;
  float pressureHpa;
  float altitudeM;
  uint16_t eco2Ppm;
  uint16_t tvocPpb;
  bool sgpWarmup;
  uint16_t f[8];
  uint16_t clear;
  uint16_t nir;
  float soundRms;
  float soundPeak;
  uint8_t soundLevel;
};

struct LatestData {
  uint32_t ms = 0;
  bool ahtOk = false;
  bool bmpOk = false;
  bool sgpOk = false;
  bool as7341Ok = false;
  bool audioOk = false;
  float temperatureC = NAN;
  float humidityRh = NAN;
  float pressureHpa = NAN;
  float altitudeM = NAN;
  uint16_t eco2Ppm = 0;
  uint16_t tvocPpb = 0;
  bool sgpWarmup = true;
  uint16_t f[8] = {};
  uint16_t clear = 0;
  uint16_t nir = 0;
  float soundRms = 0.0f;
  float soundPeak = 0.0f;
  uint8_t soundLevel = 0;
  int8_t wifiRssi = 0;
  uint32_t uptimeS = 0;
};
