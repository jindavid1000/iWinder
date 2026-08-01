#ifndef SENSORS_H
#define SENSORS_H
//============================================================================
//  sensors.h — 霍尔传感器 + Endstop 限位
//============================================================================
#include <Arduino.h>

#include "state.h"

// ISR 回调类型
typedef void (*EndstopCallback)();

class Sensors {
public:
    void begin(uint8_t pinHallIdler, uint8_t pinHallSpool, uint8_t pinEndstop,
               uint32_t hallDebounceUs, uint32_t endstopDebounceUs);
    void reattachPins(uint8_t pinHallIdler, uint8_t pinHallSpool, uint8_t pinEndstop);

    // 脉冲获取（原子读取，返回后清零可选）
    uint32_t getIdlerPulses();
    uint32_t getSpoolPulses();
    uint32_t getIdlerPulsesAndReset();
    uint32_t getSpoolPulsesAndReset();

    // Endstop
    bool     isEndstopTriggered();
    void     onEndstop(EndstopCallback cb) { _endstopCb = cb; }

    // 累计脉冲（从启动开始）
    uint32_t getTotalIdlerPulses() const { return _totalIdler; }
    uint32_t getTotalSpoolPulses() const { return _totalSpool; }

    // ISR 入口（友元）
    static void IRAM_ATTR isrHallIdler();
    static void IRAM_ATTR isrHallSpool();
    static void IRAM_ATTR isrEndstop();

    // 去抖动参数更新
    void setDebounce(uint32_t hallUs, uint32_t endstopUs);

private:
    static uint8_t    _pinHallIdler;
    static uint8_t    _pinHallSpool;
    static uint8_t    _pinEndstop;
    static volatile uint32_t _idlerCount;
    static volatile uint32_t _spoolCount;
    static volatile uint32_t _totalIdler;
    static volatile uint32_t _totalSpool;
    static volatile uint32_t _lastIdlerUs;
    static volatile uint32_t _lastSpoolUs;
    static volatile uint32_t _lastEndstopUs;
    static volatile bool     _endstopState;
    static volatile uint32_t _hallDebounceUs;
    static volatile uint32_t _endstopDebounceUs;
    static EndstopCallback   _endstopCb;
};

extern Sensors g_sensors;

#endif // SENSORS_H
