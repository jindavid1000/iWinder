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
#include "webui.h"
#include "led.h"
#include "license.h"
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("  耗材绕线器 ESP32 固件启动");
    Serial.println("========================================\n");

    // 1. 配置（纯 NVS，不碰外设）
    Serial.println("[Main] 1/5 加载配置");
    License::begin();
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

    // 5. Web 界面（WiFi 就绪后即可服务）
    Serial.println("[Main] 5/5 启动 Web 界面");
    g_webui.begin();

    License::onlineCheck();   // 联网心跳（阻塞可达数秒，必须在启动回原点之前完成，
                              // 否则 setup 卡在网络请求时小车已顶死限位而无法停车）
    g_winder.goHome();

    Serial.println("[Main] 初始化完成\n");
}

static uint32_t s_lastCheckMs = 0;
void loop() {
    // 在线心跳：联网前每 30 秒重试，连上后每小时一次
    // （开机时 WiFi 往往还没就绪，只查一次会漏掉封禁状态刷新）
    static uint32_t s_failCount = 0;
    // 常规 1 小时一次；联网失败时 30 秒重试直到成功（保证封禁状态最终同步）
    const uint32_t interval = s_failCount > 0 ? 30UL * 1000UL : 3600UL * 1000UL;
    if (millis() - s_lastCheckMs > interval) {
        s_lastCheckMs = millis();
        s_failCount = License::onlineCheck() ? 0 : s_failCount + 1;
    }
    g_winder.update();
    g_comms.update();
    g_webui.update();
    g_led.setState(g_state.state, false, g_state.wifiConnected);
    g_led.update();
}
