#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#ifndef DEVICE_ID
#define DEVICE_ID "test_1"
#endif

namespace {
const char* kDeviceId = DEVICE_ID;
const char* kUploadUrl = "https://fenghou.goudaijun.top/api/device/upload";
const unsigned long kUploadIntervalMs = 10000;
const unsigned long kWifiConnectTimeoutMs = 15000;
const unsigned long kBootLogDelayMs = 5000;

WiFiClientSecure secureClient;
String wifiHostname;
unsigned long lastUploadMs = 0;

String normalizedHostnameFromDeviceId() {
    String hostname = "fenghou-";
    hostname += kDeviceId;
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

bool scanTargetWiFi(int32_t& targetChannel, uint8_t targetBssid[6]) {
    Serial.println("Scanning WiFi networks...");
    int networkCount = WiFi.scanNetworks();
    bool targetFound = false;

    if (networkCount <= 0) {
        Serial.println("No WiFi networks found");
        return false;
    }

    for (int i = 0; i < networkCount; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid == WIFI_SSID) {
            targetFound = true;
            targetChannel = WiFi.channel(i);
            memcpy(targetBssid, WiFi.BSSID(i), 6);
        }

        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(ssid);
        Serial.print(" RSSI=");
        Serial.print(WiFi.RSSI(i));
        Serial.print(" channel=");
        Serial.print(WiFi.channel(i));
        Serial.print(" encryption=");
        Serial.println(WiFi.encryptionType(i));
    }

    Serial.print("Target SSID ");
    Serial.print(WIFI_SSID);
    Serial.println(targetFound ? " found" : " not found");
    return targetFound;
}

bool connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    unsigned long startedAt = millis();
    Serial.println("Starting WiFi connection");

    unsigned long modeStart = millis();
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    Serial.print("WiFi mode setup: ");
    Serial.print(millis() - modeStart);
    Serial.println(" ms");

    unsigned long hostnameStart = millis();
    WiFi.setHostname(wifiHostname.c_str());
    Serial.print("WiFi hostname setup: ");
    Serial.print(millis() - hostnameStart);
    Serial.println(" ms");

    unsigned long beginStart = millis();
    Serial.println("Connecting to WiFi");
    WiFi.persistent(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("WiFi begin call: ");
    Serial.print(millis() - beginStart);
    Serial.println(" ms");

    unsigned long waitStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < kWifiConnectTimeoutMs) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("WiFi wait connected step: ");
    Serial.print(millis() - waitStart);
    Serial.println(" ms");

    unsigned long elapsed = millis() - startedAt;
    if (WiFi.status() != WL_CONNECTED) {
        Serial.print("WiFi connect timeout after ");
        Serial.print(elapsed);
        Serial.print(" ms, status=");
        Serial.println(WiFi.status());
        int32_t targetChannel = 0;
        uint8_t targetBssid[6] = {0};
        unsigned long scanStart = millis();
        scanTargetWiFi(targetChannel, targetBssid);
        Serial.print("WiFi scan step after failure: ");
        Serial.print(millis() - scanStart);
        Serial.println(" ms");
        WiFi.scanDelete();
        WiFi.disconnect();
        return false;
    }

    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("WiFi hostname: ");
    Serial.println(wifiHostname);
    Serial.print("WiFi connect time: ");
    Serial.print(elapsed);
    Serial.println(" ms");

    return true;
}

String isoTimeNow() {
    // Mock time until NTP/device clock is wired in.
    return "2026-05-30T15:30:00+08:00";
}

String buildMockPayload() {
    float temperature = 24.5 + static_cast<float>(millis() % 1000) / 100.0;
    float humidity = 58.0 + static_cast<float>(millis() % 500) / 100.0;
    int eco2 = 400 + static_cast<int>((millis() / 1000) % 80);
    int tvoc = 8 + static_cast<int>((millis() / 1000) % 10);

    String payload = "{";
    payload += "\"device_id\":\"";
    payload += kDeviceId;
    payload += "\",\"sensor_data\":{";
    payload += "\"temperature\":";
    payload += String(temperature, 2);
    payload += ",\"humidity\":";
    payload += String(humidity, 2);
    payload += ",\"sgp30\":{\"eco2\":";
    payload += String(eco2);
    payload += ",\"tvoc\":";
    payload += String(tvoc);
    payload += "}},\"time\":\"";
    payload += isoTimeNow();
    payload += "\"}";

    return payload;
}

void uploadMockData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting");
        if (!connectWiFi()) {
            return;
        }
    }

    HTTPClient http;
    String payload = buildMockPayload();
    Serial.print("Payload build complete, bytes=");
    Serial.println(payload.length());

    // Development shortcut: skip certificate validation until a pinned CA is added.
    unsigned long tlsStart = millis();
    secureClient.setInsecure();
    Serial.print("TLS client setup: ");
    Serial.print(millis() - tlsStart);
    Serial.println(" ms");

    unsigned long beginStart = millis();
    if (!http.begin(secureClient, kUploadUrl)) {
        Serial.println("HTTP begin failed");
        return;
    }
    Serial.print("HTTP begin: ");
    Serial.print(millis() - beginStart);
    Serial.println(" ms");

    http.addHeader("Content-Type", "application/json");
    unsigned long postStart = millis();
    int statusCode = http.POST(payload);
    unsigned long postElapsed = millis() - postStart;

    Serial.print("POST ");
    Serial.print(kUploadUrl);
    Serial.print(" -> ");
    Serial.println(statusCode);
    Serial.print("HTTP POST elapsed: ");
    Serial.print(postElapsed);
    Serial.println(" ms");

    String response = http.getString();
    if (response.length() > 0) {
        Serial.println(response);
    }

    http.end();
}
}

void setup() {
    Serial.begin(115200);
    unsigned long serialWaitStart = millis();
    while (!Serial && millis() - serialWaitStart < 3000) {
        delay(10);
    }

    Serial.println();
    Serial.print("Serial ready, boot diagnostics start in ");
    Serial.print(kBootLogDelayMs / 1000);
    Serial.println(" seconds");
    delay(kBootLogDelayMs);

    Serial.println("Fenghou device uploader booting");
    wifiHostname = normalizedHostnameFromDeviceId();
    WiFi.onEvent(onWiFiEvent);
    Serial.print("Device ID: ");
    Serial.println(kDeviceId);
    Serial.print("WiFi hostname: ");
    Serial.println(wifiHostname);
    Serial.print("WiFi SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("WiFi password length: ");
    Serial.println(strlen(WIFI_PASSWORD));

    connectWiFi();
    uploadMockData();
    lastUploadMs = millis();
}

void loop() {
    unsigned long now = millis();
    if (now - lastUploadMs >= kUploadIntervalMs) {
        uploadMockData();
        lastUploadMs = now;
    }
}
