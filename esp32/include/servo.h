#ifndef SERVO_H
#define SERVO_H
//============================================================================
//  servo.h — 排线舵机驱动（连续旋转舵机 + 丝杆）
//============================================================================
#include <Arduino.h>
#include "state.h"

class ServoCtl {
public:
    void begin(uint8_t pin, uint16_t freq, uint8_t resBits);
    void reattach(uint8_t pin);

    // 方向控制
    void moveLeft();           // 向 Endstop 方向（物理左移）
    void moveRight();          // 远离 Endstop 方向（物理右移）
    void moveHome();           // 慢速左行（寻原点/校准，仅用于舵机标定）
    void stop();               // 停止

    // 按比例调速（0.0~1.0，用于 RPM 同步绕线）
    void setSpeedFraction(float frac);

    // 连续有符号驱动（闭环速度控制用）: pct -100(满速左)~+100(满速右)，0=停止。
    // 直接以停止脉宽为基准按比例偏移，同时维护方向状态供限位保护判断
    void drive(int16_t pct);

    // 位置估算（在主循环中调用，dt 单位 ms）
    void updatePosition(uint32_t dtMs);

    // 位置管理
    float getPosition() const { return _position; }
    void  setPosition(float pos) { _position = pos; }
    void  resetPosition() { _position = 0; }

    TraverseDir getDirection() const { return _direction; }

    // 更新运行时参数（APP 修改后调用）
    void setPulses(uint16_t stop, uint16_t left, uint16_t right, uint16_t home);
    void setSpeeds(float leftMmS, float rightMmS);

private:
    uint8_t    _pin        = 0;
    uint8_t    _channel    = 2;     // LEDC channel (servo = 2, timer1 — 避免与电机 ch0 共享 timer0 导致频率/分辨率互相覆盖)
    bool       _initialized = false;
    uint16_t   _freq       = 50;
    uint8_t    _resBits    = 16;
    uint32_t   _maxDuty    = 65535;

    uint16_t   _pulseStop  = 1500;
    uint16_t   _pulseLeft  = 500;
    uint16_t   _pulseRight = 2500;
    uint16_t   _pulseHome  = 500;
    uint16_t   _pulseMin   = 500;
    uint16_t   _pulseMax   = 2500;

    float      _speedLeft  = 0;  // mm/s
    float      _speedRight = 0;  // mm/s

    TraverseDir _direction = DIR_NONE;
    float       _position  = 0;  // mm，相对于 Endstop 原点
    float       _speedFraction = 1.0f;  // 满速比例（0.01~1.0）

    void writePulse(uint16_t pulseUs);
    void writeCurrentDirection();  // 按当前方向+比例输出 PWM
};

extern ServoCtl g_servo;

#endif // SERVO_H
