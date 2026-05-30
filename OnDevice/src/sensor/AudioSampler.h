#pragma once

#include <Arduino.h>

#include "Types.h"

class AudioSampler {
public:
  void begin(QueueHandle_t sampleQueue);
  void run();

private:
  QueueHandle_t sampleQueue_ = nullptr;
};
