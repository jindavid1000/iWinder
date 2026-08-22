#include "motor.h"

Motor g_motor;

void Motor::begin(uint8_t pin, uint16_t freq, uint8_t resBits) {
    if (_initialized) return;
    _pin     = pin;
    _freq    = freq;
    _resBits = resBits;
    _maxDuty = (1 << resBits) - 1;

    uint32_t actualFreq = ledcSetup(_channel, _freq, _resBits);
    if (actualFreq == 0) {
        Serial.printf("[Motor] LEDC 初始化失败! channel=%d freq=%d bits=%d\n",
                      _channel, _freq, _resBits);
        return;
    }
    Serial.printf("[Motor] LEDC OK: ch=%d freq=%d bits=%d actual=%d\n",
                  _channel, _freq, _resBits, actualFreq);

    ledcAttachPin(_pin, _channel);
    ledcWrite(_channel, 0);
    _currentSpeed = 0;
    _targetSpeed  = 0;
    _initialized  = true;
}

void Motor::reattach(uint8_t pin) {
    if (_pin != 0) ledcDetachPin(_pin);
    _pin = pin;
    ledcAttachPin(_pin, _channel);
    ledcWrite(_channel, 0);
}

void Motor::setDriver(uint8_t d) {
    if (!_initialized || d == _driver) { _driver = d; return; }
    _driver = d;
    if (d == 1) {
        // L298N 开关模式: 放弃 PWM，引脚改普通输出。
        // ENA 插跳线帽保持全速使能，本引脚接 IN1（IN2 接 GND），
        // 高=转 低=停，方向由接线决定。
        ledcDetachPin(_pin);
        ledcWrite(_channel, 0);
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        _l298On = false;
        Serial.println("[Motor] 驱动模式: L298N 开关（不调速）");
    } else {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        ledcAttachPin(_pin, _channel);
        ledcWrite(_channel, 0);
        _currentSpeed = 0;
        _targetSpeed  = 0;
        Serial.println("[Motor] 驱动模式: MOS PWM 调速");
    }
}

void Motor::setSpeedPct(float pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    _targetSpeed = pct;
}

void Motor::update() {
    if (!_initialized) return;
    if (_driver == 1) {
        // L298N: 无调速，目标 >0 即通电，=0 即断电。保留软启动斜坡仅作
        // 状态显示（缠料检测读 getCurrentSpeedPct）
        bool on = (_targetSpeed > 0.5f);
        if (on != _l298On) {
            _l298On = on;
            digitalWrite(_pin, on ? HIGH : LOW);
        }
        _currentSpeed = on ? 100.0f : 0.0f;
        return;
    }
    if (_currentSpeed == _targetSpeed) {
        _lastUpdateMs = millis();
        return;
    }
    uint32_t now   = millis();
    uint32_t dt    = now - _lastUpdateMs;
    _lastUpdateMs  = now;
    if (dt == 0) return;
    float step = (100.0f * dt) / _softStartMs;
    if (_targetSpeed > _currentSpeed) {
        _currentSpeed += step;
        if (_currentSpeed > _targetSpeed) _currentSpeed = _targetSpeed;
    } else {
        _currentSpeed -= step * 2;
        if (_currentSpeed < _targetSpeed) _currentSpeed = _targetSpeed;
    }
    uint32_t duty = (uint32_t)(_currentSpeed / 100.0f * _maxDuty);
    ledcWrite(_channel, duty);
}

void Motor::stop() {
    _targetSpeed  = 0;
    _currentSpeed = 0;
    if (!_initialized) return;
    if (_driver == 1) {
        _l298On = false;
        digitalWrite(_pin, LOW);
    } else {
        ledcWrite(_channel, 0);
    }
}
