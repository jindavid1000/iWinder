#ifndef LED_H
#define LED_H
//============================================================================
//  led.h — 板载 RGB LED 状态指示（GPIO48, WS2812）
//============================================================================
#include <Arduino.h>
#include "state.h"
#include <Adafruit_NeoPixel.h>

// LED 颜色（R, G, B 各 0-255）
struct LedColor {
    uint8_t r, g, b;
    static const LedColor OFF;
    static const LedColor RED;       // 错误/异常
    static const LedColor GREEN;     // 运行中
    static const LedColor BLUE;      // BLE 广播中
    static const LedColor CYAN;      // 已连接
    static const LedColor YELLOW;    // 暂停
    static const LedColor PURPLE;    // 寻原点/校准中
    static const LedColor ORANGE;    // WiFi 连接中
};

class StatusLED {
public:
    void begin(uint8_t pin = 48);
    void update();  // 在主循环中调用，根据状态自动更新
    void setState(DeviceState state, bool bleConn, bool wifiConn);

private:
    uint8_t   _pin       = 48;
    Adafruit_NeoPixel* _strip = nullptr;
    uint32_t  _lastMs    = 0;
    bool      _on        = false;
    DeviceState _state   = STATE_IDLE;
    bool      _bleConn   = false;
    bool      _wifiConn  = false;

    void show(uint8_t r, uint8_t g, uint8_t b);
    void blink(const LedColor &c, uint32_t periodMs);
};

extern StatusLED g_led;

#endif // LED_H
