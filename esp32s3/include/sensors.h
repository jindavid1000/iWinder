#ifndef SENSORS_H
#define SENSORS_H
//============================================================================
//  sensors.h — 霍尔传感器 + 双 Endstop 限位（左/右）
//============================================================================
#include <Arduino.h>

#include "state.h"

// ISR 回调类型，isRight 区分左/右限位
typedef void (*EndstopCallback)(bool isRight);

class Sensors {
public:
    void begin(uint8_t pinHallIdler, uint8_t pinHallSpool,
               uint8_t pinEndstop, uint8_t pinEndstopRight,
               uint32_t hallDebounceUs, uint32_t endstopDebounceUs);
    void reattachPins(uint8_t pinHallIdler, uint8_t pinHallSpool,
                      uint8_t pinEndstop, uint8_t pinEndstopRight);

    // 脉冲获取（原子读取，返回后清零可选）
    uint32_t getIdlerPulses();
    uint32_t getSpoolPulses();
    uint32_t getIdlerPulsesAndReset();
    uint32_t getSpoolPulsesAndReset();

    // Endstop — right=false 查左限位，right=true 查右限位
    bool     isEndstopTriggered(bool right = false);
    void     onEndstop(EndstopCallback cb) { _endstopCb = cb; }

    // 累计脉冲（从启动开始）
    uint32_t getTotalIdlerPulses() const { return _totalIdler; }
    uint32_t getTotalSpoolPulses() const { return _totalSpool; }

    // ISR 入口
    static void IRAM_ATTR isrHallIdler();
    static void IRAM_ATTR isrHallSpool();
    static void IRAM_ATTR isrEndstop();
    static void IRAM_ATTR isrEndstopRight();

    // 去抖动参数更新
    void setDebounce(uint32_t hallUs, uint32_t endstopUs);

private:
    static uint8_t    _pinHallIdler;
    static uint8_t    _pinHallSpool;
    static uint8_t    _pinEndstop;
    static uint8_t    _pinEndstopRight;
    static volatile uint32_t _idlerCount;
    static volatile uint32_t _spoolCount;
    static volatile uint32_t _totalIdler;
    static volatile uint32_t _totalSpool;
    static volatile uint32_t _lastIdlerUs;
    static volatile uint32_t _lastSpoolUs;
    static volatile uint32_t _lastEndstopUs;
    static volatile uint32_t _lastEndstopRightUs;
    static volatile bool     _endstopState;
    static volatile bool     _endstopRightState;
    static volatile uint32_t _hallDebounceUs;
    static volatile uint32_t _endstopDebounceUs;
    static EndstopCallback   _endstopCb;
};

extern Sensors g_sensors;

#endif // SENSORS_H
