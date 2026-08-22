#include <Arduino.h>
#include <Wire.h>

// ============================================================================
//  AS5600 编码器测试（SDA=GPIO21 SCL=GPIO22，舵机自动来回扫描）
//  舵机满速右行 → 右限位停 → 左行 → 左限位停 → 循环，
//  每 200ms 打印: 原始角度 / 圈数 / 位置 mm / 实测速度 / 阶段
//  验证点:
//   1. 运动期间 速度 ≈ ±57mm/s（与舵机标定值一致）
//   2. 一个来回 位置 总变化 ≈ 限位间距（≈80mm），且左右端数值重复稳定
//   3. 限位间 圈数 差 × 22mm ≈ 限位间距 → 验证编码器比例
//   4. 停止阶段 速度 归零、位置 不漂移
// ============================================================================

static const uint8_t  AS5600_ADDR = 0x36;
static const uint8_t  PIN_END_L   = 32;    // 左限位（低=触发）
static const uint8_t  PIN_END_R   = 14;    // 右限位（低=触发）
static const float    MM_PER_REV  = 22.0f; // 丝杆导程（编码器直测丝杆）

// --- 舵机 PWM（与主固件相同: LEDC ch2, 50Hz, 16bit）---
static const uint8_t  SERVO_PIN   = 5;
static const uint8_t  SERVO_CH    = 2;
static const uint16_t PULSE_STOP  = 1500;
static const uint16_t PULSE_LEFT  = 500;
static const uint16_t PULSE_RIGHT = 2500;

static void servoPulse(uint16_t us) {
    uint32_t maxDuty = (1 << 16) - 1;
    ledcWrite(SERVO_CH, (uint32_t)us * maxDuty / 20000);
}

static uint16_t readRaw() {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(0x0C);
    if (Wire.endTransmission(false) != 0) return 0xFFFF;
    if (Wire.requestFrom((int)AS5600_ADDR, 2) != 2) return 0xFFFF;
    uint16_t h = Wire.read();
    uint16_t l = Wire.read();
    return ((h & 0x0F) << 8) | l;
}

// --- AS5600 磁场诊断寄存器 ---
static uint8_t readReg8(uint8_t reg) {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0xFF;
    if (Wire.requestFrom((int)AS5600_ADDR, 1) != 1) return 0xFF;
    return Wire.read();
}

static uint16_t readReg16(uint8_t reg) {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0xFFFF;
    if (Wire.requestFrom((int)AS5600_ADDR, 2) != 2) return 0xFFFF;
    uint16_t h = Wire.read();
    uint16_t l = Wire.read();
    return (h << 8) | l;
}

// 打印磁场质量诊断:
//  STATUS(0x0B): MD=磁铁检测 ML=场太弱 MH=场太强（位2/3/4，1=触发）
//  AGC(0x1A):   自动增益 0-255。0=场过强饱和, 255=场过弱
//  MAG(0x1B):   场强幅值 0-4095，正常安装约 1000~3000
static void printMagDiag() {
    uint8_t  status = readReg8(0x0B);
    uint8_t  agc    = readReg8(0x1A);
    uint16_t mag    = readReg16(0x1B) & 0x0FFF;
    bool md = status & 0x04, ml = status & 0x08, mh = status & 0x10;
    const char *verdict;
    if (!md)          verdict = "未检测到磁铁!";
    else if (ml || agc >= 255) verdict = "磁场过弱: 磁铁太远/太小";
    else if (mh || agc == 0)   verdict = "磁场过强: 磁铁太近";
    else              verdict = "磁场强度正常";
    Serial.printf("[MAG] %s | STATUS:%s%s%s AGC=%u 场强=%u\n",
                  verdict,
                  md ? "MD " : "", ml ? "ML " : "", mh ? "MH " : "-",
                  agc, mag);
}

static int32_t totalCounts = 0;
static uint16_t lastRaw = 0;
static bool ok = false;

// 阶段: 0=右行 1=停 2=左行 3=停
static uint8_t phase = 2;
static uint32_t phaseStartMs = 0;

static float revsAtRight = 0;      // 右限位停稳后的圈数（比例标定基准）
static bool   rightSeen = false;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n======== AS5600 编码器测试（舵机自动扫描）========");

    pinMode(PIN_END_L, INPUT_PULLUP);
    pinMode(PIN_END_R, INPUT_PULLUP);

    ledcSetup(SERVO_CH, 50, 16);
    ledcAttachPin(SERVO_PIN, SERVO_CH);
    servoPulse(PULSE_STOP);

    Wire.begin(21, 22);
    Wire.setClock(400000);

    uint16_t raw = readRaw();
    ok = (raw != 0xFFFF);
    lastRaw = (raw == 0xFFFF) ? 0 : raw;
    Serial.printf("编码器初始化: %s raw=%u\n", ok ? "在位" : "无响应(检查接线/磁铁)", lastRaw);
    printMagDiag();   // 磁场质量: 距离/强度是否正常
    Serial.println("舵机将满速左右扫描（限位自动换向），观察打印。RST 退出\n");
}

// ============================================================================
//  匀速偏心检测: 右行匀速段里测每 1/8 圈的耗时。
//  磁铁偏心 → 每圈的角速度读数呈正弦波动（一圈一个周期）:
//  8 个扇区耗时出现固定"一快一慢"且位置稳定 = 偏心; 随机波动 = 电机/机械抖动。
// ============================================================================
static uint32_t secUs[8]  = {0};
static uint16_t secN[8]   = {0};
static int16_t  lastSec   = -1;
static uint32_t secUsLast = 0;

static void sectorRippleSample(uint16_t raw, uint32_t nowUs) {
    int16_t sec = raw >> 9;                    // 8 扇区/圈
    if (lastSec < 0) { lastSec = sec; secUsLast = nowUs; return; }
    int16_t d = (int16_t)((sec - lastSec) & 7);
    if (d != 1) {                              // 只统计稳定前进的单步
        lastSec = sec; secUsLast = nowUs; return;
    }
    uint32_t dt = nowUs - secUsLast;
    lastSec = sec; secUsLast = nowUs;
    secUs[sec] += dt;
    if (secN[sec] < 999) secN[sec]++;

    if (secN[0] >= 15) {                       // 累计 ~15 圈后出报告
        uint32_t avgSum = 0;
        for (int i = 0; i < 8; i++) avgSum += secUs[i] / secN[i];
        uint32_t avg = avgSum / 8;
        uint32_t mx = 0, mn = 0xFFFFFFFF;
        int mxi = 0, mni = 0;
        for (int i = 0; i < 8; i++) {
            uint32_t v = secUs[i] / secN[i];
            if (v > mx) { mx = v; mxi = i; }
            if (v < mn) { mn = v; mni = i; }
        }
        float ripple = 100.0f * (mx - mn) / avg;
        Serial.printf("[RIP] 扇区耗时(ms/⅛圈):");
        for (int i = 0; i < 8; i++) Serial.printf(" %4lu", (unsigned long)(secUs[i] / secN[i] / 1000));
        Serial.printf("  | 波动=%.1f%%", ripple);
        if (ripple < 8.0f)      Serial.println(" → 无明显偏心");
        else if (ripple < 20.0f) Serial.printf(" → 轻微偏心(最慢扇区%d 最快扇区%d, 相差约%d°)\n",
                                               mni, mxi, (mni - mxi) * 45);
        else                     Serial.printf(" → 明显偏心! 最慢扇区%d 最快扇区%d(相差%d°), 位置精度受损\n",
                                               mni, mxi, (mni - mxi) * 45);
        for (int i = 0; i < 8; i++) { secUs[i] = 0; secN[i] = 0; }
    }
}

void loop() {
    // --- 编码器轮询 ---
    uint16_t raw = readRaw();
    if (raw != 0xFFFF) {
        ok = true;
        int32_t delta = (int32_t)raw - (int32_t)lastRaw;
        if (delta > 2048)  delta -= 4096;
        if (delta < -2048) delta += 4096;
        totalCounts += delta;
        lastRaw = raw;
    } else {
        ok = false;
    }

    // --- 阶段机 ---
    bool endL = (digitalRead(PIN_END_L) == LOW);
    bool endR = (digitalRead(PIN_END_R) == LOW);
    uint32_t now = millis();

    switch (phase) {
        case 0:  // 右行
            if (endR || now - phaseStartMs > 4000) { servoPulse(PULSE_STOP); phase = 1; phaseStartMs = now; lastSec = -1; }
            else {
                servoPulse(PULSE_RIGHT);
                sectorRippleSample(raw, micros());   // 匀速段采样偏心检测
            }
            break;
        case 1:  // 停（右限位停稳后记录基准圈数）
            if (now - phaseStartMs > 800) {
                revsAtRight = totalCounts / 4096.0f;   // 停稳后记录，避开撞限位回弹
                rightSeen = true;
                phase = 2; phaseStartMs = now;
            }
            break;
        case 2:  // 左行
            if (endL || now - phaseStartMs > 4000) {
                servoPulse(PULSE_STOP);
                phase = 3;
                phaseStartMs = now;
            }
            else servoPulse(PULSE_LEFT);
            break;
        case 3:  // 停
            if (now - phaseStartMs > 800) { phase = 0; phaseStartMs = now; }
            break;
    }

    // --- 打印 ---
    static uint32_t lastPrintMs = 0;
    static float lastMm = 0;
    if (now - lastPrintMs >= 200) {
        float dt = (now - lastPrintMs) / 1000.0f;
        float mm = totalCounts / 4096.0f * MM_PER_REV;
        float speed = (mm - lastMm) / dt;
        lastMm = mm;
        lastPrintMs = now;
        const char *ph = (phase == 0) ? "右行" : (phase == 1) ? "停止"
                        : (phase == 2) ? "左行" : "停止";
        // 左限位停稳(进入阶段3)时打印: 右停稳 → 左触发 的真实行程
        static uint8_t lastPhase = 3;
        if (phase == 3 && lastPhase == 2 && rightSeen) {
            float revs = fabsf(totalCounts / 4096.0f - revsAtRight);
            Serial.printf(">> 右停稳→左触发: %.3f 圈 × %.1fmm = %.1fmm（填入「限位间距」）\n",
                          revs, MM_PER_REV, revs * MM_PER_REV);
        }
        lastPhase = phase;
        Serial.printf("[ENC] %s %-4s raw=%4u 圈数=%8.3f 位置=%8.2fmm 速度=%7.2fmm/s%s%s\n",
                      ok ? "OK" : "!!", ph, lastRaw,
                      totalCounts / 4096.0f, mm, speed,
                      endL ? " [左限位]" : "", endR ? " [右限位]" : "");
    }
    delay(2);
}
