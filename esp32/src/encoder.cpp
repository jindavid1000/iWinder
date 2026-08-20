#include "encoder.h"
#include <Wire.h>

TraverseEncoder g_encoder;

static const uint8_t AS5600_ADDR = 0x36;

bool TraverseEncoder::begin(uint8_t sda, uint8_t scl) {
    if (_begun) return _begun;
    Wire.begin(sda, scl);
    Wire.setClock(400000);
    // 试读一次验证在位
    uint16_t raw = readRawAngle();
    _ok = (raw != 0xFFFF);
    _lastRaw = (raw == 0xFFFF) ? 0 : raw;
    _begun = true;
    Serial.printf("[Encoder] AS5600 初始化: %s (raw=%u)\n",
                  _ok ? "在位" : "无响应", _lastRaw);
    return _ok;
}

uint16_t TraverseEncoder::readRawAngle() {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(0x0C);              // 角度高字节寄存器
    if (Wire.endTransmission(false) != 0) return 0xFFFF;
    if (Wire.requestFrom((int)AS5600_ADDR, 2) != 2) return 0xFFFF;
    uint16_t h = Wire.read();
    uint16_t l = Wire.read();
    return ((h & 0x0F) << 8) | l;  // 12bit
}

void TraverseEncoder::poll() {
    if (!_begun) return;
    uint16_t raw = readRawAngle();
    if (raw == 0xFFFF) {           // 读取失败保持原值
        _ok = false;
        return;
    }
    _ok = true;
    int32_t delta = (int32_t)raw - (int32_t)_lastRaw;
    // 跨圈处理: ±2048 以内视为真实增量，超出视为回绕
    if (delta > 2048)  delta -= 4096;
    if (delta < -2048) delta += 4096;
    _totalCounts += delta;
    _lastRaw = raw;

    // 速度测量: ≥20ms 窗口累计位移 → EMA 平滑
    uint32_t now = millis();
    _spdAccMm += delta * _mmPerCount;
    uint32_t win = now - _spdWinMs;
    if (win >= 20) {
        float v = _spdAccMm * 1000.0f / (float)win;
        _speedMmPerS = _speedMmPerS * 0.6f + v * 0.4f;
        _spdAccMm = 0;
        _spdWinMs = now;
    }
}

void TraverseEncoder::setPosMm(float pos) {
    // 通过调整累计计数实现软件置位
    _totalCounts = (int32_t)(pos / _mmPerCount);
}

void TraverseEncoder::setMmPerRev(float mmPerRev) {
    if (mmPerRev < 0.1f || mmPerRev > 500.0f) return;
    float mm = posMm();            // 保持当前位置不变
    _mmPerCount = mmPerRev / 4096.0f;
    _totalCounts = (int32_t)(mm / _mmPerCount);
}
