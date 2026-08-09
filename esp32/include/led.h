#ifndef LED_H
#define LED_H
//============================================================================
//  led.h — 板载单色 LED 状态指示（GPIO2）
//  用亮/灭/闪烁频率表示不同设备状态，无需外部 LED 驱动库。
//============================================================================
#include <Arduino.h>
#include "state.h"

class StatusLED {
public:
    void begin(uint8_t pin = 2);
    void update();  // 在主循环中调用，根据状态自动更新
    void setState(DeviceState state, bool bleConn, bool wifiConn);

private:
    uint8_t   _pin       = 2;
    uint32_t  _lastMs    = 0;
    bool      _on        = false;
    DeviceState _state   = STATE_IDLE;
    bool      _bleConn   = false;
    bool      _wifiConn  = false;

    void set(bool on);
    void blink(uint32_t periodMs);
};

extern StatusLED g_led;

#endif // LED_H
