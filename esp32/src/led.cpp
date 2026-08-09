#include "led.h"

StatusLED g_led;

void StatusLED::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    set(false);
}

void StatusLED::set(bool on) {
    _on = on;
    digitalWrite(_pin, on ? HIGH : LOW);
}

void StatusLED::blink(uint32_t periodMs) {
    uint32_t now = millis();
    if (now - _lastMs >= periodMs) {
        _lastMs = now;
        set(!_on);
    }
}

void StatusLED::setState(DeviceState state, bool bleConn, bool wifiConn) {
    _state = state;
    _bleConn = bleConn;
    _wifiConn = wifiConn;
}

void StatusLED::update() {
    // 已连接 + 运行中 -> 常亮
    if (_bleConn || _wifiConn) {
        if (_state == STATE_RUNNING) {
            set(true);
        } else if (_state == STATE_ERROR) {
            // 连接 + 错误 -> 快闪
            blink(200);
        } else {
            // 连接 + 其他 -> 常亮
            set(true);
        }
        return;
    }

    // 未连接 -> 按状态指示
    switch (_state) {
        case STATE_IDLE:
        case STATE_COMPLETED:
            // 待机 -> 慢闪
            blink(1000);
            break;
        case STATE_HOMING:
        case STATE_POSITIONING:
        case STATE_CALIBRATING:
            // 寻原点/校准 -> 快闪
            blink(300);
            break;
        case STATE_RUNNING:
            // 未连接但运行中 -> 中速闪
            blink(500);
            break;
        case STATE_PAUSED:
            // 暂停 -> 慢闪
            blink(800);
            break;
        case STATE_ERROR:
            // 错误 -> 快闪
            blink(200);
            break;
        default:
            blink(1000);
            break;
    }
}
