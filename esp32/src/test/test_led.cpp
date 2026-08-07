#include "test_common.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n======== LED 测试 ========");
    log("LED 亮 1 秒");
    ledOn();
    delay(1000);
    log("LED 灭");
    ledOff();
    log("闪烁 5 次");
    for (int i = 0; i < 5; i++) { ledBlink(200); delay(300); }
    log("测试完成");
}

void loop() {
    ledOn(); delay(500);
    ledOff(); delay(500);
}
