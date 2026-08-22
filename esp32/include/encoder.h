#ifndef ENCODER_H
#define ENCODER_H
//============================================================================
//  encoder.h — AS5600 磁编码器（排线闭环位置反馈，直测丝杆）
//  12bit 绝对角度 × 多圈累计 → 丝杆位置 mm。
//  轮询跑在独立 1kHz FreeRTOS 任务上，主循环卡顿(delay/网络)不影响计数。
//============================================================================
#include <Arduino.h>

class TraverseEncoder {
public:
    // 初始化 I2C 并启动 1kHz 轮询任务。addr 固定 0x36（AS5600）
    bool begin(uint8_t sda, uint8_t scl);

    // 丝杆位置 (mm) = 累计圈数 × mmPerEncoderRev × 方向符号
    float posMm() const;

    // 累计编码器圈数（含方向符号，限位间比例标定用）
    float getRevs() const;

    // 设置当前位置为 pos（不改硬件，只改软件偏移）
    void setPosMm(float pos);

    // 方向符号: +1 = 编码器计数增加对应排线右行；-1 = 反装。
    // 寻原点时自动学习，无需手动配置。
    void setSign(int8_t s) { _sign = (s < 0) ? -1 : 1; }
    int8_t getSign() const { return _sign; }

    // 每编码器圈的丝杆位移 mm（标定写入；默认 = 丝杆导程）
    void setMmPerRev(float mmPerRev);
    float getMmPerRev() const { return _mmPerCount * 4096.0f; }

    // 实测丝杆速度 (mm/s，右行为正)
    float getSpeedMmPerS() const;

    // I2C 最近一次读取是否成功
    bool ok() const { return _ok; }

    // 诊断: 跨圈修正次数 / 最大轮询间隔（应为个位数 ms）
    uint16_t unwrapCorrections() const { return _unwrapCorr; }
    uint32_t maxPollMs() const { return _maxPollMs; }

private:
    static void pollTask(void *arg);   // 1kHz 轮询任务入口
    uint16_t readRawAngle();           // 0..4095, 失败返回 0xFFFF

    bool     _begun    = false;
    volatile bool _ok  = false;
    volatile int32_t _totalCounts = 0; // 多圈累计（含圈间进位）
    uint16_t _lastRaw  = 0;
    float    _mmPerCount = 0.016f;     // mm / count（默认 66mm/4096）
    int8_t   _sign      = 1;           // 方向符号（寻原点自动学习）
    uint8_t  _sda = 0, _scl = 0;       // 引脚（总线恢复用）

    // 诊断
    volatile uint16_t _unwrapCorr = 0;
    volatile uint32_t _maxPollMs  = 0;

    // 速度测量（任务侧更新）
    volatile float   _speedMmPerS = 0;
    float    _spdAccCounts = 0;
    uint32_t _spdWinMs     = 0;
};

extern TraverseEncoder g_encoder;

#endif // ENCODER_H
