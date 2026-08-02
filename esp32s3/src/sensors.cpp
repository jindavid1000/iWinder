#include "sensors.h"

Sensors g_sensors;

// 静态成员初始化
uint8_t    Sensors::_pinHallIdler   = 0;
uint8_t    Sensors::_pinHallSpool   = 0;
uint8_t    Sensors::_pinEndstop     = 0;
uint8_t    Sensors::_pinEndstopRight = 0;
volatile uint32_t Sensors::_idlerCount   = 0;
volatile uint32_t Sensors::_spoolCount   = 0;
volatile uint32_t Sensors::_totalIdler   = 0;
volatile uint32_t Sensors::_totalSpool   = 0;
volatile uint32_t Sensors::_lastIdlerUs  = 0;
volatile uint32_t Sensors::_lastSpoolUs  = 0;
volatile uint32_t Sensors::_lastEndstopUs = 0;
volatile uint32_t Sensors::_lastEndstopRightUs = 0;
volatile bool     Sensors::_endstopState  = false;
volatile bool     Sensors::_endstopRightState = false;
volatile uint32_t Sensors::_hallDebounceUs   = 5000;
volatile uint32_t Sensors::_endstopDebounceUs = 20000;
EndstopCallback   Sensors::_endstopCb = nullptr;

void IRAM_ATTR Sensors::isrHallIdler() {
    uint32_t now = micros();
    if (now - _lastIdlerUs < _hallDebounceUs) return;
    _lastIdlerUs = now;
    _idlerCount++;
    _totalIdler++;
}

void IRAM_ATTR Sensors::isrHallSpool() {
    uint32_t now = micros();
    if (now - _lastSpoolUs < _hallDebounceUs) return;
    _lastSpoolUs = now;
    _spoolCount++;
    _totalSpool++;
}

void IRAM_ATTR Sensors::isrEndstop() {
    uint32_t now = micros();
    if (now - _lastEndstopUs < _endstopDebounceUs) return;
    _lastEndstopUs = now;
    _endstopState = true;
    if (_endstopCb) _endstopCb(false);
}

void IRAM_ATTR Sensors::isrEndstopRight() {
    uint32_t now = micros();
    if (now - _lastEndstopRightUs < _endstopDebounceUs) return;
    _lastEndstopRightUs = now;
    _endstopRightState = true;
    if (_endstopCb) _endstopCb(true);
}

void Sensors::begin(uint8_t pinHallIdler, uint8_t pinHallSpool,
                     uint8_t pinEndstop, uint8_t pinEndstopRight,
                     uint32_t hallDebounceUs, uint32_t endstopDebounceUs) {
    _pinHallIdler = pinHallIdler;
    _pinHallSpool = pinHallSpool;
    _pinEndstop   = pinEndstop;
    _pinEndstopRight = pinEndstopRight;
    _hallDebounceUs    = hallDebounceUs;
    _endstopDebounceUs = endstopDebounceUs;
    _idlerCount = _spoolCount = 0;
    _totalIdler = _totalSpool = 0;
    _endstopState = false;
    _endstopRightState = false;

    pinMode(_pinHallIdler, INPUT_PULLUP);
    pinMode(_pinHallSpool, INPUT_PULLUP);
    pinMode(_pinEndstop,   INPUT_PULLUP);
    pinMode(_pinEndstopRight, INPUT_PULLUP);

    attachInterrupt(_pinHallIdler, isrHallIdler, FALLING);
    attachInterrupt(_pinHallSpool, isrHallSpool, FALLING);
    attachInterrupt(_pinEndstop,   isrEndstop,   FALLING);
    attachInterrupt(_pinEndstopRight, isrEndstopRight, FALLING);
}

void Sensors::reattachPins(uint8_t pinHallIdler, uint8_t pinHallSpool,
                            uint8_t pinEndstop, uint8_t pinEndstopRight) {
    detachInterrupt(_pinHallIdler);
    detachInterrupt(_pinHallSpool);
    detachInterrupt(_pinEndstop);
    detachInterrupt(_pinEndstopRight);
    _pinHallIdler = pinHallIdler;
    _pinHallSpool = pinHallSpool;
    _pinEndstop   = pinEndstop;
    _pinEndstopRight = pinEndstopRight;
    pinMode(_pinHallIdler, INPUT_PULLUP);
    pinMode(_pinHallSpool, INPUT_PULLUP);
    pinMode(_pinEndstop,   INPUT_PULLUP);
    pinMode(_pinEndstopRight, INPUT_PULLUP);
    attachInterrupt(_pinHallIdler, isrHallIdler, FALLING);
    attachInterrupt(_pinHallSpool, isrHallSpool, FALLING);
    attachInterrupt(_pinEndstop,   isrEndstop,   FALLING);
    attachInterrupt(_pinEndstopRight, isrEndstopRight, FALLING);
}

void Sensors::setDebounce(uint32_t hallUs, uint32_t endstopUs) {
    _hallDebounceUs = hallUs;
    _endstopDebounceUs = endstopUs;
}

uint32_t Sensors::getIdlerPulses() {
    portENTER_CRITICAL(&g_dataMux);
    uint32_t v = _idlerCount;
    portEXIT_CRITICAL(&g_dataMux);
    return v;
}

uint32_t Sensors::getSpoolPulses() {
    portENTER_CRITICAL(&g_dataMux);
    uint32_t v = _spoolCount;
    portEXIT_CRITICAL(&g_dataMux);
    return v;
}

uint32_t Sensors::getIdlerPulsesAndReset() {
    portENTER_CRITICAL(&g_dataMux);
    uint32_t v = _idlerCount;
    _idlerCount = 0;
    portEXIT_CRITICAL(&g_dataMux);
    return v;
}

uint32_t Sensors::getSpoolPulsesAndReset() {
    portENTER_CRITICAL(&g_dataMux);
    uint32_t v = _spoolCount;
    _spoolCount = 0;
    portEXIT_CRITICAL(&g_dataMux);
    return v;
}

bool Sensors::isEndstopTriggered(bool right) {
    portENTER_CRITICAL(&g_dataMux);
    bool v = right ? _endstopRightState : _endstopState;
    if (right) _endstopRightState = false;
    else       _endstopState = false;
    portEXIT_CRITICAL(&g_dataMux);
    return v;
}
