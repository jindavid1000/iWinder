#include "test_common.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    testLed.begin();
    testLed.setBrightness(40);
    ledOff();
    Serial.println("\n======== Endstop 测试（左+右）========");

    pinMode(PIN_ENDSTOP, INPUT_PULLUP);
    pinMode(PIN_ENDSTOP_RIGHT, INPUT_PULLUP);

    log("触碰左/右开关，LED 变绿，15 秒...");
    log("左=绿色 / 右=蓝色");

    unsigned long start = millis();
    bool leftTrig = false, rightTrig = false;
    bool lastL = false, lastR = false;

    while (millis() - start < 15000) {
        bool left = (digitalRead(PIN_ENDSTOP) == LOW);
        bool right = (digitalRead(PIN_ENDSTOP_RIGHT) == LOW);

        if (left != lastL) {
            Serial.printf("  [左] %s\n", left ? "ON" : "off");
            lastL = left;
            if (left) leftTrig = true;
        }
        if (right != lastR) {
            Serial.printf("  [右] %s\n", right ? "ON" : "off");
            lastR = right;
            if (right) rightTrig = true;
        }

        // 左=绿 右=蓝 都按=白
        uint8_t r = left ? 255 : 0;
        uint8_t b = right ? 255 : 0;
        ledShow(r, (left || right) ? 100 : 0, b);
        delay(10);
    }
    ledOff();

    Serial.printf("[结果] 左: %s  右: %s\n",
                  leftTrig ? "OK" : "未触发",
                  rightTrig ? "OK" : "未触发");
    log("测试完成");
}

void loop() {
    ledShow(0, 255, 0); delay(500);
    ledOff(); delay(500);
}
