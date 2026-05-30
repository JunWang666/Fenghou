#include <WiFi.h>
#include <HTTPClient.h>

// 替换为你的 Wi-Fi 凭证
const char* ssid = "LEGION-33H5IUH1";
const char* password = "34:c71M2";


String serverName = "http://192.168.31.197";

void setup() {
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi Connected!");
}

void loop() {
    // 确保 Wi-Fi 连着才发数据
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // 配置请求地址
        http.begin(serverName);

        // 发起 HTTP GET 请求
        int httpResponseCode = http.GET();

        // 如果返回码大于 0，说明成功连上了服务器
        if (httpResponseCode > 0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);

            // 打印服务器返回的内容
            String payload = http.getString();
            Serial.println(payload);
        } else {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
        }

        // 释放资源
        http.end();
    } else {
        Serial.println("WiFi Disconnected");
    }

    // 等待 10 秒再发下一次
    delay(10000);
}