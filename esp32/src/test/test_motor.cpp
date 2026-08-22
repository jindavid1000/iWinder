#include "test_common.h"

// ============================================================================
//  电机测试（MOS 管与 L298N 接法通用）
//  每 8 秒一个循环: 通电 3 秒 → 断电 5 秒，LED 同步指示。
//  - MOS 接法: GPIO4 直接驱动栅极，通电 = 电机全速
//  - L298N 接法: ENA 插跳线，IN1=GPIO4，通电 = 电机全速（方向由接线决定）
//  验证点: 通电时电机转、断电时立即停、无异常发热/异响
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n======== 电机测试（通电 3s / 断电 5s 循环）========");

    pinMode(PIN_MOTOR_PWM, OUTPUT);
    digitalWrite(PIN_MOTOR_PWM, LOW);
    parkUnusedOutputs(PIN_MOTOR_PWM);
}

void loop() {
    log("通电");
    ledOn();
    digitalWrite(PIN_MOTOR_PWM, HIGH);
    delay(3000);

    log("断电");
    ledOff();
    digitalWrite(PIN_MOTOR_PWM, LOW);
    delay(5000);
}
