#include "encoder.h"
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

TraverseEncoder g_encoder;

static const uint8_t AS5600_ADDR = 0x36;

bool TraverseEncoder::begin(uint8_t sda, uint8_t scl) {
    if (_begun) return _ok;
    _sda = sda; _scl = scl;
    Wire.begin(sda, scl);
    // 100kHz: 现场实测 400kHz 在舵机/电机 EMI 下会总线锁死（连续 NACK +
    // 垃圾数据），慢边沿对噪声容忍度高得多。排线场景 1kHz 轮询绰绰有余
    Wire.setClock(100000);
    // 试读一次验证在位
    uint16_t raw = readRawAngle();
    _ok = (raw != 0xFFFF);
    _lastRaw = (raw == 0xFFFF) ? 0 : raw;
    _begun = true;
    Serial.printf("[Encoder] AS5600 初始化: %s (raw=%u)\n",
                  _ok ? "在位" : "无响应", _lastRaw);

    // 独立 1kHz 轮询任务: 跑在 core 1（core 0 是 WiFi 协议栈，事件忙起来
    // 会把轮询饿出 20ms+ 的洞，跨圈解包就错了）
    xTaskCreatePinnedToCore(pollTask, "enc_poll", 2048, this, 3, nullptr, 1);
    return _ok;
}

// I2C 总线恢复: 从机被 EMI 打挂卡死 SDA 后，唯一办法是发 9+ 个 SCL 脉冲
// 让从机吐出残余位，然后重新初始化总线
static void busRecover(uint8_t sda, uint8_t scl) {
    Wire.end();
    pinMode(scl, OUTPUT_OPEN_DRAIN);
    pinMode(sda, INPUT_PULLUP);
    for (int i = 0; i < 16; i++) {
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
    }
    Wire.begin(sda, scl);
    Wire.setClock(100000);
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
    uint32_t prevPollMs = 0;
    uint16_t errStreak = 0;
    while (true) {
        uint32_t now = millis();
        if (lastPollMs != 0) {
            uint32_t iv = now - lastPollMs;
            if (iv > self->_maxPollMs) self->_maxPollMs = iv;
        }
        prevPollMs = lastPollMs;
        lastPollMs = now;

        // 连续失败 → 总线锁死，脉冲恢复后重试
        if (errStreak >= 20) {
            busRecover(self->_sda, self->_scl);
            errStreak = 0;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        uint16_t raw = self->readRawAngle();
        if (raw == 0xFFFF) {
            self->_ok = false;
            errStreak++;
        } else {
            self->_ok = true;
            errStreak = 0;
            int32_t delta = (int32_t)raw - (int32_t)self->_lastRaw;
            if (delta > 2048)  { delta -= 4096; self->_unwrapCorr++; }
            if (delta < -2048) { delta += 4096; self->_unwrapCorr++; }
            // 垃圾读数防护: 正常轮询间隔(≤5ms)下单拍增量不可能超过半圈的 1/4，
            // 超过必是 EMI 破坏的数据——丢弃，不累计（累计了就是 ±22mm 跳变）
            uint32_t gap = now - prevPollMs;
            if (abs(delta) > 512 && gap <= 5) {
                self->_lastRaw = raw;   // 角度本身可信，仅不累计本拍增量
            } else {
                self->_totalCounts += delta;
                self->_lastRaw = raw;
            }

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
