#include <Arduino.h>
#include <WiFi.h>

#include "ConfigStore.h"
#include "DataAggregator.h"
#include "module/NetworkUploader.h"
#include "sensor/AudioSampler.h"
#include "sensor/I2cSensorManager.h"
#include "sensor/Types.h"

namespace {
QueueHandle_t sensorQueue;
QueueHandle_t uploadQueue;
SemaphoreHandle_t dataMutex;
SemaphoreHandle_t configMutex;

LatestData latestData;
ConfigStore configStore;
I2cSensorManager i2cSensors;
AudioSampler audioSampler;
DataAggregator dataAggregator;
NetworkUploader networkUploader;

String wifiHostname;

String normalizedHostnameFromDeviceId(const char *deviceId) {
  String hostname = "fenghou-";
  hostname += deviceId;
  hostname.toLowerCase();

  for (size_t i = 0; i < hostname.length(); i++) {
    char c = hostname.charAt(i);
    bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    if (!allowed) {
      hostname.setCharAt(i, '-');
    }
  }

  return hostname;
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_STA_START:
    Serial.println("[wifi-event] STA start");
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    Serial.println("[wifi-event] STA connected to AP");
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    Serial.print("[wifi-event] got IP: ");
    Serial.println(WiFi.localIP());
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    Serial.print("[wifi-event] STA disconnected, reason=");
    Serial.println(info.wifi_sta_disconnected.reason);
    break;
  default:
    break;
  }
}

void taskI2CSensors(void *) {
  i2cSensors.run();
}

void taskAudio(void *) {
  audioSampler.run();
}

void taskDataProcess(void *) {
  dataAggregator.run();
}

void taskNetworkUpload(void *) {
  networkUploader.run();
}
} // namespace

void setup() {
  Serial.begin(115200);
  unsigned long serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {
    delay(10);
  }
  delay(2000);

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
  wifiHostname = normalizedHostnameFromDeviceId(cfg.deviceId);
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(wifiHostname.c_str());
  WiFi.persistent(false);

  Serial.println("Fenghou on-device monitor booting");
  Serial.printf("Config: valid=%d ssid=%s server=%s device=%s\n",
                cfg.valid,
                cfg.ssid,
                cfg.server,
                cfg.deviceId);
  Serial.print("WiFi hostname: ");
  Serial.println(wifiHostname);

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
