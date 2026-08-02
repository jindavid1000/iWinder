#include "led.h"

StatusLED g_led;

const LedColor LedColor::OFF    = {0, 0, 0};
const LedColor LedColor::RED    = {255, 0, 0};
const LedColor LedColor::GREEN  = {0, 255, 0};
const LedColor LedColor::BLUE   = {0, 0, 255};
const LedColor LedColor::CYAN   = {0, 255, 255};
const LedColor LedColor::YELLOW = {255, 180, 0};
const LedColor LedColor::PURPLE = {180, 0, 255};
const LedColor LedColor::ORANGE = {255, 100, 0};

#include <Adafruit_NeoPixel.h>

void StatusLED::begin(uint8_t pin) {
    _pin = pin;
    _strip = new Adafruit_NeoPixel(1, _pin, NEO_GRB + NEO_KHZ800);
    _strip->begin();
    _strip->setBrightness(30);  // 亮度限制，避免太刺眼
    show(0, 0, 0);
}

void StatusLED::show(uint8_t r, uint8_t g, uint8_t b) {
    if (!_strip) return;
    _strip->setPixelColor(0, _strip->Color(r, g, b));
    _strip->show();
}

void StatusLED::blink(const LedColor &c, uint32_t periodMs) {
    uint32_t now = millis();
    if (now - _lastMs >= periodMs) {
        _lastMs = now;
        _on = !_on;
        if (_on) {
            show(c.r, c.g, c.b);
        } else {
            show(0, 0, 0);
        }
    }
}

void StatusLED::setState(DeviceState state, bool bleConn, bool wifiConn) {
    _state = state;
    _bleConn = bleConn;
    _wifiConn = wifiConn;
}

void StatusLED::update() {
    // 已连接 -> 固定青色
    if (_bleConn || _wifiConn) {
        if (_state == STATE_RUNNING) {
            // 连接 + 运行中 -> 绿色
            show(LedColor::GREEN.r, LedColor::GREEN.g, LedColor::GREEN.b);
        } else if (_state == STATE_ERROR) {
            // 连接 + 错误 -> 红色闪烁
            blink(LedColor::RED, 200);
        } else {
            // 连接 + 其他 -> 青色
            show(LedColor::CYAN.r, LedColor::CYAN.g, LedColor::CYAN.b);
        }
        return;
    }

    // 未连接 -> 按状态指示
    switch (_state) {
        case STATE_IDLE:
        case STATE_COMPLETED:
            // 待机 -> 蓝色慢闪（BLE 广播中）
            blink(LedColor::BLUE, 1000);
            break;
        case STATE_HOMING:
        case STATE_POSITIONING:
        case STATE_CALIBRATING:
            // 寻原点/校准 -> 紫色闪烁
            blink(LedColor::PURPLE, 300);
            break;
        case STATE_RUNNING:
            // 未连接但运行中 -> 绿色闪烁
            blink(LedColor::GREEN, 500);
            break;
        case STATE_PAUSED:
            // 暂停 -> 黄色闪烁
            blink(LedColor::YELLOW, 800);
            break;
        case STATE_ERROR:
            // 错误 -> 红色快闪
            blink(LedColor::RED, 200);
            break;
        default:
            blink(LedColor::BLUE, 1000);
            break;
    }
}
