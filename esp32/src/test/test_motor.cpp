#include "test_common.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n======== 电机测试 ========");

    uint8_t ch = 0;
    setupLEDC(PIN_MOTOR_PWM, ch, 1000, 10);
    parkUnusedOutputs(PIN_MOTOR_PWM);

    log("软启动渐快...");
    ledOn();
    for (int d = 0; d <= 1023; d += 20) { ledcWrite(ch, d); delay(20); }

    log("100% 运行 30秒");
    delay(30000);

    log("渐慢停止");
    for (int d = 1023; d >= 0; d -= 20) { ledcWrite(ch, d); delay(20); }
    ledcWrite(ch, 0);
    ledOff();
    log("测试完成");
}

void loop() {
    ledOn(); delay(500);
    ledOff(); delay(500);
}
