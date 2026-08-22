#ifndef MOTOR_H
#define MOTOR_H
//============================================================================
//  motor.h — 收线盘直流电机驱动
//  MOS 管 PWM 调速（默认）或 L298N 开关模式（ENA 插跳线全速，GPIO 只控启停）
//============================================================================
#include <Arduino.h>

class Motor {
public:
    void begin(uint8_t pin, uint16_t freq, uint8_t resBits);
    void reattach(uint8_t pin);          // 引脚变更后重新初始化
    void setDriver(uint8_t d);           // 0=MOS PWM 1=L298N 开关（运行中可切）
    void setSpeedPct(float pct);          // 设置目标速度 0-100%（L298N: >0 即开）
    void update();                        // 软启动渐变，在主循环中调用
    void stop();                          // 立即停止
    float getCurrentSpeedPct() const { return _currentSpeed; }
    float getTargetSpeedPct() const { return _targetSpeed; }
    bool  isRunning() const { return _currentSpeed > 0.1f; }

private:
    uint8_t  _pin        = 0;
    uint8_t  _channel    = 0;       // LEDC channel (motor = 0)
    bool     _initialized = false;
    uint8_t  _driver     = 0;       // 0=MOS PWM 1=L298N 开关
    bool     _l298On     = false;   // L298N 当前输出态
    uint16_t _freq       = 1000;
    uint8_t  _resBits    = 10;
    uint32_t _maxDuty    = 1023;
    float    _targetSpeed   = 0;
    float    _currentSpeed  = 0;
    uint32_t _lastUpdateMs  = 0;
    uint16_t _softStartMs   = 1000;
};

extern Motor g_motor;

#endif // MOTOR_H
