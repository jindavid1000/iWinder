#include "test_common.h"

// 累计计数
static int totalSpool = 0;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n======== 霍尔传感器测试（料盘 GPIO27）========");

    pinMode(PIN_HALL_SPOOL, INPUT_PULLUP);
    attachInterrupt(PIN_HALL_SPOOL, isrSpool, FALLING);

    log("检测到脉冲即打印，按 RST 退出\n");
}

void loop() {
    if (spoolCount != 0) {
        totalSpool += spoolCount;
        Serial.printf("  [霍尔] +%d  累计=%d\n", spoolCount, totalSpool);
        spoolCount = 0;
        ledBlink(50);
    }
}
