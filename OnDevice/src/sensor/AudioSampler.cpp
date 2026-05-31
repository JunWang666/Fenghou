#include "AudioSampler.h"

#include <math.h>

#include "../DebugLog.h"

#include "driver/i2s.h"

void AudioSampler::begin(QueueHandle_t sampleQueue) {
  sampleQueue_ = sampleQueue;
}

void AudioSampler::run() {
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
#ifdef I2S_COMM_FORMAT_STAND_I2S
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
#else
  cfg.communication_format = I2S_COMM_FORMAT_I2S;
#endif
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_I2S_SD;

  esp_err_t ok = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  if (ok == ESP_OK) {
    ok = i2s_set_pin(I2S_NUM_0, &pins);
  }
  if (ok != ESP_OK) {
    Serial.printf("I2S init failed: %d\n", ok);
  }

  int32_t samples[256];
  uint32_t windowStart = millis();
  double sumSq = 0.0;
  float peak = 0.0f;
  uint32_t count = 0;

  for (;;) {
    size_t bytesRead = 0;
    if (ok == ESP_OK && i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(200)) == ESP_OK) {
      uint32_t n = bytesRead / sizeof(int32_t);
      for (uint32_t i = 0; i < n; i++) {
        int32_t v = samples[i] >> 14;
        float norm = fabsf(v / 131072.0f);
        sumSq += norm * norm;
        if (norm > peak) {
          peak = norm;
        }
      }
      count += n;
    }

    if (millis() - windowStart >= 1000) {
      float rms = count ? sqrtf(sumSq / count) : 0.0f;
      float db = rms >= AUDIO_MIN_RMS_FOR_DB ? 20.0f * log10f(rms) + AUDIO_DB_SPL_OFFSET : 0.0f;
      db = constrain(db, 0.0f, 130.0f);
      uint8_t level = static_cast<uint8_t>(roundf(db));
      SensorSample sample = {};
      sample.kind = KIND_SOUND;
      sample.ms = millis();
      sample.ok = ok == ESP_OK && count > 0;
      sample.soundRms = rms;
      sample.soundPeak = min(peak, 1.0f);
      sample.soundDb = db;
      sample.soundLevel = level;
      DEBUG_LOG("[sensor:audio] ok=%d sample_count=%lu rms=%.5f peak=%.5f db=%.1f level=%u\n",
                sample.ok ? 1 : 0,
                static_cast<unsigned long>(count),
                sample.soundRms,
                sample.soundPeak,
                sample.soundDb,
                sample.soundLevel);
      xQueueSend(sampleQueue_, &sample, QUEUE_WAIT);

      windowStart = millis();
      sumSq = 0.0;
      peak = 0.0f;
      count = 0;
    }
  }
}
