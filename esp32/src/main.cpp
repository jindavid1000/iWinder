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
    Serial.println("  耗材绕线器 ESP32 固件启动");
    Serial.println("========================================\n");

    // 1. 配置（纯 NVS，不碰外设）
    Serial.println("[Main] 1/5 加载配置");
    g_storage.begin();
    g_storage.loadConfig(g_config);
    Serial.printf("[Main] 线径=%.2fmm 料盘宽=%.1fmm\n",
                  g_config.filamentDiameter, g_config.spoolWidth);

    // 2. 通信（最先初始化，此时系统干净，无外设干扰）
    Serial.println("[Main] 2/5 初始化 WiFi");
    g_comms.onMessage([](const String &msg) {
        g_protocol.handle(msg);
    });
    g_comms.begin();

    // 3. 硬件（即使 LEDC 出问题也不影响已建立的 WiFi）
    Serial.println("[Main] 3/5 初始化硬件");
    g_winder.begin();
    Serial.println("[Main] 硬件就绪");

    // 4. 状态灯
    Serial.println("[Main] 4/5 初始化状态灯");
    g_led.begin();

    // 5. 寻原点
    Serial.println("[Main] 5/5 寻原点");
    g_winder.goHome();

    Serial.println("[Main] 初始化完成\n");
}

void loop() {
    g_winder.update();
    g_comms.update();
    g_led.setState(g_state.state, false, g_state.wifiConnected);
    g_led.update();
}
