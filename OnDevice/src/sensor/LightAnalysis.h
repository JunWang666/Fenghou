#pragma once

#include <Arduino.h>

namespace LightAnalysis {

bool isStrictSunlight(const uint16_t f[8], uint16_t clear, uint16_t nir);
bool updateExposureTracking(const uint16_t f[8], uint16_t clear, uint16_t nir, uint32_t currentTime);
uint32_t getSunlightExposureMinutes();

} // namespace LightAnalysis
