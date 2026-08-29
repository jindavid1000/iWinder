#ifndef TEST_COMMON_H
#define TEST_COMMON_H
#include <Arduino.h>

#define PIN_LED          2
#define PIN_MOTOR_PWM    4
#define PIN_SERVO_PWM    5
#define PIN_ENDSTOP      14
#define PIN_ENDSTOP_RIGHT 32
#define PIN_HALL_SPOOL   27

inline void ledOn()  { pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, HIGH); }
inline void ledOff() { pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, LOW); }
inline void ledBlink(int ms = 200) { ledOn(); delay(ms); ledOff(); }

inline void log(const char* m) { Serial.print("[TEST] "); Serial.println(m); }

inline void waitEnter(const char* msg) {
    Serial.println(msg);
    Serial.println(">>> 按回车继续 <<<");
    while (true) {
        if (Serial.available() && Serial.read() == '\n') break;
        delay(10);
    }
}

inline void setupLEDC(uint8_t pin, uint8_t ch, uint32_t freq, uint8_t res) {
    ledcSetup(ch, freq, res);
    ledcAttachPin(pin, ch);
}

// 把本测试不用的输出引脚钉死电平，防止悬空：
//  - 电机 SIG 悬空 → MOS 栅极漂移，电机微动
//  - 舵机 SIG 悬空 → 360°舵机被噪声触发随机转动
inline void parkUnusedOutputs(uint8_t usedPin) {
    if (usedPin != PIN_MOTOR_PWM) { pinMode(PIN_MOTOR_PWM, OUTPUT); digitalWrite(PIN_MOTOR_PWM, LOW); }
    if (usedPin != PIN_SERVO_PWM) { pinMode(PIN_SERVO_PWM, OUTPUT); digitalWrite(PIN_SERVO_PWM, LOW); }
}

// 霍尔 ISR 计数
volatile int spoolCount = 0;
volatile unsigned long lastSpoolUs = 0;
void IRAM_ATTR isrSpool() { unsigned long n = micros(); if (n - lastSpoolUs < 5000) return; lastSpoolUs = n; spoolCount++; }

#endif
