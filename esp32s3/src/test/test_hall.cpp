#include "test_common.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    testLed.begin();
    testLed.setBrightness(40);
    ledOff();
    Serial.println("\n======== 霍尔传感器测试 ========");

    pinMode(PIN_HALL_IDLER, INPUT_PULLUP);
    pinMode(PIN_HALL_SPOOL, INPUT_PULLUP);
    attachInterrupt(PIN_HALL_IDLER, isrIdler, FALLING);
    attachInterrupt(PIN_HALL_SPOOL, isrSpool, FALLING);

    idlerCount = 0;
    spoolCount = 0;
    log("转动磁铁，15 秒...");

    unsigned long start = millis();
    int li = 0, ls = 0;
    while (millis() - start < 15000) {
        if (idlerCount != li) {
            Serial.printf("  [霍尔A] %d\n", idlerCount);
            li = idlerCount; ledShow(0, 100, 0); delay(50); ledOff();
        }
        if (spoolCount != ls) {
            Serial.printf("  [霍尔B] %d\n", spoolCount);
            ls = spoolCount; ledShow(0, 0, 100); delay(50); ledOff();
        }
        delay(10);
    }
    detachInterrupt(PIN_HALL_IDLER);
    detachInterrupt(PIN_HALL_SPOOL);
    Serial.printf("[结果] 从动轮: %d, 料盘: %d\n", idlerCount, spoolCount);
    log("测试完成");
}

void loop() {
    ledShow(0, 255, 0); delay(500);
    ledOff(); delay(500);
}
