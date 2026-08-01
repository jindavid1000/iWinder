#ifndef SLIP_H
#define SLIP_H
//============================================================================
//  slip.h — 打滑检测（理论应收长度 vs 实测出线长度）
//============================================================================
#include <Arduino.h>
#include "state.h"

class SlipDetector {
public:
    // 重置（每次启动绕线任务时调用）
    void reset();

    // 每次收到新的料盘脉冲时调用，增量计算理论长度
    // spoolNewPulses = 自上次调用以来的新料盘脉冲数
    void onSpoolPulses(uint32_t spoolNewPulses);

    // 每次收到新的从动轮脉冲时调用，增量计算实测长度
    void onIdlerPulses(uint32_t idlerNewPulses);

    // 周期性检测（在主循环中调用）
    // dtSec: 距上次调用的秒数
    // 返回 ERR_NONE 或异常码
    ErrorCode check(float dtSec);

    // 获取计算结果
    float getLengthMeasured() const { return _lengthMeasured; }
    float getLengthTheoretical() const { return _lengthTheoretical; }
    float getEffectiveDiameter() const { return _effectiveDiameter; }
    uint16_t getCurrentLayer() const { return _currentLayer; }

    // 更新参数
    void setParams(float tolerance, float stallTimeoutS);

private:
    float _lengthMeasured   = 0;    // m
    float _lengthTheoretical = 0;   // m
    float _effectiveDiameter = 0;   // mm
    uint32_t _spoolTurnsTotal = 0;  // 料盘累计整圈（*1000 避免浮点）
    uint32_t _spoolPulseAccum = 0;  // 料盘脉冲累计（用于层计算）
    uint16_t _currentLayer   = 0;

    float _tolerance        = 10.0f;
    float _stallTimeoutS    = 3.0f;

    // 卡线检测：记录最后一次各传感器活动时间
    float _lastSpoolActivityS  = 0;
    float _lastIdlerActivityS  = 0;

    void _recalcLayer();
};

extern SlipDetector g_slip;

#endif // SLIP_H
