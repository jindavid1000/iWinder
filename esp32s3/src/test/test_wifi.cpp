#include "test_common.h"
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(500);
    testLed.begin();
    testLed.setBrightness(40);
    ledOff();
    Serial.println("\n======== WiFi AP 射频测试 ========");

    log("手机 WiFi 设置中搜索 'ESP-Winder-Test'");
    log("能搜到 = 射频正常; 搜不到 = 射频问题");

    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP("ESP-Winder-Test");
    if (ok) {
        Serial.printf("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        log("15 秒内用手机查看...");
        ledShow(0, 80, 80);
        delay(15000);
        WiFi.softAPdisconnect(true);
    } else {
        log("[FAIL] WiFi AP 启动失败");
        ledShow(255, 0, 0); delay(3000);
    }
    ledOff();
    WiFi.mode(WIFI_OFF);
    log("测试完成");
}

void loop() {
    ledShow(0, 255, 0); delay(500);
    ledOff(); delay(500);
}
