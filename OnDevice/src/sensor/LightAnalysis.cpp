#include "LightAnalysis.h"

namespace {

static constexpr uint32_t SUNLIGHT_RESULT_WINDOW_MS = 60000;
static constexpr uint8_t SUNLIGHT_SAMPLE_HISTORY_SIZE = 32;

struct SunlightSample {
  uint32_t time = 0;
  bool strictSunlight = false;
};

SunlightSample sunlightSamples[SUNLIGHT_SAMPLE_HISTORY_SIZE];
uint8_t nextSunlightSample = 0;
uint8_t sunlightSampleCount = 0;

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

uint8_t updateRecentMinuteResult(const uint16_t f[8], uint16_t clear, uint16_t nir, uint32_t currentTime) {
  bool strictSunlight = isStrictSunlight(f, clear, nir);

  sunlightSamples[nextSunlightSample].time = currentTime;
  sunlightSamples[nextSunlightSample].strictSunlight = strictSunlight;
  nextSunlightSample = (nextSunlightSample + 1) % SUNLIGHT_SAMPLE_HISTORY_SIZE;
  if (sunlightSampleCount < SUNLIGHT_SAMPLE_HISTORY_SIZE) {
    sunlightSampleCount++;
  }

  uint8_t recentSamples = 0;
  uint8_t recentStrictSamples = 0;
  for (uint8_t i = 0; i < sunlightSampleCount; i++) {
    const SunlightSample &sample = sunlightSamples[i];
    if (currentTime < sample.time || currentTime - sample.time > SUNLIGHT_RESULT_WINDOW_MS) {
      continue;
    }
    recentSamples++;
    if (sample.strictSunlight) {
      recentStrictSamples++;
    }
  }

  return recentSamples > 0 && recentStrictSamples * 2 >= recentSamples ? 1 : 0;
}

} // namespace LightAnalysis
