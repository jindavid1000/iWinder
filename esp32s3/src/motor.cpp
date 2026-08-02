#include "motor.h"

Motor g_motor;

void Motor::begin(uint8_t pin, uint16_t freq, uint8_t resBits) {
    if (_initialized) return;
    _pin     = pin;
    _freq    = freq;
    _resBits = resBits;
    _maxDuty = (1 << resBits) - 1;
    ledcSetup(_channel, _freq, _resBits);
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

void Motor::setSpeedPct(float pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    _targetSpeed = pct;
}

void Motor::update() {
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
    ledcWrite(_channel, 0);
}
