#ifndef SLIP_H
#define SLIP_H
//============================================================================
//  slip.h — 料盘计量器（圈数 → 长度 → 有效直径 → 层数）
//  原打滑检测已移除（从动轮霍尔取消），仅保留长度/直径/层计算。
//============================================================================
#include <Arduino.h>
#include "state.h"

class SlipDetector {
public:
    // 重置（每次启动绕线任务时调用）
    void reset();

    // 每次收到新的料盘脉冲时调用，增量计算长度
    void onSpoolPulses(uint32_t spoolNewPulses);

    // 获取计算结果
    float getLength() const { return _lengthTheoretical; }
    float getEffectiveDiameter() const { return _effectiveDiameter; }
    uint16_t getCurrentLayer() const { return _currentLayer; }

    // 保留旧名称做兼容（winder.cpp 内部调用）
    float getLengthTheoretical() const { return _lengthTheoretical; }

private:
    float _lengthTheoretical = 0;   // m
    float _effectiveDiameter = 0;   // mm
    uint32_t _spoolPulseAccum = 0;  // 料盘脉冲累计
    uint16_t _currentLayer   = 0;

    void _recalcLayer();
};

extern SlipDetector g_slip;

#endif // SLIP_H
