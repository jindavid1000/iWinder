#ifndef SENSORS_H
#define SENSORS_H
//============================================================================
//  sensors.h — 霍尔传感器（料盘）+ 双 Endstop 限位（左/右）
//============================================================================
#include <Arduino.h>

#include "state.h"

// ISR 回调类型，isRight 区分左/右限位
typedef void (*EndstopCallback)(bool isRight);

class Sensors {
public:
    void begin(uint8_t pinHallSpool,
               uint8_t pinEndstop, uint8_t pinEndstopRight,
               uint32_t hallDebounceUs, uint32_t endstopDebounceUs);
    void reattachPins(uint8_t pinHallSpool,
                      uint8_t pinEndstop, uint8_t pinEndstopRight);

    // 料盘脉冲获取（原子读取，返回后清零可选）
    uint32_t getSpoolPulses();
    uint32_t getSpoolPulsesAndReset();

    // Endstop — right=false 查左限位，right=true 查右限位
    bool     isEndstopTriggered(bool right = false);

    // 直接读取引脚电平（不依赖中断，用于安全保护）
    bool     isLeftEndstopPressed()  const { return digitalRead(_pinEndstop) == LOW; }
    bool     isRightEndstopPressed() const { return digitalRead(_pinEndstopRight) == LOW; }
    void     onEndstop(EndstopCallback cb) { _endstopCb = cb; }

    // 累计脉冲（从启动开始）
    uint32_t getTotalSpoolPulses() const { return _totalSpool; }

    // 最近两次脉冲的间隔 (us)，用于低速下的转速估算
    uint32_t getSpoolIntervalUs() const { return _spoolIntervalUs; }

    // ISR 入口
    static void IRAM_ATTR isrHallSpool();
    static void IRAM_ATTR isrEndstop();
    static void IRAM_ATTR isrEndstopRight();

    // 去抖动参数更新
    void setDebounce(uint32_t hallUs, uint32_t endstopUs);

private:
    static uint8_t    _pinHallSpool;
    static uint8_t    _pinEndstop;
    static uint8_t    _pinEndstopRight;
    static volatile uint32_t _spoolCount;
    static volatile uint32_t _totalSpool;
    static volatile uint32_t _lastSpoolUs;
    static volatile uint32_t _spoolIntervalUs;   // 最近一次脉冲间隔
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
