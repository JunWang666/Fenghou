#include <Arduino.h>
#include <Wire.h>

// 强制指定你项目里用的引脚
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

void setup() {
  Serial.begin(115200);

  // 给原生 USB 串口留出 3 秒钟的连接时间
  delay(3000);
  Serial.println("\n--- I2C 硬件诊断测试开始 ---");
  Serial.printf("使用的引脚: SDA = %d, SCL = %d\n", PIN_I2C_SDA, PIN_I2C_SCL);

  // 初始化 I2C
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("正在扫描 I2C 总线...");

  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("成功：在地址 0x%02X 发现 I2C 设备！\n", address);
      nDevices++;
    } else if (error == 4) {
      Serial.printf("警告：在地址 0x%02X 发生未知错误 (Error 4)\n", address);
    }
    // error 为 2 正常表示 NACK（该地址没设备）
  }

  if (nDevices == 0) {
    Serial.println("失败：没有找到任何 I2C 设备。请检查：1.引脚是否接反 2.排线是否断裂 3.模块是否损坏 4.是否需要外部上拉电阻");
  } else {
    Serial.println("扫描完成。\n");
  }

  delay(3000); // 每 3 秒扫描一次
}