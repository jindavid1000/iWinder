#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "motor.h"
#include "servo.h"
#include "sensors.h"
#include "slip.h"
#include "storage.h"
#include "comms.h"
#include "protocol.h"
#include "winder.h"
#include "led.h"
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("  耗材绕线器 ESP32-S3 固件启动");
    Serial.println("========================================\n");
    Serial.println("[Main] 步骤1: 加载配置");

    // 1. 加载配置
    g_storage.begin();
    g_storage.loadConfig(g_config);
    Serial.printf("[Main] 配置已加载, 线径=%.2fmm, 料盘宽=%.1fmm\n",
                  g_config.filamentDiameter, g_config.spoolWidth);

    Serial.println("[Main] 步骤2: 初始化硬件");
    // 2. 初始化硬件模块
    g_winder.begin();
    g_led.begin();
    Serial.println("[Main] 硬件初始化完成");

    Serial.println("[Main] 步骤3: 初始化通信");
    // 3. 初始化通信
    g_comms.onMessage([](const String &msg) {
        g_protocol.handle(msg);
    });
    g_comms.begin();
    Serial.println("[Main] 通信初始化完成");

    Serial.println("[Main] 步骤4: 开机寻原点");
    // 5. 开机自动寻原点
    g_winder.goHome();

    Serial.println("[Main] 初始化完成\n");
}

void loop() {
    g_winder.update();
    g_comms.update();
    g_led.setState(g_state.state, false, g_state.wifiConnected);
    g_led.update();
}
