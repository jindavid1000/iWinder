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
    void moveLeft();           // 向 Endstop 方向
    void moveRight();          // 远离 Endstop 方向
    void moveHome();           // 慢速左行（寻原点/校准）
    void stop();               // 停止

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
    uint8_t    _channel    = 1;     // LEDC channel (servo = 1)
    uint16_t   _freq       = 50;
    uint8_t    _resBits    = 16;
    uint32_t   _maxDuty    = 65535;

    uint16_t   _pulseStop  = 1500;
    uint16_t   _pulseLeft  = 1000;
    uint16_t   _pulseRight = 2000;
    uint16_t   _pulseHome  = 1300;

    float      _speedLeft  = 0;  // mm/s
    float      _speedRight = 0;  // mm/s

    TraverseDir _direction = DIR_NONE;
    float       _position  = 0;  // mm，相对于 Endstop 原点

    void writePulse(uint16_t pulseUs);
};

extern ServoCtl g_servo;

#endif // SERVO_H
