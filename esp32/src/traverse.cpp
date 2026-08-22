#include "traverse.h"
#include "servo.h"
#include "sensors.h"
#include "encoder.h"
#include "config.h"
#include "winder.h"

TraverseCtl g_traverse;

// ============================================================================
//  位置源
// ============================================================================

bool TraverseCtl::encoderMode() {
    return g_config.traverseEncoder == 1;
}

float TraverseCtl::pos() {
    // 编码器模式下始终用编码器: 瞬时 I2C 读错时计数只是暂停（冻结在最后
    // 已知位置），切到未标定的舵机估算反而会注入假位移导致乱冲
    if (encoderMode()) return g_encoder.posMm();
    return g_servo.getPosition();
}

void TraverseCtl::setPos(float p) {
    g_servo.setPosition(p);          // 估算源同步，保证可回退
    if (encoderMode()) g_encoder.setPosMm(p);
}

// ============================================================================
//  任务生命周期
// ============================================================================

void TraverseCtl::beginWinding() {
    _windTargetPos = g_config.traverseLeftStart;
    _windDirRight  = true;
    _windLastTurns = g_state.spoolTurns;
    _roundTrips    = 0;
    g_state.roundTrips = 0;
    _windGraceUntilMs = millis() + 10000;   // 定位收敛宽限
    _staticSinceMs = 0;
    _trackMag = 12;
}

void TraverseCtl::stop() {
    if (g_servo.getDirection() != DIR_NONE) {
        _travDir = g_servo.getDirection();
        g_servo.stop();
    }
}

void TraverseCtl::saveForCalib() {
    // 目标与方向由成员天然保存，无需额外快照
}

void TraverseCtl::restoreAfterCalib() {
    _windLastTurns = g_state.spoolTurns;   // 校准期间圈数不推进目标，重新对齐
    float L = g_config.traverseLeftStart, R = g_config.traverseRightEnd;
    if (_windTargetPos > R) _windTargetPos = R;
    if (_windTargetPos < L) _windTargetPos = L;
    _windGraceUntilMs = millis() + 15000;  // 从限位返回原位置的宽限
    _travDir = _windDirRight ? DIR_RIGHT : DIR_LEFT;
    _trackMag = 12;
}

// ============================================================================
//  圈数驱动的目标三角波
// ============================================================================

bool TraverseCtl::onSpoolTurns(float turns) {
    bool leftFold = false;
    _turnsAdvanced = (turns != _windLastTurns);
    if (!_turnsAdvanced) return false;

    float d = (turns - _windLastTurns) * g_config.filamentDiameter;
    _windLastTurns = turns;
    _windTargetPos += _windDirRight ? d : -d;

    float L = g_config.traverseLeftStart;
    float R = g_config.traverseRightEnd;
    if (_windTargetPos >= R) {
        _windTargetPos = R - (_windTargetPos - R);
        _windDirRight = false;
    } else if (_windTargetPos <= L) {
        _windTargetPos = L + (L - _windTargetPos);
        _windDirRight = true;
        _roundTrips++;
        g_state.roundTrips = _roundTrips;
        g_state.calibCountdown = (g_config.calIntervalRounds > _roundTrips)
                               ? (g_config.calIntervalRounds - _roundTrips) : 0;
        leftFold = true;
    }
    return leftFold;
}

// ============================================================================
//  闭环控制
// ============================================================================


// 限位稳定判定: 连续 ≥5ms 为低才认（过滤舵机电流瞬态打出的假脉冲）。
// update() 每拍 ≥5ms 调用一次，连续两拍为低即满足。
bool TraverseCtl::leftEndstopStable(uint32_t now) {
    if (g_sensors.isLeftEndstopPressed()) {
        if (_leftLowSinceMs == 0) { _leftLowSinceMs = now; return false; }
        return (now - _leftLowSinceMs) >= 5;
    }
    _leftLowSinceMs = 0;
    return false;
}

bool TraverseCtl::rightEndstopStable(uint32_t now) {
    if (g_sensors.isRightEndstopPressed()) {
        if (_rightLowSinceMs == 0) { _rightLowSinceMs = now; return false; }
        return (now - _rightLowSinceMs) >= 5;
    }
    _rightLowSinceMs = 0;
    return false;
}

void TraverseCtl::resumeMove(TraverseDir dir) {
    if (g_servo.getDirection() == dir) return;
    if (dir == DIR_LEFT) g_servo.moveLeft();
    else                 g_servo.moveRight();
}

void TraverseCtl::update(uint32_t now) {
    g_servo.updatePosition(5);   // 开环估算源（编码器模式下仅作回退，不参与控制）
    g_state.traversePos = pos();
    g_state.traverseDir = g_servo.getDirection();

    float posNow = pos();
    float err = _windTargetPos - posNow;

    // ---- 诊断 ----
    static uint32_t lastDbgMs = 0;
    if (now - lastDbgMs >= 1000) {
        lastDbgMs = now;
        Serial.printf("[TRA] pos=%.2f tgt=%.2f err=%.2f dir=%s turns=%.1f\n",
                      posNow, _windTargetPos, err,
                      g_servo.getDirection() == DIR_LEFT  ? "L" :
                      g_servo.getDirection() == DIR_RIGHT ? "R" : "-",
                      g_state.spoolTurns);
    }

    // ---- 静止异常保护 ----
    // 料盘静止超 3 秒仍未追上目标（偏差>2mm）→ 坐标链异常（方向/限位镜像等）。
    // 高速停手后目标领先数毫米属正常追赶，3 秒窗口避免误杀。
    if (!_turnsAdvanced && fabsf(err) > 2.0f && now > _windGraceUntilMs) {
        if (_staticSinceMs == 0) _staticSinceMs = now;
        if (now - _staticSinceMs > 3000) {
            stop();
            g_winder.reportFault("排线位置与目标偏差过大且料盘静止，疑似方向/限位接线镜像");
            return;
        }
    } else {
        _staticSinceMs = 0;
    }

    if (fabsf(err) < 0.15f) {
        // 已到位: 暂停等待目标推进（料盘慢/停时排线也停，位置不漂移）
        stop();
        // 静止时仍顶限位 → 舵机中位偏移（停止脉宽下仍在缓慢转动）
        if ((g_sensors.isLeftEndstopPressed() || g_sensors.isRightEndstopPressed())
            && now > _windGraceUntilMs) {   // 刚锚定/折返后顶限位属正常，不误报
            if (_stillPressMs == 0) _stillPressMs = now;
            else if (now - _stillPressMs > 2000) {
                stop();
                g_winder.reportFault("排线静止时仍顶限位: 舵机中位偏移，请微调「停止PWM」直至静止");
                return;
            }
        } else {
            _stillPressMs = 0;
        }
        endstopSafety(now);
        return;
    }

    TraverseDir wantDir = (err > 0) ? DIR_RIGHT : DIR_LEFT;
    _travDir = wantDir;

    if (encoderMode() && g_encoder.ok()) {
        // ===== 编码器闭环: 连续旋转 + 速度自适应匹配 =====
        // 舵机持续旋转（平滑），每 40ms 结算一次:
        //   应走速度 = 圈数前馈 + 误差回收（限幅）
        //   实测速度 = 编码器速度
        // 两者差多少，驱动幅度就微调多少——自动收敛到"刚好跟上目标"
        // 的持续转速。另带预判滑行刹车: 误差小于滑行距离就断驱动，
        // 让惯性把最后一段滑完，不冲过头。
        if (now - _piLastMs >= 40) {
            _piLastMs = now;

            float vMeas = g_encoder.getSpeedMmPerS();   // 实测速度（右正）
            bool coast = (fabsf(err) > 0.15f &&
                          fabsf(err) < fabsf(vMeas) * 0.15f + 0.5f);

            if (coast) {
                // 预判滑行刹车: 断驱后 ~0.15s 才真正停 → 滑行 ≈ v×0.15。
                // 误差小于滑行距离+0.5mm 时断驱，滑行恰好吃掉剩余误差
                g_servo.drive(0);
                _trackMag = 12;
            } else {
                float ff = (g_state.spoolRpm / 60.0f) * g_config.filamentDiameter;
                float windDir = _windDirRight ? 1.0f : -1.0f;
                float e = err;                    // 误差回收，限 ±5mm/s
                if (e >  5.0f) e =  5.0f;
                if (e < -5.0f) e = -5.0f;
                float desired = ff * windDir + e * 1.2f;

                // 绕线方向禁倒车（小误差时）: 反向要穿丝杆回差，且表现为
                // "走回头路"。超前就停等目标，误差 >2.5mm 才允许真倒车。
                if (fabsf(err) < 2.5f && desired * windDir < 0) desired = 0;

                if (fabsf(desired) < 0.3f) {
                    g_servo.drive(0);             // 无需求: 停
                    _trackMag = 12;               // 幅度重置回中
                } else {
                    int16_t dir = (desired > 0) ? 1 : -1;
                    float vAlong = vMeas * dir;   // 实速在目标方向的分量
                    float vErr = fabsf(desired) - vAlong;
                    int16_t step = (int16_t)(vErr * 2.0f);        // 速度差 → 幅度微调
                    if (step >  5) step =  5;
                    if (step < -5) step = -5;
                    _trackMag += step;
                    if (_trackMag < 6)  _trackMag = 6;  // 死区内由自适应往上爬
                    if (_trackMag > 30) _trackMag = 30;
                    g_servo.drive(dir * _trackMag);
                }
            }
        }
    } else {
        // ===== 开环估算兜底: 迟滞步进 =====
        // 连续旋转舵机低速近乎开关特性，比例调速不可行:
        // 误差 >1mm 以固定偏移走，收敛到 0.3mm 内停。
        if (fabsf(err) > 1.0f) {
            resumeMove(wantDir);
            g_servo.setSpeedFraction(SERVO_MIN_FRAC);
        } else if (fabsf(err) < 0.3f) {
            stop();
        }
    }

    endstopSafety(now);
}

// ============================================================================
//  运行中的限位安全保护
// ============================================================================

void TraverseCtl::endstopSafety(uint32_t now) {
    // 用"意图方向"而非舵机实际方向判断: PI 死区停车时舵机方向为 NONE，
    // 但目标仍在往右推（顶在限位上），此时也必须处理限位。
    bool pushRight = (g_servo.getDirection() == DIR_RIGHT ||
                      (g_servo.getDirection() == DIR_NONE && _travDir == DIR_RIGHT));
    bool pushLeft  = (g_servo.getDirection() == DIR_LEFT ||
                      (g_servo.getDirection() == DIR_NONE && _travDir == DIR_LEFT));

    if (pushRight && rightEndstopStable(now)) {
        // 位置合理性: 右限位锚定优先在排线确实接近右端时接受。
        // 但若限位被持续压住 ≥500ms（EMI 假脉冲不可能持续），
        // 即使坐标不符也必须接受——否则编码器漂移/参数偏差会让
        // 排线顶死在限位上被永久忽略（目标一直推右、永不换向）。
        uint32_t heldMs = now - _rightLowSinceMs;
        if (pos() < g_config.travelRangeMm - 3.0f && heldMs < 500) {
            static uint32_t lastIgnR = 0;
            if (now - lastIgnR > 1000) {   // 限流: 持续误压时每秒只提示一次
                lastIgnR = now;
                Serial.printf("[Winder] 忽略右限位瞬态（排线 %.1fmm 处，疑干扰）\n", pos());
            }
            return;
        }
        g_servo.stop();
        float drift = g_config.travelRangeMm - pos();
        if (drift > 3.0f) {
            Serial.printf("[Winder] 警告: 右限位触发时坐标偏差 %.1fmm，"
                          "请核查 travelRangeMm 实测值与编码器 mm/圈 标定\n", drift);
        }
        setPos(g_config.travelRangeMm);
        _windDirRight = false;
        if (_windTargetPos > g_config.traverseRightEnd) _windTargetPos = g_config.traverseRightEnd;
        _windGraceUntilMs = now + 10000;   // 位置校正后长距离回退的宽限
        Serial.printf("[Winder] 右限位触发换向 pos=%.1f\n", pos());
    } else if (pushLeft && leftEndstopStable(now)) {
        uint32_t heldMs = now - _leftLowSinceMs;
        if (pos() > 3.0f && heldMs < 500) {
            static uint32_t lastIgnL = 0;
            if (now - lastIgnL > 1000) {
                lastIgnL = now;
                Serial.printf("[Winder] 忽略左限位瞬态（排线 %.1fmm 处，疑干扰）\n", pos());
            }
            return;
        }
        g_servo.stop();
        if (pos() < -3.0f) {
            Serial.printf("[Winder] 警告: 左限位触发时坐标偏差 %.1fmm，"
                          "请核查 travelRangeMm 实测值与编码器 mm/圈 标定\n", pos());
        }
        setPos(0);
        _windDirRight = true;
        if (_windTargetPos < g_config.traverseLeftStart) _windTargetPos = g_config.traverseLeftStart;
        _windGraceUntilMs = now + 10000;
        Serial.printf("[Winder] 左限位触发换向 pos=%.1f\n", pos());
    }
}
