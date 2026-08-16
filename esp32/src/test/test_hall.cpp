#include "test_common.h"

// 累计计数
static int totalSpool = 0;
// RPM 统计（与固件 doRunning 相同的固定窗口法）
static const int MAGNETS = 8;          // 料盘磁铁数，与 config.h 一致
static volatile uint32_t winPulses = 0;
static uint32_t winStartMs = 0;
static float smoothRpm = 0;

static void addWinPulses(int n) { winPulses += n; }

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n======== 霍尔传感器测试（料盘 GPIO27）========");
    Serial.println("脉冲即时打印；每秒打印一次转速（圈/分）");

    pinMode(PIN_HALL_SPOOL, INPUT_PULLUP);
    attachInterrupt(PIN_HALL_SPOOL, isrSpool, FALLING);

    winStartMs = millis();
    log("检测到脉冲即打印，按 RST 退出\n");
}

void loop() {
    if (spoolCount != 0) {
        noInterrupts();
        int n = spoolCount;
        spoolCount = 0;
        interrupts();
        totalSpool += n;
        addWinPulses(n);
        Serial.printf("  [霍尔] +%d  累计=%d\n", n, totalSpool);
        ledBlink(50);
    }

    // 1 秒窗口计算 RPM（EMA 平滑，停转快速归零）
    uint32_t now = millis();
    if (now - winStartMs >= 1000) {
        noInterrupts();
        uint32_t p = winPulses;
        winPulses = 0;
        interrupts();
        float winRpm = (p / (float)MAGNETS) / ((now - winStartMs) / 1000.0f) * 60.0f;
        winStartMs = now;
        smoothRpm = (smoothRpm == 0) ? winRpm
                   : (winRpm < 0.5f) ? smoothRpm * 0.3f   // 停转快速衰减
                                      : smoothRpm * 0.6f + winRpm * 0.4f;
        if (smoothRpm < 0.05f) smoothRpm = 0;
        Serial.printf("  [转速] %.1f RPM  (窗口脉冲=%u, 累计圈数=%.2f)\n",
                      smoothRpm, p, totalSpool / (float)MAGNETS);
    }
}
