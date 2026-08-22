#include "encoder.h"
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

TraverseEncoder g_encoder;

static const uint8_t AS5600_ADDR = 0x36;

bool TraverseEncoder::begin(uint8_t sda, uint8_t scl) {
    if (_begun) return _ok;
    Wire.begin(sda, scl);
    Wire.setClock(400000);
    // 试读一次验证在位
    uint16_t raw = readRawAngle();
    _ok = (raw != 0xFFFF);
    _lastRaw = (raw == 0xFFFF) ? 0 : raw;
    _begun = true;
    Serial.printf("[Encoder] AS5600 初始化: %s (raw=%u)\n",
                  _ok ? "在位" : "无响应", _lastRaw);

    // 独立 1kHz 轮询任务: 每次间隔 1ms、增量约 10 计数，
    // 跨圈判定永远安全（主循环的 delay/网络阻塞不再影响计数）
    xTaskCreatePinnedToCore(pollTask, "enc_poll", 2048, this, 3, nullptr, 0);
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

void TraverseEncoder::pollTask(void *arg) {
    TraverseEncoder *self = static_cast<TraverseEncoder *>(arg);
    uint32_t lastPollMs = 0;
    while (true) {
        uint32_t now = millis();
        if (lastPollMs != 0) {
            uint32_t iv = now - lastPollMs;
            if (iv > self->_maxPollMs) self->_maxPollMs = iv;
        }
        lastPollMs = now;

        uint16_t raw = self->readRawAngle();
        if (raw == 0xFFFF) {
            self->_ok = false;
        } else {
            self->_ok = true;
            int32_t delta = (int32_t)raw - (int32_t)self->_lastRaw;
            if (delta > 2048)  { delta -= 4096; self->_unwrapCorr++; }
            if (delta < -2048) { delta += 4096; self->_unwrapCorr++; }
            self->_totalCounts += delta;
            self->_lastRaw = raw;

            // 速度测量: ≥20ms 窗口 → EMA
            self->_spdAccCounts += delta;
            uint32_t win = now - self->_spdWinMs;
            if (win >= 20) {
                float v = (self->_spdAccCounts / 4096.0f) * self->getMmPerRev()
                        * 1000.0f / (float)win * self->_sign;
                self->_speedMmPerS = self->_speedMmPerS * 0.6f + v * 0.4f;
                self->_spdAccCounts = 0;
                self->_spdWinMs = now;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));   // 1kHz
    }
}

float TraverseEncoder::posMm() const {
    return (float)_totalCounts * _mmPerCount * _sign;
}

float TraverseEncoder::getRevs() const {
    return ((float)_totalCounts / 4096.0f) * _sign;
}

float TraverseEncoder::getSpeedMmPerS() const {
    return _speedMmPerS;
}

void TraverseEncoder::setPosMm(float pos) {
    _totalCounts = (int32_t)(pos / (_mmPerCount * _sign));
}

void TraverseEncoder::setMmPerRev(float mmPerRev) {
    if (mmPerRev < 0.1f || mmPerRev > 500.0f) return;
    float mm = posMm();            // 保持当前位置不变
    _mmPerCount = mmPerRev / 4096.0f;
    _totalCounts = (int32_t)(mm / (_mmPerCount * _sign));
}
