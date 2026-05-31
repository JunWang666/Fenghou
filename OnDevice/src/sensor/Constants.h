#pragma once

#include <Arduino.h>

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#error "This firmware is configured for ESP32-S3. Select an ESP32-S3 board."
#endif

#ifndef ENV_BOARD_NAME
#define ENV_BOARD_NAME "ESP32-S3"
#endif

// ESP32-S3 default pin plan.
// I2C: GPIO8 SDA, GPIO9 SCL
// INMP441: GPIO15 BCLK/SCK, GPIO16 WS/LRCL, GPIO17 SD/DOUT, L/R tied to GND
// Override these PIN_* macros from build flags if your S3 board already uses them.
#ifndef PIN_I2C_SDA
static constexpr int PIN_I2C_SDA = 8;
#endif
#ifndef PIN_I2C_SCL
static constexpr int PIN_I2C_SCL = 9;
#endif
#ifndef PIN_I2S_BCLK
static constexpr int PIN_I2S_BCLK = 15;
#endif
#ifndef PIN_I2S_WS
static constexpr int PIN_I2S_WS = 16;
#endif
#ifndef PIN_I2S_SD
static constexpr int PIN_I2S_SD = 17;
#endif

static constexpr uint32_t I2C_CLOCK_HZ = 100000;
static constexpr TickType_t QUEUE_WAIT = pdMS_TO_TICKS(20);

static constexpr uint8_t AHT20_ADDR = 0x38;
static constexpr uint8_t AS7341_ADDR = 0x39;
static constexpr uint8_t SGP30_ADDR = 0x58;
static constexpr uint8_t BMP280_ADDR_PRIMARY = 0x76;
static constexpr uint8_t BMP280_ADDR_SECONDARY = 0x77;
static constexpr uint8_t ST25_USER_ADDR = 0x53;


static constexpr const char *DEFAULT_SERVER = DEFAULT_UPLOAD_URL;
static constexpr const char *DEFAULT_VIEW_URL_PREFIX = "https://fenghou.goudaijun.top/view/";
static constexpr const char *CONFIG_BEGIN = "CFG:";

static constexpr uint32_t AHT_SAMPLE_INTERVAL_MS = 1000;
static constexpr uint32_t BMP_SAMPLE_INTERVAL_MS = 1000;
static constexpr uint32_t SGP_SAMPLE_INTERVAL_MS = 1000;
static constexpr uint32_t AS7341_SAMPLE_INTERVAL_MS = 2000;
static constexpr uint32_t NFC_POLL_INTERVAL_MS = 5000;
static constexpr uint32_t AUDIO_SAMPLE_INTERVAL_MS = 1000;
static constexpr uint32_t AUDIO_NOISE_WINDOW_MS = 60000;
static constexpr float AUDIO_DB_SPL_OFFSET = 60.0f;
static constexpr float AUDIO_MIN_RMS_FOR_DB = 0.000001f;
static constexpr float AUDIO_HIGH_VOLUME_DB = 85.0f;

static constexpr uint32_t DEFAULT_UPLOAD_INTERVAL_MS = 30000;
static constexpr uint32_t MIN_UPLOAD_INTERVAL_MS = 30000;
static constexpr uint32_t MAX_UPLOAD_INTERVAL_MS = 300000;
