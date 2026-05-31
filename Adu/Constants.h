#pragma once

#include <Arduino.h>

// ESP32-C3 pin plan from the design:
// I2C: GPIO4 SDA, GPIO5 SCL
// INMP441: GPIO6 BCLK, GPIO7 WS, GPIO10 SD/DOUT, L/R tied to GND
static constexpr int PIN_I2C_SDA = 4;
static constexpr int PIN_I2C_SCL = 5;
static constexpr int PIN_I2S_BCLK = 6;
static constexpr int PIN_I2S_WS = 7;
static constexpr int PIN_I2S_SD = 10;

static constexpr uint32_t I2C_CLOCK_HZ = 100000;
static constexpr TickType_t QUEUE_WAIT = pdMS_TO_TICKS(20);

static constexpr uint8_t AHT20_ADDR = 0x38;
static constexpr uint8_t AS7341_ADDR = 0x39;
static constexpr uint8_t SGP30_ADDR = 0x58;
static constexpr uint8_t BMP280_ADDR_PRIMARY = 0x76;
static constexpr uint8_t BMP280_ADDR_SECONDARY = 0x77;
static constexpr uint8_t ST25_USER_ADDR = 0x53;

static constexpr const char *CONFIG_BEGIN = "CFG:";

static constexpr uint32_t DEFAULT_UPLOAD_INTERVAL_MS = 10000;
static constexpr uint32_t MIN_UPLOAD_INTERVAL_MS = 5000;
static constexpr uint32_t MAX_UPLOAD_INTERVAL_MS = 300000;
