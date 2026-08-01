#include "slip.h"

SlipDetector g_slip;

void SlipDetector::reset() {
    _lengthMeasured    = 0;
    _lengthTheoretical = 0;
    _effectiveDiameter = g_config.effectiveCoreDiameter();
    _spoolTurnsTotal   = 0;
    _spoolPulseAccum   = 0;
    _currentLayer      = 0;
    _lastSpoolActivityS = 0;
    _lastIdlerActivityS = 0;
}

void SlipDetector::setParams(float tolerance, float stallTimeoutS) {
    _tolerance     = tolerance;
    _stallTimeoutS = stallTimeoutS;
}

void SlipDetector::_recalcLayer() {
    // 每层圈数 = 料盘宽度 / 线径
    float turnsPerLayer = g_config.spoolWidth / g_config.filamentDiameter;
    if (turnsPerLayer < 1) turnsPerLayer = 1;

    // 料盘累计圈数 = 累计脉冲 / 料盘磁铁数
    float totalTurns = (float)_spoolPulseAccum / g_config.hallSpoolMagnets;

    uint16_t newLayer = (uint16_t)(totalTurns / turnsPerLayer);
    if (newLayer != _currentLayer) {
        _currentLayer = newLayer;
        _effectiveDiameter = g_config.effectiveCoreDiameter()
                           + 2.0f * g_config.filamentDiameter * _currentLayer;
    }
}

void SlipDetector::onSpoolPulses(uint32_t spoolNewPulses) {
    if (spoolNewPulses == 0) return;
    _lastSpoolActivityS = 0;  // 有活动，重置计时

    _spoolPulseAccum += spoolNewPulses;

    // 增量计算：每个料盘脉冲对应 1/MAGNETS 圈
    float turnsInc = (float)spoolNewPulses / g_config.hallSpoolMagnets;

    // 更新有效直径（可能进入新层）
    _recalcLayer();

    // 理论应收长度增量 = turnsInc * pi * effectiveDiameter (mm -> m)
    float lenInc = turnsInc * PI * _effectiveDiameter / 1000.0f;
    _lengthTheoretical += lenInc;
}

void SlipDetector::onIdlerPulses(uint32_t idlerNewPulses) {
    if (idlerNewPulses == 0) return;
    _lastIdlerActivityS = 0;

    // 实测出线长度 = (脉冲 / 磁铁数) * pi * 从动轮直径 (mm -> m)
    float turnsInc = (float)idlerNewPulses / g_config.hallIdlerMagnets;
    float lenInc = turnsInc * PI * g_config.idlerDiameter / 1000.0f;
    _lengthMeasured += lenInc;
}

ErrorCode SlipDetector::check(float dtSec) {
    // 更新不活动计时器
    _lastSpoolActivityS += dtSec;
    _lastIdlerActivityS += dtSec;

    // 需要足够的数据才能判定（理论长度 > 0.1m 才开始比较）
    if (_lengthTheoretical > 0.1f) {
        float diff = fabsf(_lengthTheoretical - _lengthMeasured);
        float devPct = diff / _lengthTheoretical * 100.0f;
        if (devPct > _tolerance) {
            return ERR_SLIP;
        }
    }

    // 卡线检测：料盘在转但从动轮长时间没脉冲
    if (_lastIdlerActivityS > _stallTimeoutS && _lastSpoolActivityS < _stallTimeoutS) {
        // 料盘近期有活动但从动轮超时无活动
        return ERR_STALL;
    }

    // 断线检测：从动轮有脉冲但料盘长时间没脉冲
    if (_lastSpoolActivityS > _stallTimeoutS && _lastIdlerActivityS < _stallTimeoutS) {
        return ERR_BREAK;
    }

    return ERR_NONE;
}
