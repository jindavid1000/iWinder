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

static int32_t totalCounts = 0;
static uint16_t lastRaw = 0;
static bool ok = false;

// 阶段: 0=右行 1=停 2=左行 3=停
static uint8_t phase = 2;
static uint32_t phaseStartMs = 0;

static float revsAtLastLeft = 0;    // 上次到左限位时的圈数
static bool   leftSeen = false;

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
    Serial.println("舵机将满速左右扫描（限位自动换向），观察打印。RST 退出\n");
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
            if (endR || now - phaseStartMs > 4000) { servoPulse(PULSE_STOP); phase = 1; phaseStartMs = now; }
            else servoPulse(PULSE_RIGHT);
            break;
        case 1:  // 停
            if (now - phaseStartMs > 800) { phase = 2; phaseStartMs = now; }
            break;
        case 2:  // 左行
            if (endL || now - phaseStartMs > 4000) {
                servoPulse(PULSE_STOP);
                phase = 3;
                phaseStartMs = now;
                if (leftSeen) {
                    float revs = totalCounts / 4096.0f - revsAtLastLeft;
                    Serial.printf(">> 本次左限位到右限位: %.3f 圈 × %.1fmm = %.1fmm (对比限位间距设定值)\n",
                                  fabsf(revs), MM_PER_REV, fabsf(revs) * MM_PER_REV);
                }
                revsAtLastLeft = totalCounts / 4096.0f;
                leftSeen = true;
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
        Serial.printf("[ENC] %s %-4s raw=%4u 圈数=%8.3f 位置=%8.2fmm 速度=%7.2fmm/s%s%s\n",
                      ok ? "OK" : "!!", ph, lastRaw,
                      totalCounts / 4096.0f, mm, speed,
                      endL ? " [左限位]" : "", endR ? " [右限位]" : "");
    }
    delay(2);
}
