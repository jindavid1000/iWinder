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

void ServoCtl::writeCurrentDirection() {
    // 根据方向 + 速度比例计算实际 PWM 脉宽
    uint16_t pulse;
    if (_direction == DIR_LEFT) {
        // 左行：从 stop(1500) 向 left(500) 偏移，按比例
        pulse = _pulseStop - (_pulseStop - _pulseLeft) * _speedFraction;
    } else if (_direction == DIR_RIGHT) {
        // 右行：从 stop(1500) 向 right(2500) 偏移，按比例
        pulse = _pulseStop + (_pulseRight - _pulseStop) * _speedFraction;
    } else {
        pulse = _pulseStop;
    }
    writePulse(pulse);
}

void ServoCtl::setSpeedFraction(float frac) {
    if (frac < 0.01f) frac = 0.01f;
    if (frac > 1.0f) frac = 1.0f;
    _speedFraction = frac;
    if (_direction != DIR_NONE) writeCurrentDirection();
}

void ServoCtl::moveLeft()  { _direction = DIR_LEFT;  writeCurrentDirection(); }
void ServoCtl::moveRight() { _direction = DIR_RIGHT; writeCurrentDirection(); }
void ServoCtl::moveHome()  { _speedFraction = 1.0f; _direction = DIR_LEFT;  writeCurrentDirection(); }
void ServoCtl::stop()      { _direction = DIR_NONE; _speedFraction = 1.0f; writePulse(_pulseStop); }

void ServoCtl::drive(int16_t pct) {
    if (pct < -100) pct = -100;
    if (pct >  100) pct =  100;
    if (pct == 0) {
        _direction = DIR_NONE;
        writePulse(_pulseStop);
        return;
    }
    _direction = (pct > 0) ? DIR_RIGHT : DIR_LEFT;
    _speedFraction = fabsf(pct) / 100.0f;
    writeCurrentDirection();
}

void ServoCtl::updatePosition(uint32_t dtMs) {
    if (_direction == DIR_NONE) return;
    float speed = (_direction == DIR_LEFT) ? _speedLeft : _speedRight;
    if (speed <= 0) return;
    float delta = speed * _speedFraction * (dtMs / 1000.0f);
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
