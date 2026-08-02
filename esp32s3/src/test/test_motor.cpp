#include "test_common.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    testLed.begin();
    testLed.setBrightness(40);
    ledOff();
    Serial.println("\n======== 电机测试 ========");

    uint8_t ch = 0;
    setupLEDC(PIN_MOTOR_PWM, ch, 1000, 10);

    log("软启动渐快...");
    ledShow(50, 50, 0);
    for (int d = 0; d <= 512; d += 10) { ledcWrite(ch, d); delay(20); }

    log("50% 运行 3秒");
    delay(3000);

    log("渐慢停止");
    for (int d = 512; d >= 0; d -= 10) { ledcWrite(ch, d); delay(20); }
    ledcWrite(ch, 0);
    ledOff();
    log("测试完成");
}

void loop() {
    ledShow(0, 255, 0); delay(500);
    ledOff(); delay(500);
}
