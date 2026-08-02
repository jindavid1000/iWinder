#include "test_common.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    testLed.begin();
    testLed.setBrightness(40);
    ledOff();
    Serial.println("\n======== LED 测试 ========");

    log("依次显示白/红/绿/蓝");
    ledShow(255, 255, 255); delay(400);
    ledShow(255, 0, 0);     delay(400);
    ledShow(0, 255, 0);     delay(400);
    ledShow(0, 0, 255);     delay(400);
    ledOff();
    log("测试完成");
}

void loop() {
    ledShow(0, 255, 0); delay(500);
    ledOff(); delay(500);
}
