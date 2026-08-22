#ifndef CALIB_SERVO_H
#define CALIB_SERVO_H
//============================================================================
//  calib_servo.h — 舵机速度/编码器比例标定（独立状态机）
//  流程: 归位左限位 → 满速 3 个来回 → 步进速度(30%) 1 个来回。
//  产出: 左/右行满速速度、编码器每圈位移（编码器模式）。
//============================================================================
#include <Arduino.h>

class ServoCalibrator {
public:
    // 开始标定（距离 = 限位间距）。返回 false 表示前置条件不满足。
    bool start();
    void update();          // 每拍驱动
    bool active() const { return _phase != PHASE_IDLE; }

private:
    enum Phase : uint8_t {
        PHASE_IDLE,
        PHASE_HOME,         // 归位左限位
        PHASE_GO_RIGHT,     // 满速右行计时
        PHASE_GO_LEFT,      // 满速左行计时
        PHASE_SLOW_RIGHT,   // 步进速度右行
        PHASE_SLOW_LEFT,    // 步进速度左行
        PHASE_DONE
    };
    Phase     _phase = PHASE_IDLE;
    uint8_t   _round = 0;
    uint32_t  _moveStartMs = 0;
    uint32_t  _timeoutMs   = 0;
    float     _dist = 0;
    float     _sumRight = 0, _sumLeft = 0;
    float     _slowRight = 0, _slowLeft = 0;
    float     _encRevsAtLeft = 0;
    float     _passStartRevs = 0;   // 单趟起点圈数（速度=实测圈数×每圈位移÷时间）
};

extern ServoCalibrator g_scalib;

#endif // CALIB_SERVO_H
