#pragma once

#include <Arduino.h>

namespace LightAnalysis {

bool isStrictSunlight(const uint16_t f[8], uint16_t clear, uint16_t nir);
uint8_t updateRecentMinuteResult(const uint16_t f[8], uint16_t clear, uint16_t nir, uint32_t currentTime);

} // namespace LightAnalysis
