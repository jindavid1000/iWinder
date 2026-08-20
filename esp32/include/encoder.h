#ifndef ENCODER_H
#define ENCODER_H
//============================================================================
//  encoder.h — AS5600 磁编码器（排线闭环位置反馈，装在舵机输出轴）
//  12bit 绝对角度 × 多圈累计 → 丝杆位置 mm。
//  编码器轴到丝杆有增速齿轮时由 encGearRatio / 标定比例换算。
//============================================================================
#include <Arduino.h>

class TraverseEncoder {
public:
    // 初始化 I2C。addr 固定 0x36（AS5600）
    bool begin(uint8_t sda, uint8_t scl);

    // 主循环轮询（≥50Hz 即可，内部做跨圈累计）
    void poll();

    // 丝杆位置 (mm) = 累计圈数 × mmPerEncoderRev
    float posMm() const { return _totalCounts * _mmPerCount; }

    // 累计编码器圈数（限位间比例标定用）
    float getRevs() const { return _totalCounts / 4096.0f; }

    // 实测丝杆速度 (mm/s，右行为正，20ms 窗口 + EMA 平滑)
    float getSpeedMmPerS() const { return _speedMmPerS; }

    // 设置当前位置为 pos（不改硬件，只改软件偏移）
    void setPosMm(float pos);

    // 每编码器圈的丝杆位移 mm（标定写入；默认 = 丝杆导程 × 齿比）
    void setMmPerRev(float mmPerRev);
    float getMmPerRev() const { return _mmPerCount * 4096.0f; }

    // I2C 最近一次读取是否成功
    bool ok() const { return _ok; }

private:
    uint16_t readRawAngle();          // 0..4095, 失败返回 0xFFFF
    bool     _begun    = false;
    bool     _ok       = false;
    int32_t  _totalCounts = 0;        // 多圈累计（含圈间进位）
    uint16_t _lastRaw  = 0;
    float    _mmPerCount = 0.016f;    // mm / count（默认 66mm/4096）

    // 速度测量
    float    _speedMmPerS = 0;
    float    _spdAccMm    = 0;        // 窗口内累计位移
    uint32_t _spdWinMs    = 0;        // 窗口起始时间
};

extern TraverseEncoder g_encoder;

#endif // ENCODER_H
