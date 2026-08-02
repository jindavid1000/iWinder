#ifndef TEST_COMMON_H
#define TEST_COMMON_H
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define PIN_LED          2
#define PIN_MOTOR_PWM    4
#define PIN_SERVO_PWM    5
#define PIN_ENDSTOP      14
#define PIN_ENDSTOP_RIGHT 32
#define PIN_HALL_IDLER   13
#define PIN_HALL_SPOOL   27

static Adafruit_NeoPixel testLed(1, PIN_LED, NEO_GRB + NEO_KHZ800);

inline void ledShow(uint8_t r, uint8_t g, uint8_t b) {
    testLed.setPixelColor(0, testLed.Color(r, g, b));
    testLed.show();
}
inline void ledOff() { ledShow(0, 0, 0); }

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

// 霍尔 ISR 计数
volatile int idlerCount = 0, spoolCount = 0;
volatile unsigned long lastIdlerUs = 0, lastSpoolUs = 0;
void IRAM_ATTR isrIdler() { unsigned long n = micros(); if (n - lastIdlerUs < 5000) return; lastIdlerUs = n; idlerCount++; }
void IRAM_ATTR isrSpool() { unsigned long n = micros(); if (n - lastSpoolUs < 5000) return; lastSpoolUs = n; spoolCount++; }

#endif
