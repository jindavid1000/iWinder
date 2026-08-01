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

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("\n========================================");
    Serial.println("  耗材绕线器 ESP32-S3 固件启动");
    Serial.println("========================================\n");

    // 1. 加载配置
    g_storage.begin();
    g_storage.loadConfig(g_config);
    Serial.printf("[Main] 配置已加载, 线径=%.2fmm, 料盘宽=%.1fmm\n",
                  g_config.filamentDiameter, g_config.spoolWidth);

    // 2. 初始化硬件模块
    g_winder.begin();

    // 3. 初始化通信
    g_comms.onMessage([](const String &msg) {
        g_protocol.handle(msg);
    });
    g_comms.begin();

    // 4. 尝试 WiFi 重连
    g_comms.wifiTryReconnect();

    // 5. 开机自动寻原点
    g_winder.goHome();

    Serial.println("[Main] 初始化完成\n");
}

void loop() {
    g_winder.update();
    g_comms.update();
}
