#include "slip.h"

SlipDetector g_slip;

void SlipDetector::reset() {
    _lengthTheoretical = 0;
    _effectiveDiameter = g_config.effectiveCoreDiameter();
    _spoolPulseAccum   = 0;
    _currentLayer      = 0;
}

void SlipDetector::_recalcLayer() {
    float turnsPerLayer = g_config.spoolWidth / g_config.filamentDiameter;
    if (turnsPerLayer < 1) turnsPerLayer = 1;

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

    _spoolPulseAccum += spoolNewPulses;

    float turnsInc = (float)spoolNewPulses / g_config.hallSpoolMagnets;

    _recalcLayer();

    // 长度增量 = turnsInc * pi * effectiveDiameter (mm -> m)
    float lenInc = turnsInc * PI * _effectiveDiameter / 1000.0f;
    _lengthTheoretical += lenInc;
}
