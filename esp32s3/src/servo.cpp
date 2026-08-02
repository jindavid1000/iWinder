#include "servo.h"

ServoCtl g_servo;

void ServoCtl::begin(uint8_t pin, uint16_t freq, uint8_t resBits) {
    if (_initialized) return;
    _pin     = pin;
    _freq    = freq;
    _resBits = resBits;
    _maxDuty = (1 << resBits) - 1;

    uint32_t actualFreq = ledcSetup(_channel, _freq, _resBits);
    if (actualFreq == 0) {
        Serial.printf("[Servo] LEDC 初始化失败! channel=%d freq=%d bits=%d\n",
                      _channel, _freq, _resBits);
        return;  // 不 attach，避免绑定无效通道
    }
    Serial.printf("[Servo] LEDC OK: ch=%d freq=%d bits=%d actual=%d\n",
                  _channel, _freq, _resBits, actualFreq);

    ledcAttachPin(_pin, _channel);
    writePulse(_pulseStop);
    _direction = DIR_NONE;
    _position  = 0;
    _initialized = true;
}

void ServoCtl::reattach(uint8_t pin) {
    if (_pin != 0) ledcDetachPin(_pin);
    _pin = pin;
    ledcAttachPin(_pin, _channel);
    writePulse(_pulseStop);
}

void ServoCtl::writePulse(uint16_t pulseUs) {
    if (!_initialized) return;
    // 20ms 周期（50Hz），整数运算避免浮点精度问题
    uint32_t duty = (uint32_t)pulseUs * _maxDuty / 20000;
    ledcWrite(_channel, duty);
}

void ServoCtl::moveLeft()  { writePulse(_pulseLeft);  _direction = DIR_LEFT; }
void ServoCtl::moveRight() { writePulse(_pulseRight); _direction = DIR_RIGHT; }
void ServoCtl::moveHome()  { writePulse(_pulseHome);  _direction = DIR_LEFT; }
void ServoCtl::stop()      { writePulse(_pulseStop);  _direction = DIR_NONE; }

void ServoCtl::updatePosition(uint32_t dtMs) {
    if (_direction == DIR_NONE) return;
    float speed = (_direction == DIR_LEFT) ? _speedLeft : _speedRight;
    if (speed <= 0) return;
    float delta = speed * (dtMs / 1000.0f);
    if (_direction == DIR_LEFT) {
        _position -= delta;
        if (_position < 0) _position = 0;
    } else {
        _position += delta;
    }
}

void ServoCtl::setPulses(uint16_t stop, uint16_t left, uint16_t right, uint16_t home) {
    _pulseStop = stop; _pulseLeft = left; _pulseRight = right; _pulseHome = home;
}
void ServoCtl::setSpeeds(float leftMmS, float rightMmS) {
    _speedLeft = leftMmS; _speedRight = rightMmS;
}
