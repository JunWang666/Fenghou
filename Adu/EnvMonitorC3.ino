#include <Arduino.h>

#include "AudioSampler.h"
#include "ConfigStore.h"
#include "DataAggregator.h"
#include "I2cSensorManager.h"
#include "NetworkUploader.h"
#include "Types.h"

static QueueHandle_t sensorQueue;
static QueueHandle_t uploadQueue;
static SemaphoreHandle_t dataMutex;
static SemaphoreHandle_t configMutex;

static LatestData latestData;
static ConfigStore configStore;
static I2cSensorManager i2cSensors;
static AudioSampler audioSampler;
static DataAggregator dataAggregator;
static NetworkUploader networkUploader;

static void taskI2CSensors(void *) {
  i2cSensors.run();
}

static void taskAudio(void *) {
  audioSampler.run();
}

static void taskDataProcess(void *) {
  dataAggregator.run();
}

static void taskNetworkUpload(void *) {
  networkUploader.run();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  sensorQueue = xQueueCreate(12, sizeof(SensorSample));
  uploadQueue = xQueueCreate(3, sizeof(LatestData));
  dataMutex = xSemaphoreCreateMutex();
  configMutex = xSemaphoreCreateMutex();
  if (!sensorQueue || !uploadQueue || !dataMutex || !configMutex) {
    Serial.println("FreeRTOS allocation failed.");
    while (true) {
      delay(1000);
    }
  }

  if (!configStore.begin(configMutex)) {
    Serial.println("Preferences init failed.");
  }
  configStore.load();

  DeviceConfig cfg;
  configStore.getCopy(&cfg);
  Serial.printf("Config: valid=%d ssid=%s server=%s device=%s\n",
                cfg.valid,
                cfg.ssid,
                cfg.server,
                cfg.deviceId);

  i2cSensors.begin(sensorQueue, &configStore, dataMutex, &latestData);
  audioSampler.begin(sensorQueue);
  dataAggregator.begin(sensorQueue, uploadQueue, dataMutex, &latestData, &configStore);
  networkUploader.begin(uploadQueue, &configStore);

  xTaskCreate(taskI2CSensors, "i2c", 6144, nullptr, 2, nullptr);
  xTaskCreate(taskAudio, "audio", 4096, nullptr, 3, nullptr);
  xTaskCreate(taskDataProcess, "data", 4096, nullptr, 1, nullptr);
  xTaskCreate(taskNetworkUpload, "upload", 6144, nullptr, 2, nullptr);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
