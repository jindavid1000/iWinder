#include "test_common.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    testLed.begin();
    testLed.setBrightness(40);
    ledOff();
    Serial.println("\n======== 舵机最高速测试 ========");

    uint8_t ch = 1;
    setupLEDC(PIN_SERVO_PWM, ch, 50, 14);
    auto wp = [&](uint16_t us) {
        ledcWrite(ch, (uint32_t)us * ((1 << 14) - 1) / 20000);
    };

    log("满速右行 2500us, 60 秒");
    wp(2500);
    ledShow(0, 50, 0);

    unsigned long start = millis();
    while (millis() - start < 60000) {
        Serial.printf("  已运行 %lus / 60s\n", (millis() - start) / 1000);
        delay(5000);
    }

    log("停止");
    wp(1500);
    ledOff();
    log("测试完成");
}

void loop() {
    ledShow(0, 255, 0); delay(500);
    ledOff(); delay(500);
}
