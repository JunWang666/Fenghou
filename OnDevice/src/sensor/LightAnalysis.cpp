#include "LightAnalysis.h"

namespace {

uint32_t lastCheckTime = 0;
uint32_t sunlightExposureMs = 0;

} // namespace

namespace LightAnalysis {

bool isStrictSunlight(const uint16_t f[8], uint16_t clear, uint16_t nir) {
  uint32_t visible = 0;
  for (uint8_t i = 0; i < 8; i++) {
    visible += f[i];
  }

  if (clear < 200 || visible < 300) {
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

  return nirRatio >= 0.15f &&
         redRatio >= 0.18f &&
         blueRatio >= 0.08f &&
         spread <= 10.0f;
}

bool updateExposureTracking(const uint16_t f[8], uint16_t clear, uint16_t nir, uint32_t currentTime) {
  bool strictSunlight = isStrictSunlight(f, clear, nir);

  if (lastCheckTime == 0 || currentTime < lastCheckTime) {
    lastCheckTime = currentTime;
    return strictSunlight;
  }

  uint32_t deltaTime = currentTime - lastCheckTime;
  lastCheckTime = currentTime;

  if (strictSunlight) {
    sunlightExposureMs += deltaTime;
  }

  return strictSunlight;
}

uint32_t getSunlightExposureMinutes() {
  return sunlightExposureMs / 60000UL;
}

} // namespace LightAnalysis
